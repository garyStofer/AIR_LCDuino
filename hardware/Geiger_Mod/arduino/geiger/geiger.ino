/* ARDUINO IDE GEIGER COUNTER ver 1.01 produced by RH Electronics http://rhelectronics.net
You can purchase the hardware for this project on RH Electronics website. The DIY kit include high quality pcb
and electronics components to solder this project.

This code was successfully compiled on Arduino IDE 1.05 11556 bytes 29 Jan. 2014
------------------------------------------------------------------------------------------------------------------------------
This is open source code written by Alex Boguslavsky. I have used some parts of BroHogan's source code for reference, but 
I have made my own electrical circuit, software and pcb re-design because I think using 555 timer and 74hc14 is excessive when it possible 
to produce high voltage with PWM or led flashing with another IO. Partly this code is implementation of my own ideas
and algorithms and partly just copy-paste. The project IS NOT PIN TO PIN COMPATIBLE with BroHogan!

The main idea was to create simple portable radiation logger based on open arduino IDE. You are free to modify and 
improve this code under GNU licence. Take note, you are responsible for any modifications. Please send me your suggestions
or bug reports at support@radiohobbystore.com

------------------------------------------------------------------------------------------------------------------------------
Main Board Features:

1. High voltage is programmed to 400V. Voltage stabilization IS NOT strict, but when battery is too low it will correct the
output. With the default pre-set high voltage can be trimmed up to 480V, I have checked with 1G resistor divider. If you need 500V
or more I recommend to use external voltage multiplier x2 or x3. 

2. My improved counting algorithm: moving average with checking of rapid changes.

3. Smart back light. If enabled, back light pin can be used as alarm pin for external load. Counter support 16x2 lcd only.

4. Sieverts or Rouentgen units for displaying current dose. User selection saved into EE prom for next load, but can be changed
with key "down" any time.

5. UART logging with RH Electronics "Radiation Logger" http://www.rhelectronics.net/store/radiation-logger.html
Other similar logging software is compatible. Please also visit: http://radmon.org

6. Short LED flash for each nuclear event from tube. Optional Active 5V Buzzer for sound and alarm can be connected to the board.

7. Fast bar graph on lcd display. No sense for background radiation, but can be very useful for elevated values. Shows counts 
per second measurement.

8. Two tact switch buttons. One to switch units, second to log absorbed data to EE prom.

9. Automatic EE prom logging of absorbed dose once per hour.

10. DIP components.


------------------------------------------------------------------------------------------------------------------------------

Additional hardware required:

1. GM Tube

2. USB TTL Arduino Module

-------------------------------------------------------------------------------------------------------------------------------
Warranty Disclaimer:
This Geiger Counter kit is for EDUCATIONAL PURPOSES ONLY. 
You are responsible for your own safety during soldering or using this device!
Any high voltage injury caused by this counter is responsibility or the buyer only. 
DO NOT BUY IF YOU DON'T KNOW HOW TO CARRY HIGH VOLTAGE DEVICES! 
The designer and distributor of this kit is not responsible for radiation dose readings 
you can see, or not see, on the display. You are fully responsible for you safety and health
in high radiation area. The counter dose readings cannot be used for making any decisions. 
Software and hardware of the kit provided AS IS, without any warranty for precision radiation measurements. 
When you buy the kit it mean you are agree with the disclaimer!


Copyright (C) <2013>  <Alex Boguslavsky RH Electronics>

http://rhelectronics.net
support@radiohobbystore.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

//----------------- load parameters--------------------//

#define RAD_LOGGER    true          // enable serial CPM logging to computer for "Radiation Logger"
#define SMART_BL      true          // if true LCD backlight controlled through ALARM level and key, else backlight is always on 
#define ACTIVE_BUZZER true          // true if active 5V piezo buzzer used. set false if passive soldered. 
#define EEPROM_LOG    true          // if true will log absorbed dose to eeprom every 60 minutes. Use only if there is no Logging Shield available!


// install supplied libraries from lib.zip!
#include <Arduino.h>              
#include <avr/pgmspace.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <SPI.h>
#include "Configurator.h"




//-------------------------------------------------------//

//-----------------Define global project variables-------------------------//

unsigned long CurrentBat;                        // voltage of the battery
unsigned long counts;                            // counts counter
unsigned int  countsHSecond;                     // half second counter
static unsigned long value[] = {0,0,0,0,0,0};           // array variable for cpm moving algorithm
static unsigned long cpm;                               // cpm variable
static unsigned long rapidCpm;                          // rapidly changed cpm
static unsigned long minuteCpm;                         // minute cpm value for absorbed radiation counter
static unsigned long previousValue;                     // previous cpm value

unsigned long previousMillis = 0;                // millis counters
unsigned long previousMillis_bg  = 0;
unsigned long previousMillis_pereferal = 0;
unsigned long previousMillis_hour = 0;
int n = 0;                                       // counter for moving average array
long result;                                     // voltage reading result
int buttonStateDo = 1;                           // buttons status
int buttonStateUp = 1;
unsigned int barCount;                           // bargraph variable
static float absorbedDose;                       // absorbed dose
static float dose;                               // radiation dose
static float minuteDose;                         // minute absorbed dose
boolean savedEeprom = false;                     // eeprom flag

boolean event         = false;                   // GM tube event received, lets make flag 
//boolean limit         = false;                   // absorbed dose limit flag

int cps_bar = BARGRAPH / 60;                     // fast bargraph scale for cps

//-------------Roentgen/ Sieverts conversion factor----------------//
float factor_Rn = (FACTOR_USV * 100000) / 877;   // convert to Roentgen
float factor_Sv = FACTOR_USV;
float factor_Now = factor_Sv;                    // Sieverts by default
int units;                                       // 0 - Sieverts; 1 - roentgen
//-----------------------------------------------------------------//


//-------------------Initialize LCD---------------------//
LiquidCrystal lcd(9, 4, 8, 7, 6, 5);
//----------------------------------------------------//



///////////////////////////////////////////////////////////////////////////////////////////
//------------------------------------SETUP AREA-----------------------------------------//
///////////////////////////////////////////////////////////////////////////////////////////

void setup()
{
  // zero some important variables first
  counts = 0;
  countsHSecond = 0;
  minuteCpm = 0;
  minuteDose = 0;
  n = 0;
  cpm = 0;
  
  
  // configure atmega IO
  pinMode(LIGHT, OUTPUT);         // turn on backlight
  digitalWrite(LIGHT, HIGH);  
  pinMode(LED, OUTPUT);           // configure led pin as output
  digitalWrite(LED, LOW);
  pinMode(2, INPUT);              // set pin INT0 input for capturing GM Tube events
  digitalWrite(2, HIGH);          // turn on pullup resistors 
  pinMode(BUTTON_UP, INPUT);      // set buttons pins as input with internal pullup
  digitalWrite(BUTTON_UP, HIGH);
  pinMode(BUTTON_DO, INPUT);
  digitalWrite(BUTTON_DO, HIGH);  
  pinMode(A5, OUTPUT);            // Set A5 as GND point for buzzer
  digitalWrite(A5, LOW);
   
 // start uart on 9600 baud rate 
  Serial.begin(9600);            
  delay(1000);
  

 // start LCD display  
  lcd.begin(16, 2);
 // create and load custom characters to lcd memory  
  lcd.createChar(6, batmid);   // create battery indicator for mid state
  lcd.createChar(7, batlow);   // create battery indicator for low state
  lcd.createChar(0, bar_0);    // load 7 custom characters in the LCD
  lcd.createChar(1, bar_1);
  lcd.createChar(2, bar_2);
  lcd.createChar(3, bar_3);
  lcd.createChar(4, bar_4);
  lcd.createChar(5, bar_5);

 // extract starting messages from memory and print it on lcd
  lcd.setCursor(0, 0);
  lcdprint_P((const char *)pgm_read_word(&(string_table[0]))); // "  Arduino IDE   "
  lcd.setCursor(0, 1);
  lcdprint_P((const char *)pgm_read_word(&(string_table[1]))); // " Geiger Counter "
  blinkLed(3,50);                                              // say hello!
  delay(2000);
  pinMode(10, OUTPUT);          // making this pin output for PWM
  
    
  //---------------starting tube high voltage----------------------//
 
  setPwmFrequency(10, PWN_FREQ);    //set PWM frequency
  CurrentBat = readVcc();  
  if (CurrentBat >= FUL_BAT){
    analogWrite(10, PWM_FUL);      //correct tube voltage
  }
  
  if (CurrentBat <= LOW_BAT){
    analogWrite(10, PWM_LOW);      //correct tube voltage

}
  
  if (CurrentBat < FUL_BAT && CurrentBat > LOW_BAT){
    analogWrite(10, PWM_MID);      //correct tube voltage
  } 
  delay(2000);  
//------------------------------------------------------------------//

//---------------------read EEPROM---------------------------------//

  byte units_flag = EEPROM.read(FLAG_ADDR);   // check if we have a new atmega chip with blank eeprom
   if (units_flag == 0xFF){
     EEPROM.write(UNITS_ADDR, 0x00);          // and writing zeros instead of 0xFF for new atmega
     EEPROM.write(UNITS_ADDR+1, 0x00);
     EEPROM.write(ABSOR_ADDR, 0x00);
     EEPROM.write(ABSOR_ADDR+1, 0x00);
     EEPROM.write(ABSOR_ADDR+2, 0x00);
     EEPROM.write(ABSOR_ADDR+3, 0x00);
     EEPROM.write(FLAG_ADDR,  0x00);
   }
   
  units = EEPROM.read(UNITS_ADDR);
  
  
  #if (EEPROM_LOG)
  absorbedDose = eepromReadFloat(ABSOR_ADDR); 
  if (readButtonUp()== LOW){
    absorbedDose = 0.00;
    eepromWriteFloat(ABSOR_ADDR, absorbedDose);
  }
  #endif
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcdprint_P((const char *)pgm_read_word(&(string_table[2])));   //"TOTAL DOSE   uSv"
  lcd.setCursor(0,1);
  pFloat(absorbedDose);
  delay(3000);
//------------------------------------------------------------------//

// prepare lcd
  lcd.clear();
  

//---------------------Allow external interrupts on INT0---------//
  attachInterrupt(0, tube_impulse, FALLING);
  interrupts();
//---------------------------------------------------------------//

}

///////////////////////////////////////////////////////////////////////////////////////
//-----------------------------MAIN PROGRAM CYCLE IS HERE----------------------------//
///////////////////////////////////////////////////////////////////////////////////////

void loop()
{
  unsigned long currentMillis = millis();

//--------------------------------makes beep and led---------------------------------//
   if (event == true){                    // make led and beep for every tube event
   tone(LED, 4000, LED_TIME);             // http://letsmakerobots.com/node/28278 
   event = false;
   }
//-----------------------------------------------------------------------------------//
//--------------------------What to do if buttons pressed----------------------------//
   // swith dose units
      if (readButtonDo()== LOW){
      units = 1 - units;
      digitalWrite(LIGHT, HIGH);
      if (units == 0){
      lcd.setCursor(0, 0); 
      lcdprint_P((const char *)pgm_read_word(&(string_table[6]))); // "Sieverts"
      

       }
       else
       {
        lcd.setCursor(0, 0);
        lcdprint_P((const char *)pgm_read_word(&(string_table[7]))); // "Roentgen"
        

    } 
    EEPROM.write (UNITS_ADDR, units); //save units to eeprom
    }

    
    if (readButtonUp()== LOW){
    digitalWrite(LIGHT, HIGH);
    float absorbedDose_tmp = eepromReadFloat(ABSOR_ADDR);  // check if eeprom value is smaller than
                                                           // current absorbed dose and write to eeprom
    if (absorbedDose > absorbedDose_tmp){                  // should safe eerpom cycles
    eepromWriteFloat(ABSOR_ADDR, absorbedDose);
    lcd.setCursor(10, 0);
    lcdprint_P((const char *)pgm_read_word(&(string_table[5]))); // "SAVED!"
      }
    }

//------------------------------------------------------------------------------------//



//---------------------------What to do every 500ms ----------------------------------//
  if(currentMillis - previousMillis_bg > 500){      
   previousMillis_bg = currentMillis;
   
     #if (SMART_BL)                          // check backlight alarm status
     if (countsHSecond > (ALARM / 120)){     // actually we need ALARM / 120 because it half second measure
       digitalWrite(LIGHT, HIGH);            // but I like how it works with ALARM / 60, make your choice 
      
      // another way  directly control of the bl led, keep commented!
      // PORTD |= _BV(3);        // turn on bl led, arduino IDE is very slow, we need direct approach to the port
     }
     #endif
   
   barCount = countsHSecond * 2;            // calculate and draw fast bargraph here   
   lcd.setCursor(10,1);
   lcdBar(barCount);
   countsHSecond = 0;
   
 
 }
//------------------------------------------------------------------------------------//


//-------------------------What to do every 10 seconds-------------------------------------//
 if(currentMillis - previousMillis > 10000) {      // calculating cpm value every 10 seconds
    previousMillis = currentMillis;
    
      lcd.setCursor(0, 0);  
      lcd.write("\xe4");                          // print Mu , use "u" if have problem with lcd symbols table
      lcd.setCursor(0, 1);
     
      lcdprint_P((const char *)pgm_read_word(&(string_table[8])));  // "CPM:"
     value[n] = counts;
     previousValue = cpm;
     
     cpm = value[0] + value[1] + value[2] + value[3] +value[4] + value[5];

     if (n == 5)
     { 
       n = 0;
     }
     else
     {
       n++;
     }
     

     
//-------check if cpm level changes rapidly and make new recalculation-------//     
     if (previousValue > cpm){        
        rapidCpm = previousValue - cpm;
         if (rapidCpm >= 50){
           cpm = counts * 6;
         }
     }
     
         if (previousValue < cpm){
        rapidCpm = cpm - previousValue;
         if (rapidCpm >= 50){
           cpm = counts * 6;
         }
     }


// calculate and print radiation dose on lcd    
     clearLcd(4, 0, 6);
     
     if (units == 0){
       factor_Now = factor_Sv;
       lcd.setCursor(1, 0);
       lcd.print(F("Sv:")); 
     }
     else{
       factor_Now = factor_Rn;
       lcd.setCursor(1, 0);
       lcd.print(F("Rn:")); 
     }

      dose = cpm * factor_Now;
    
     if (dose >= 99.98){           // check if dose value are bigger than 99.99 to make the value printable on 5 lcd characters
      int roundDose;
      roundDose = (int) dose;
      lcd.setCursor(4, 0);
      lcd.print(roundDose);
     }
     else
     {

       pFloat(dose);
     }

// print cpm on lcd  
     clearLcd(4, 1, 6);

     lcd.setCursor(4, 1);
     lcd.print(cpm);
     
// turn on/off backlight alarm if cpm > < ALARM
     #if (SMART_BL)
     if (cpm> (ALARM / 3)){
       digitalWrite(LIGHT, HIGH);
       #if (ACTIVE_BUZZER)
       blinkLed(3,50);              // make additional alarm beeps
       #endif
     }
     else{
       digitalWrite(LIGHT, LOW);  
     }
     #endif
 

// send cpm value to Radiation Logger http://www.rhelectronics.net/store/radiation-logger.html

     #if (RAD_LOGGER)
        Serial.print(cpm);  // send cpm data to Radiation Logger 
        Serial.write(' ');  // send null character to separate next data
     #endif  
     counts = 0;            // clear variable for next turn
}
//--------------------------------------------------------------------------------------------//

//------------------------What to do every minute---------------------------------------------//
 if(currentMillis - previousMillis_pereferal > 60000){     
   previousMillis_pereferal = currentMillis;
  
 float minuteDose = (minuteCpm * factor_Sv)/60;       // count absorbed radiation dose during last minute

    absorbedDose += minuteDose;
  if (absorbedDose > DOSE_LIMIT){                     // check if you get 1mSv absorbed limit
     absorbedDose = 0.00;
  }
      
    
       clearLcd(10, 0, 6);
       lcd.setCursor(10,0);
       pFloat(absorbedDose);
       

 
      CurrentBat = readVcc();

      if (CurrentBat >= FUL_BAT){
        analogWrite(10, PWM_FUL);    //correct tube voltage if battery ok
        lcd.setCursor(15,0);
        lcd.write(' ');
      }
      
      if (CurrentBat <= LOW_BAT){
        analogWrite(10, PWM_LOW);    //correct tube voltage if battery low
        lcd.setCursor(15,0);
        lcd.write(7);    
      }
      
      if (CurrentBat < FUL_BAT && CurrentBat > LOW_BAT){
        analogWrite(10, PWM_MID);    //correct tube voltage if battery on the middle
        lcd.setCursor(15,0);
        lcd.write(6);    
      }        
       minuteDose = 0.00;      
       minuteCpm = 0;                         // reset minute cpm counter
 }  
//------------------------------------------------------------------------------------------//

//--------------------------------What to do every hour-------------------------------------//
// because eeprom has limited write cycles quantity, use once per hour auto logging

 #if (EEPROM_LOG)
 if(currentMillis - previousMillis_hour > 3600000){      
   previousMillis_hour = currentMillis;
   eepromWriteFloat(ABSOR_ADDR, absorbedDose);      // log by auto to eeprom once per hour
 }
 #endif
 
//-----------------------------------------------------------------------------------------//

}

///////////////////////////////////////////////////////////////////////////////////////
//-----------------------------MAIN PROGRAM ENDS HERE--------------------------------//
///////////////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////////
//-----------------------------SUB PROCEDURES ARE HERE-------------------------------//
///////////////////////////////////////////////////////////////////////////////////////  


//----------------------------------GM TUBE EVENTS ----------------------------------//
void tube_impulse ()                // interrupt care should short and fast as possible
{ 
  counts++;                         // increase 10 seconds cpm counter
  countsHSecond++;                  // increase 1/2 second cpm counter for bar graph
  minuteCpm++;                      // increase 60 seconds cpm counter
  event = true;                     // make event flag
  
 // another way to make event flag by directly control of the led, keep commented!
 // PORTC |= _BV(3);                // turn on led, arduino IDE is very slow, we need direct approach to the port

}
//----------------------------------------------------------------------------------//



//------------SECRET ARDUINO PWM----------------//
//  http://playground.arduino.cc/Code/PwmFrequency
void setPwmFrequency(int pin, int divisor) {
  byte mode;
  if(pin == 5 || pin == 6 || pin == 9 || pin == 10) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 64: mode = 0x03; break;
      case 256: mode = 0x04; break;
      case 1024: mode = 0x05; break;
      default: return;
    }
    if(pin == 5 || pin == 6) {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    } else {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  } else if(pin == 3 || pin == 11) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 32: mode = 0x03; break;
      case 64: mode = 0x04; break;
      case 128: mode = 0x05; break;
      case 256: mode = 0x06; break;
      case 1024: mode = 0x7; break;
      default: return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
  }
}
//------------SECRET ARDUINO PWM----------------//




//------------SECRET ARDUINO VOLTMETER----------------//
//  http://www.instructables.com/id/Secret-Arduino-Voltmeter/
unsigned long readVcc() { 
  unsigned long voltage;
  // READ 1.1V INTERNAL REFERENCE VOLTAGE
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); 
  ADCSRA |= _BV(ADSC); 
  while (bit_is_set(ADCSRA,ADSC));
  voltage = ADCL;
  voltage |= ADCH<<8;
  voltage = 1126400L / voltage; 
  return voltage;
}
//------------SECRET ARDUINO VOLTMETER----------------// 


//------------------read upper button with debounce-----------------//
int readButtonUp() {                         // reads upper button
  buttonStateUp = digitalRead(BUTTON_UP);
  if (buttonStateUp == 1){
    return HIGH;
  }
  else
  {
   delay(100);
    buttonStateUp = digitalRead(BUTTON_UP);
    if (buttonStateUp == 1){
      return HIGH;
    }
    else
    {
      return LOW;
    }
}
 
}
//------------------------------------------------------------------//

//------------------read bottom button with debounce-----------------//
int readButtonDo() {                         // reads bottom button
  buttonStateDo = digitalRead(BUTTON_DO);
  if (buttonStateDo == 1){
    return HIGH;
  }
  else
  {
   delay(100);
    buttonStateDo = digitalRead(BUTTON_DO);
    if (buttonStateDo == 1){
      return HIGH;
    }
    else
    {
      return LOW;
    }
}
}

//------------------------------------------------------------------//



//----------------------------------CPM bargraph-------------------------------------//
// made by DeFex     http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1264215873/0
// copied from BroHogan   https://sites.google.com/site/diygeigercounter/home
// adapted to show my custom bargraph scale symbols

void lcdBar(int c){                         // displays CPM as bargraph

  unsigned int scaler = cps_bar / 35;        
  unsigned int cntPerBar = (c / scaler);      // amount of bars needed to display the count
  unsigned int fullBlock = (cntPerBar / 6);   // divide for full "blocks" of 6 bars 
  unsigned int prtlBlock = (cntPerBar % 6 );  // calc the remainder of bars
  if (fullBlock >6){                          // safety to prevent writing to 7 blocks
    fullBlock = 6;
    prtlBlock = 0;
  }
  for (unsigned int i=0; i<fullBlock; i++){
    lcd.write(5);                          // print full blocks
  }
  lcd.write(prtlBlock>6 ? 5 : prtlBlock); // print remaining bars with custom char
  for (int i=(fullBlock + 1); i<8; i++){
    lcd.write(byte(0));                     // blank spaces to clean up leftover
  }  
}


//-----------------------------------------------------------------------------------//
////--------------------------EEPROM read write float----------------------------------//
////http://www.elcojacobs.com/storing-settings-between-restarts-in-eeprom-on-arduino/  //
//
float eepromReadFloat(int address){
   union u_tag {
     byte b[4];
     float fval;
   } u;   
   u.b[0] = EEPROM.read(address);
   u.b[1] = EEPROM.read(address+1);
   u.b[2] = EEPROM.read(address+2);
   u.b[3] = EEPROM.read(address+3);
   return u.fval;
}
 
void eepromWriteFloat(int address, float value){
   union u_tag {
     byte b[4];
     float fval;
   } u;
   u.fval=value;
 
   EEPROM.write(address  , u.b[0]);
   EEPROM.write(address+1, u.b[1]);
   EEPROM.write(address+2, u.b[2]);
   EEPROM.write(address+3, u.b[3]);
}
////-----------------------------------------------------------------------------------//

//-----------------------------print from progmem-----------------------------------//
// copied from BroHogan   https://sites.google.com/site/diygeigercounter/home

void lcdprint_P(const char *text) {  // print a string from progmem to the LCD
  /* Usage: lcdprint_P(pstring) or lcdprint_P(pstring_table[5].  If the string 
   table is stored in progmem and the index is a variable, the syntax is
   lcdprint_P((const char *)pgm_read_word(&(pstring_table[index])))*/
  while (pgm_read_byte(text) != 0x00)
    lcd.write(pgm_read_byte(text++));
}


