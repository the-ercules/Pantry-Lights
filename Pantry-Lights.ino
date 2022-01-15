#include "led_control.h" //Custom
#include <MemoryFree.h> //https://github.com/maniacbug/MemoryFree
#include <ezButton.h> //https://github.com/ArduinoGetStarted/button
#include <EEPROM.h> //Included

const String REVISION_NUMBER = "0.2.2";

//Debugging
const boolean DEBUGGING = true;
const int memoryInterval = 5000; //milliseconds
unsigned long outputMemory = 0; //timestamp in milliseconds


const byte PWM_OUT = 9;
const byte LED_A = 7;
const byte LED_B = 6;
const byte LED_C = 5;
const byte LIGHT_SENSOR = A0;
const byte DOOR_SWITCH = A1;
const byte INC_BUTTON = 4;
const byte DEC_BUTTON = 2;
const byte SET_BUTTON = 3;

const int BRIGHT_ADJUSTMENT_INCREMENT = 20; // Each button press changes the brightness by this amount out of 1023
const byte LOW_BRIGHT_THRESHOLD_ADJUSTMENT_INCREMENT = 5;
const long TIMEOUT_ADJUSTMENT_INCREMENT = 900000; //milliseconds : 15 minute interval
const byte ANALOG_DEBOUNCE = 3; //Requred ADC value delta before a change is registered
const byte SETTING_BUTTON_DEBOUNCE = 20; //milliseconds
const byte DOOR_SWITCH_DEBOUNCE = 50; //milliseconds
const long MILLIS_PER_MINUTE = 60000;

boolean emptyRom = true;


//PWM values for high and low modes (default inc)
unsigned int highBright = 1023;
unsigned int lowBright = 128;

//Setting Menu
byte settingMenu = 0; //Menu currently being interacted with, 0 for not in menu (normal operation)
boolean menuSetup = false;
unsigned long lastMenuInteraction = 0; //timestamp in milliseconds

//Photosensor
//In installations without this sensor, install a jumper or set lowBrightTrheshold to 0
byte ambientSensor = 0;
byte lowBrightThreshold = 60;
byte lastAmbientSensor = 0; 
byte ambientSensorTest = 0;

//Timeouts
unsigned long lightsOnTime = 0; //timestamp in milliseconds
unsigned long lightTimeout = 30;  //minutes
unsigned long menuTimeout = 5; //milliseconds: 5 minutes

//Digital input setup
ezButton incButton(INC_BUTTON);
ezButton decButton(DEC_BUTTON);
ezButton setButton(SET_BUTTON);
ezButton doorSwitch(DOOR_SWITCH, INPUT);

//Output setup
Led pwm(PWM_OUT);
Led indicatorA(LED_A);
Led indicatorB(LED_B);
Led indicatorC(LED_C);

void setup() {
  
  //Debugging
  Serial.begin(9600);

  //Button Setup and Debounce
  incButton.setDebounceTime(SETTING_BUTTON_DEBOUNCE);
  decButton.setDebounceTime(SETTING_BUTTON_DEBOUNCE);
  setButton.setDebounceTime(SETTING_BUTTON_DEBOUNCE);
  doorSwitch.setDebounceTime(DOOR_SWITCH_DEBOUNCE);

  //LED setup and default values
  pwm.initPwm16(0);
  indicatorA.init(false);
  indicatorB.init(false);
  indicatorC.init(false);

  //Pre-populate ADC values
  ambientSensor = analogRead(LIGHT_SENSOR) / 4;
  lastAmbientSensor = ambientSensorTest;

  //Factory default settings reset: Hold menu button while resetting the Arduino
  boolean skipMemoryRead = false;
  if(!setButton.getState()){
    skipMemoryRead = true;
    indicatorA.flashOn(10);
    indicatorB.flashOn(10);
    indicatorC.flashOn(10);
  }

  //Read values from EEPROM

  //if bytes in addresses 0 and 1 are not 77 and 69 respectively,
  //assume no settings have ever been written
  //skip reading, and use defaults
  if(!skipMemoryRead && EEPROM.read(0) == 77 && EEPROM.read(1) == 69){
    emptyRom = false;
    EEPROM.get(2, highBright);
    EEPROM.get(4, lowBright);
    
    lowBrightThreshold = EEPROM.read(6);
    lightTimeout = EEPROM.read(7);
  }
  //Convert light on timeout from minutes to milliseconds
  lightTimeout = lightTimeout * MILLIS_PER_MINUTE;
  menuTimeout = menuTimeout * MILLIS_PER_MINUTE;

  //Output settings to Serial
  Serial.println("\n----------------------------------------\n");
  Serial.print("Pantry Lighting Solution v");
  Serial.println(REVISION_NUMBER);
  Serial.println("By: Matt Erculiani");
  Serial.println("(C) 2022\n");
  Serial.println("----------------Settings----------------");
  if(skipMemoryRead){
    Serial.println("Panic sequence detected! Default values loaded");
    Serial.println("Cycle through all settings once to lock in defaults");
    Serial.println("If you did this on accident, just reset");
  }
  else if(emptyRom){
    Serial.println("No ROM data detected, using defaults");
  }
  else{
    Serial.println("ROM data detected! Using your saved settings");
  }
  Serial.print("HighBright Level: ");
  Serial.println(highBright);
  Serial.print("LowBright Level: ");
  Serial.println(lowBright);
  Serial.print("LowBright Threshold: ");
  Serial.println(lowBrightThreshold);
  Serial.print("Timeout Period: ");
  Serial.print(lightTimeout/MILLIS_PER_MINUTE);
  Serial.println(" minutes");
  Serial.println("-------------Runtime Output-------------");
  
  //FINALLY: Bring lights up if door is open
  setLights();
}

