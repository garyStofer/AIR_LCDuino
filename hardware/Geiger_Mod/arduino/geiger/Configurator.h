// battery Ni-MH settings are here. 
#define LOW_BAT 4550     // Battery is NOT OK 
#define FUL_BAT 4850     // Battery is OK

// define constants, do not modify pins
#define LED A3           // led and buzzer on Analog Pin 3
#define LIGHT 3          // backlight on Digital Pin 3
#define ALARM 600        // 600 cpm default alarm level
#define BARGRAPH 2100    // 1500 cpm by default
#define BUTTON_UP A0     // define upper button pin
#define BUTTON_DO A1     // define bottom button pin
#define LED_TIME 5       // for led/buzzer to be ON; 5-20ms for most LED's. If you also use active 5V buzzer, then find the optimal time for the clicker sound. 

// PWM starting points, you need to calibrate with 1Giga ohm resistor the tube starting voltage!
// connect 5.00V power supply and find optimal PWM_FUL value
// then add 5 points to PWM_MID and 10 points to PWM_LOW
// start with 90!

#define PWM_FUL 120       // pwm settings for different battery levels 
#define PWM_MID 125
#define PWM_LOW 130
#define PWN_FREQ 8       // PWM frequency setup


// GM Tube setting
#define FACTOR_USV 0.0057    // Sieverts Conversion Factor, known value from tube datasheet 
/*
SBM-20   0.0057
SBM-19   0.0021
SI-29BG  0.0082
SI-180G  0.0031
J305     0.0081
SBT11-A  0.0031
SBT-9    0.0117
*/

#define DOSE_LIMIT 999.98       // Maximum allowed absorbed value in uSV
                             // be sure it can be written to 6 lcd characters "000.00"
                             

// EEPROM addresses
#define UNITS_ADDR    0     // int 2 bytes
#define FLAG_ADDR     10    // byte 
#define ABSOR_ADDR    2     // unsigned long 4 bytes

 

// custom made messages for lcd
// need to use progmem to safe Atmega 328 SRAM
//                               1234567890123456
prog_char string_0[]  PROGMEM = "  Arduino IDE   ";
prog_char string_1[]  PROGMEM = " Geiger Counter ";
prog_char string_2[]  PROGMEM = "TOTAL DOSE   uSv";  
prog_char string_3[]  PROGMEM = " ";  
prog_char string_4[]  PROGMEM = " ";  
prog_char string_5[]  PROGMEM = "SAVED!";  
prog_char string_6[]  PROGMEM = "Sieverts  ";         
prog_char string_7[]  PROGMEM = "Roentgen  ";         
prog_char string_8[]  PROGMEM = "CPM:";               


PROGMEM const char *string_table[] = {
  string_0,  string_1,  string_2,  string_3,  string_4,  string_5,
  string_6, string_7, string_8
};


// battery indicators
byte batmid[8] = {
  0b00000,
  0b01110,
  0b10001,
  0b10011,
  0b10111,
  0b11111,
  0b11111,
  0b00000
};

byte batlow[8] = {
  0b00000,
  0b01110,
  0b10001,
  0b10011,
  0b10101,
  0b11001,
  0b11111,
  0b00000
};


// bargraph symbols
// blank
byte bar_0[8] = {        
  B00000,
  B00000,
  B00000,
  B11111,
  B11111,
  B00000,
  B00000,
  B00000
}; 

// 1 bar
byte bar_1[8] = {
  B00000,
  B10000,
  B10000,
  B11111,
  B11111,
  B10000,
  B10000,
  B00000
};

// 2 bars
byte bar_2[8] = {
  B00000,
  B11000,
  B11000,
  B11111,
  B11111,
  B11000,
  B11000,
  B00000
};

// 3 bars
byte bar_3[8] = {
  B00000,
  B11100,
  B11100,
  B11111,
  B11111,
  B11100,
  B11100,
  B00000
};

// 4 bars
byte bar_4[8] = {
  B00000,
  B11110,
  B11110,
  B11111,
  B11111,
  B11110,
  B11110,
  B00000
};

// 5 bars
byte bar_5[8] = {
  B00000,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B00000
};


