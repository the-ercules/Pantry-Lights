#include "led_control.h"
#include "MemoryFree.h"
#include <ezButton.h>
#include <EEPROM.h>

//Constants
const int AdjustmentIncrement = 20; // Each button press changes the brightness by this amount out of 255
const byte PWM_OUT = 9;
const byte LED_A = 2;
const byte LED_B = 6;
const byte LED_C = 12;
const byte LIGHT_SENSOR = A0;
const byte DOOR_SWITCH = 11;
const byte INC_BUTTON = 4;
const byte DEC_BUTTON = 8;
const byte SET_BUTTON = 7;
const byte ANALOG_DEBOUNCE = 3;
const byte SETTING_BUTTON_DEBOUNCE = 20;
const byte DOOR_SWITCH_DEBOUNCE = 50;
const long MILLIS_PER_MINUTE = 60000;

//Debugging
const int memoryInterval = 5000;
unsigned long outputMemory = 0;

//PWM values for high and low modes (default inc)
unsigned int highBright = 1023;
unsigned int lowBright = 128;

byte settingMenu = 0;
boolean menuSetup = true;

byte ambientSensor = 0;
byte lowBrightThreshold = 60;

unsigned long lightsOnTime = 0;
unsigned int lightTimeout = 1;

byte lastAmbientSensor = 0;
byte ambientSensorTest = 0;

ezButton incButton(INC_BUTTON);
ezButton decButton(DEC_BUTTON);
ezButton setButton(SET_BUTTON);
ezButton doorSwitch(DOOR_SWITCH, INPUT);

Led pwm(PWM_OUT);
Led indicatorA(LED_A);
Led indicatorB(LED_B);
Led indicatorC(LED_C);

void setup() {
  
  Serial.begin(9600);

  //Button Setup and Debounce
  incButton.setDebounceTime(SETTING_BUTTON_DEBOUNCE);
  decButton.setDebounceTime(SETTING_BUTTON_DEBOUNCE);
  setButton.setDebounceTime(SETTING_BUTTON_DEBOUNCE);
  doorSwitch.setDebounceTime(DOOR_SWITCH_DEBOUNCE);

  //Enable 16-Bit PWM on pins 9 and 10


  //LED setup and default values
  pwm.initPwm16(0);
  indicatorA.init(false);
  indicatorB.init(false);
  indicatorC.init(false);

  //Pre-populate ADC values
  ambientSensor = analogRead(LIGHT_SENSOR) / 4;
  lastAmbientSensor = ambientSensorTest;

  //Convert light on timeout from minutes to milliseconds
  lightTimeout = lightTimeout * MILLIS_PER_MINUTE;
  Serial.print("ligthTimeout: ");
  Serial.println(lightTimeout * MILLIS_PER_MINUTE);

  //Bring lights on if door is open
  if(!doorSwitch.getState()){
    lightsOn();
  }
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
        Serial.print("Ambient Light Reading: ");
        Serial.println(ambientSensor);
      }
    }
  }
  if(incButton.isPressed()){
    if(!pwm.fadeIncrement(AdjustmentIncrement)){
      pwm.flashOff(1);
    }
  }
  if(decButton.isPressed()){
    pwm.fadeIncrement(-AdjustmentIncrement);
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
        menuSetup = false;
      }
      
      break;
    case 2:
      if(menuSetup){
        highBright = pwm.getBrightness();
        pwm.setBrightness(lowBright);
        indicatorA.on();
        indicatorB.on();
        indicatorC.off();
        menuSetup = false;
      }
      
      break;
    case 3:
      if(menuSetup){
        lowBright = pwm.getBrightness();
        indicatorA.on();
        indicatorB.on();
        indicatorC.on();
        menuSetup = false;
      }
//      lightsOn();
      break;
      
    default:
      if(menuSetup){
        settingMenu = 0;
        indicatorA.off();
        indicatorB.off();
        indicatorC.off();
        if(!doorSwitch.getState()){
          lightsOn();
        }
        else{
          lightsOff();
        }
        menuSetup = false;
      }
      if(doorSwitch.isPressed()){
        lightsOn();
      }
      else if(doorSwitch.isReleased()) {
        lightsOff();
      }
      break;
  }

  if(pwm.getBrightness() && millis() > lightTimeout){
    if((unsigned long) (millis() - lightsOnTime) >= lightTimeout && !pwm.isBusy()){
        lightsOff();
        Serial.println("TIMEOUT!");
    }
  }

  

//debugging
  if((unsigned long) (millis() - outputMemory) >= memoryInterval){
    Serial.print("freeMemory()=");
    Serial.println(freeMemory());
    Serial.print("pwmBrightness=");
    Serial.println(pwm.getBrightness());
    Serial.print("pwmBrightTarget=");
    Serial.println(pwm.getBrightTarget());
    outputMemory = millis();
  }
}

void lightsOn(){
  if(ambientSensor > lowBrightThreshold){
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