void loop() {
  
  //Button housekeeping
  incButton.loop();
  decButton.loop();
  setButton.loop();
  doorSwitch.loop();
  
  //LED housekeeping
  pwm.loop();
  indicatorA.loop();
  indicatorB.loop();
  indicatorC.loop();


//  Serial.print("Photoresistor: ");
//  Serial.println(analogRead(A0)/4);

  //Convert 10 bit ADC value into 8 bits to match PWM output
  //De-bounce analog increment by requring minimum change of ANALOG_DEBOUNCE
  if(pwm.getBrightness() == pwm.getBrightTarget()){
    ambientSensorTest = analogRead(LIGHT_SENSOR) / 4;
    if(ambientSensorTest != lastAmbientSensor){
      if(ambientSensorTest == 255){
        ambientSensor = 255;
        lastAmbientSensor = 255;
      }
      else if(ambientSensorTest == 0){
        ambientSensor = 0;
        lastAmbientSensor = 0;
      }
      else if (lastAmbientSensor - ambientSensorTest > ANALOG_DEBOUNCE || lastAmbientSensor - ambientSensorTest < -ANALOG_DEBOUNCE){
        ambientSensor = analogRead(LIGHT_SENSOR) / 4;
        lastAmbientSensor = ambientSensor;
//        Serial.print("Ambient Light Reading: ");
//        Serial.println(ambientSensor);
      }
    }
  }
  
  if(setButton.isPressed()){
    settingMenu++;
    menuSetup = true;
  }
  switch(settingMenu){
    //program high-mode brightness level
    case 1:
      if(menuSetup){
        indicatorA.on();
        indicatorB.off();
        indicatorC.off();
        pwm.setBrightness(highBright);
        lastMenuInteraction = millis();
        menuSetup = false;
      }
      if(incButton.isPressed()){
        if(!pwm.fadeIncrement(BRIGHT_ADJUSTMENT_INCREMENT)){
          pwm.flashOff(1);
        }
      }
      if(decButton.isPressed()){
        pwm.fadeIncrement(-BRIGHT_ADJUSTMENT_INCREMENT);
      }
      break;
      
    //program low-mode brightness level
    case 2:
      if(menuSetup){
        highBright = pwm.getBrightness();
        pwm.setBrightness(lowBright);
        indicatorA.on();
        indicatorB.on();
        indicatorC.off();
        lastMenuInteraction = millis();
        menuSetup = false;
      }
      if(incButton.isPressed()){
        if(!pwm.fadeIncrement(BRIGHT_ADJUSTMENT_INCREMENT)){
          pwm.flashOff(1);
        }
      }
      if(decButton.isPressed()){
        pwm.fadeIncrement(-BRIGHT_ADJUSTMENT_INCREMENT);
      }
      break;
      
    //program low-mode brightness threshold
    case 3:
      if(menuSetup){
        lowBright = pwm.getBrightness();
        indicatorA.on();
        indicatorB.on();
        indicatorC.on();
        lightsOff();
        lastMenuInteraction = millis();
        menuSetup = false;
      }
      if(incButton.isPressed()){
        if(lowBrightThreshold + LOW_BRIGHT_THRESHOLD_ADJUSTMENT_INCREMENT > lowBrightThreshold){
          if(lowBrightThreshold < ambientSensor){
            lowBrightThreshold += LOW_BRIGHT_THRESHOLD_ADJUSTMENT_INCREMENT;
            if(lowBrightThreshold > ambientSensor){
              pwm.flashOn(1);
            }
          }
          else{
            lowBrightThreshold += LOW_BRIGHT_THRESHOLD_ADJUSTMENT_INCREMENT;
          }
        }
        Serial.print("LowBright Threshold: ");
        Serial.println(lowBrightThreshold);
      }
      if(decButton.isPressed()){
        if(lowBrightThreshold - LOW_BRIGHT_THRESHOLD_ADJUSTMENT_INCREMENT < lowBrightThreshold){
          if(lowBrightThreshold > ambientSensor){
            lowBrightThreshold -= LOW_BRIGHT_THRESHOLD_ADJUSTMENT_INCREMENT;
            if(lowBrightThreshold < ambientSensor){
              pwm.flashOn(1);
            }
          }
          else{
            lowBrightThreshold -= LOW_BRIGHT_THRESHOLD_ADJUSTMENT_INCREMENT;
          }
        }
        Serial.print("LowBright Threshold: ");
        Serial.println(lowBrightThreshold);
      }
      break;

    //Program timeout in case door is left open
    //Easiest way to program this would be 
    case 4:
      if(menuSetup){
        indicatorA.off();
        indicatorB.on();
        indicatorC.on();
        lightsOff();
        lastMenuInteraction = millis();
        menuSetup = false;
      }
      if(incButton.isPressed()){
        if(lightTimeout + TIMEOUT_ADJUSTMENT_INCREMENT > lightTimeout){
          lightTimeout += TIMEOUT_ADJUSTMENT_INCREMENT;
          pwm.flashOn(1);
        }
        else{
          pwm.flashOn(2);
        }
      }
      if(decButton.isPressed()){
        if(lightTimeout - TIMEOUT_ADJUSTMENT_INCREMENT < lightTimeout){
          lightTimeout -= TIMEOUT_ADJUSTMENT_INCREMENT;
          pwm.flashOn(1);
        }
        else{
          pwm.flashOn(2);
        }
      }
      break;
      
    //Normal operating state
    default:
      if(menuSetup){
        EEPROM.put(2, highBright);
        EEPROM.put(4, lowBright);
        EEPROM.put(6, lowBrightThreshold);
        EEPROM.put(7, (byte)(lightTimeout/MILLIS_PER_MINUTE));
        settingMenu = 0;
        indicatorA.off();
        indicatorB.off();
        indicatorC.off();
        setLights();
        menuSetup = false;
        if(emptyRom){
          EEPROM.update(0, (byte) 77);
          EEPROM.update(1, (byte) 69);
        }
      }
      if(doorSwitch.isPressed()){
        lightsOn();
      }
      else if(doorSwitch.isReleased()) {
        lightsOff();
      }
      break;
  }

  //Light Timeout
  if(lightTimeout && pwm.getBrightness()){ //lightTimeout != 0 AND lights are on
  // I had "&& millis() > lightTimeout" as part of the above if condition and 
  // I can't remember why it's there and don't think it's important
    if((unsigned long) (millis() - lightsOnTime) >= lightTimeout && !pwm.isBusy()){
        lightsOff();
//        Serial.println("TIMEOUT!");
//        Serial.print("Lights left on for ");
//        Serial.println((unsigned long) (millis() - lightsOnTime));
    }
  }

  //Menu Timeout; Does not save changes.
  if(settingMenu && (unsigned long) (millis() - lastMenuInteraction) >= menuTimeout){
      settingMenu = 0;
      menuSetup = false;
      indicatorA.off();
      indicatorB.off();
      indicatorC.off();
      setLights();
  }

  

//debugging (specific interval)
  if((unsigned long) (millis() - outputMemory) >= memoryInterval && DEBUGGING){
    Serial.print("millis()=");
    Serial.println(millis());
    Serial.print("freeMemory()=");
    Serial.println(freeMemory());
    Serial.print("pwmBrightness=");
    Serial.println(pwm.getBrightness());
    Serial.print("pwmBrightTarget=");
    Serial.println(pwm.getBrightTarget());
    Serial.print("ambientSensor=");
    Serial.println(ambientSensor);
    
    outputMemory = millis();
  }
}

void lightsOn(){
  if(ambientSensor >= lowBrightThreshold){
    pwm.fadeTo(highBright);
  }
  else{
    pwm.fadeTo(lowBright);
  }
  lightsOnTime = millis();
}
void lightsOff(){
  pwm.fadeTo(0);
}
void setLights(){
  if(!doorSwitch.getState()){
    lightsOn();
  }
  else{
    lightsOff();
  }
}
