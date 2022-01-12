#include "led_control.h"
#define blinkTime 100
#define fadeDelay 1

Led::Led(byte pin){
  this->pin = pin;
}

void Led::setPwm(unsigned int pwm_target) {
  if(maxBright > 255){
    noInterrupts();
    switch (pin) {
      case 9 :
        OCR1A = pow(pwm_target, pwm16brightnessExponent);
        break;
      case 10 :
        OCR1B = pow(pwm_target, pwm16brightnessExponent);
        break;
    }
    interrupts();
  }
  else{
    analogWrite(pin, LinearAppearance[pwm_target]);
  }
}

void Led::init(boolean defaultState){
  pinMode(pin, OUTPUT);
  maxBright = 1;
  if(defaultState){
    on();
  }
  else{
    off();
  }
}
void Led::initPwm(byte defaultBright){
  brightness = defaultBright;
  brightTarget = defaultBright;
  pwm = true;
  pinMode(pin, OUTPUT);
  setPwm(brightness);
}
void Led::initPwm16(unsigned int defaultBright){
  if(pin == 9 || pin == 10){
    noInterrupts();
    if(pin == 9){
      TCCR1A = (TCCR1A & B00111100) | B10000010;   //Phase and frequency correct, Non-inverting mode, TOP defined by ICR1
    }
    else if(pin == 10){
      TCCR1A = (TCCR1A & B11001100) | B10000010;   //Phase and frequency correct, Non-inverting mode, TOP defined by ICR1
    }
    TCCR1B = (TCCR1B & B11100000) | B00010001;   //No prescale
    
    maxBright = pwm16maxBright;
    ICR1 = 65535;
    interrupts();
    initPwm(defaultBright);
  }
  
}
void Led::on(){
  brightness = maxBright;
  brightTarget = maxBright;
  if(pwm){
    setPwm(maxBright);
  }
  else{
    digitalWrite(pin, HIGH);
  }
}
void Led::off(){
  digitalWrite(pin, LOW);
  brightness = 0;
  brightTarget = 0;
}
boolean Led::setBrightness(unsigned int brightness){
  if(this->brightness != brightness){
    this->brightness = brightness;
    brightTarget = brightness;
    setPwm(brightness);
    return true;
  }
  return false;
}
unsigned int Led::getBrightness(){
  return brightness;
}
unsigned int Led::getBrightTarget(){
  return brightTarget;
}
boolean Led::isBusy(){
  return busy;
}
boolean Led::flashOff(byte flashes){
  if(brightness){
    flashOffs = flashes*2-1;      
    if(pwm){
      setPwm(0);
    }
    else{
      digitalWrite(pin, LOW);
    }
    eventStart = millis();
    return true;
  }
  return false;
}
boolean Led::flashOn(byte flashes){
  if(brightness != maxBright){
    flashOns = flashes*2-1;
    if(pwm){
      setPwm(maxBright);
    }
    else{
      digitalWrite(pin, HIGH);
    }
    eventStart = millis();
    return true;
  }
  return false;
}
//returns TRUE if adjustment can occur
//returns FALSE if adjustment is not possible
boolean Led::fadeIncrement(long increment){
  if(brightTarget + increment < 0){
    brightTarget = 0;
  }
  else if(brightTarget + increment > maxBright){
    brightTarget = maxBright;
  }
  else{
    brightTarget += increment;
  }
  if(brightTarget != brightness){
    eventStart = millis();
    return true;
  }
  return false;
}
//returns TRUE if adjustment can occur
//returns FALSE if adjustment is not possible
boolean Led::fadeTo(unsigned int brightTarget){
  this->brightTarget = brightTarget;
  if(brightTarget != brightness){
    return true;
  }
  return false;
}
void Led::loop(){
  if(flashOffs){
    if((unsigned long) (millis() - eventStart) >= blinkTime){
      if(!(flashOffs % 2)){
        if(pwm){
          setPwm(0);
        }
        else{
          digitalWrite(pin, LOW);
        }
      }
      else{
        if(pwm){
          setPwm(brightness);
        }
        else{
          digitalWrite(pin, HIGH);
        }
      }
      flashOffs--;
      eventStart = millis();
      busy = true;
    }
  }
  else if(flashOns){
    if((unsigned long) (millis() - eventStart) >= blinkTime){
      if(!(flashOns % 2)){
        if(pwm){
          setPwm(maxBright);
        }
        else{
          digitalWrite(pin, HIGH);
        }
      }
      else{
        if(pwm){
          setPwm(brightness);
        }
        else{
          digitalWrite(pin, LOW);
        }
      }
      flashOns--;
      eventStart = millis();
      busy = true;
    }
  }
  else if(pwm && brightness != brightTarget){
    unsigned long currentInterval = millis() - eventStart; 
    if(currentInterval >= fadeDelay){
      if(brightTarget > brightness){
        brightness++;
        setPwm(brightness);
        eventStart = millis();
      }
      else{
        brightness--;
        setPwm(brightness);
        eventStart = millis();
      }
    }
    busy = true;
  }
  else if(brightness != brightTarget){
    if(brightTarget > brightness){
        on();
      }
      else{
        off();
      }
      busy = true;
  }
  else{
    busy = false;
  }
}