//----------------------------------------------------------------------------------//


//------------------------------------LED BLINK--------------------------------------//

void blinkLed(int i, int time){                        // make beeps and blink signals
    int ii;                                            // blink counter
    for (ii = 0; ii < i; ii++){
      
    digitalWrite(LED, HIGH);   // 
    delay(time);
    digitalWrite(LED, LOW);
    delay(time);
    }
}


//--------------------------------Print zero on lcd----------------------------------//

void drawZerotoLcd(){                               // print zero symbol when need it
  lcd.write('0');
}


//--------------------------------Print float on lcd---------------------------------//
// 
static void pFloat(float dd){                       // avoid printing float on lcd
     lcd.print(int(dd));                            // convert it to text before 
     lcd.write('.');                                // sending to lcd
      if ((dd-int(dd))<0.10) {
       drawZerotoLcd(); 
       lcd.print(int(100*(dd-int(dd))));
      }
      else{
       lcd.print(int(100*(dd-int(dd))));      
     }
}

//--------------------------------clear lcd zone-------------------------------------//
void clearLcd(byte x, byte y, byte zone){
  int ii;
  lcd.setCursor (x,y);
  for (ii = 0; ii < zone; ii++){
    lcd.write(' ');
  }
}
  

///////////////////////////////////////////////////////////////////////////////////////
//-----------------------------SUB PROCEDURES ENDS HERE------------------------------//
///////////////////////////////////////////////////////////////////////////////////////     
  


