// Sketch to read Bosh BMP085/ BMP180 and SI_7021 hygrometer and display various aviation relatred readings on 8x2 lcd display
// Optionally allows the reading of wind direction and speed indicators. See #define WITH_WIND
// Warning : This sketch needs to be compiled and run on a device with Optiboot since the watchdog doesn't work under std BL
// Last compiled and tested with Arduino IDE 1.6.6

#include "build_opts.h"		// Controls build time features
#include <LiquidCrystal.h>
#include <avr/wdt.h>
#include "BMP085_baro.h"
#include "SI_7021.h"
#include "TMP100.h"
#include "Atmos.h"
#include "Wind.h"	
#include "RPM.h"	
#include <EEPROM.h>


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  BUILD OPTIONS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

#define LCD_COLS 8
#define LCD_ROWS 2

//With a Temp-Dewpoint delta of less that 3degC (~5degF) you can expect fog, so I set the default for the alarm a bit higher than that.
#define TD_DELTA_ALARM	4.0 // in degC when the alarm is to come on
#define VOLT_LOW_ALARM 6.5
#define VOLT_HIGH_ALARM 15.0
#define FREEZE_ALARM 1.0


// The pin definitions are as per obfuscated Arduino pin defines -- see aka for ATMEL pin names as found on the MEGA328P spec sheet
#define Enc_A_PIN 2     // This generates the interrupt, providing the click  aka PD2 (Int0)
#define Enc_B_PIN 14    // This is providing the direction aka PC0,A0
#define Enc_PRESS_PIN 3	// aka PD3 ((Int1)
#define Enc_DIRECTION (-1)  // polarity of encoder , either -1 or +1 depending on the phase relation of the two signals in regard of the turn direction

#define LED1_PIN 10			// aka PB2
#define LED2_PIN 17			// aka PC3,A3

#define VBUS_ADC 7			// ADC7
#define VBUS_ADC_BW  (5.0*(14+6.8)/(1024*6.8))		//adc bit weight for voltage divider 14.0K and 6.8k to gnd.


#define UPDATE_PER 1000   // in ms second, this is the measure interval and also the LCD update interval

#define LONGPRESS_TIMEOUT 100000 // Note: not in milli seconds since this is used in a SW while loop timer

#define KMpMILE 0.62137119

enum Displays {
  V_Bus=0,
  D_Alt,
  Alt,
  Station_P,
  Rel_Hum,
  Temp,
  DewPoint,
#ifdef WetBulbTemp
  T_WetBulb,
#endif
  TD_spread,
#ifdef WITH_WIND
  Wind_DIR,
  Wind_SPD,
  Wind_AVG,
  Wind_GST,
#else ifdef WITH_RPM
  RPM,
#endif
  DISP_END        // this must be the last entry
 };

// Globals for rotary encoder
char EncoderCnt = RPM;    // Default startup display
char EncoderPressedCnt = 0;
char EncoderDirection = -1; // So that it decremets to a valid display in case the default display is not currently valid due to sensor lacking
unsigned char ShortPressCnt = 0;
unsigned char LongPressCnt = 0;
static bool MetricDisplay = false;


// Global LCD control class
LiquidCrystal lcd(9, 8, 6, 7, 4, 5); // in 4 bit interface mode

static bool No_Baro = true;
static bool No_Hygro = true;
static bool No_TMP100 = true;


/* Note regarding contrast control */
// Contrast control simply using a PWM doesn't work because the PWM needs to be filtered with a R-C, but the LCD itself is pulling the LCD control
// input up, so the R-C from the MCU would have to be relatively low in impendance <300ohms, making for a big capacitor and lots of current
// Would have to feed the filtered voltage into an OP-Amp to drive the LCD input properly.
// For now the hardware has a resistor pair that sets the contrast .
/*


  This ISR handles the reading of a quadrature encoder knob and inc or decrements  two global encoder variables
  depending on the direction of the encoder and the state of the button. The encoder functions at 1/4 the maximal possible resolution 
  and generally provides one inc/dec per mechanical detent.

  One of the quadrature inputs is used to trigger this interrupt handler, which checks the other quadrature input to decide the direction.
  It is assumed that the direction  quadrature signal is not bouncing while the first phase is causing the initial interrupt as it's signal 
  is 90deg opposed.  RC filtering of the contacts is required. 

  Three encoder count variables are being modified by this ISR
    1) EncoderCnt increments or decrements when the knob is turned without the button being pressed
    2) EncoderPressCnt increments or decrements when the knob is  turned while the button is also pressed. 
       This happens only after a LONG press timeout
    3) EncoderDirection either +1 or -1 depending on the direction the user turned the knob last
*/
void ISR_KnobTurn( void)
{
  if ( digitalRead( Enc_B_PIN ) )
    EncoderDirection = Enc_DIRECTION;
  else 
    EncoderDirection = -Enc_DIRECTION;
     
  if ( digitalRead( Enc_PRESS_PIN ) )
       EncoderCnt += EncoderDirection;
  else
       EncoderPressedCnt += EncoderDirection;
  
}


/*
  ISR to handle the button press interrupt. 
  
  Two modes of button presses are recognized. A short, momentary press, and a long, timing-out press.
  While the hardware de bounced button signal is sampled for up to LONGPRESS_TIMEOUT time in this ISR no other code is being executed. If the time-out occurs
  a long button press-, otherwise a short button press is registered. This timing has to be done by a software counter since interrupts are disabled
  and function millis() and micros() won't work during this time. 
  Software timing is CPU clock dependent and therefore has to be adjusted to the clock frequency. 
*/
void ISR_ButtonPress(void)
{
  volatile unsigned long t = 0;
  while ( !digitalRead( Enc_PRESS_PIN ))
  {
    if (t++ > LONGPRESS_TIMEOUT)
    {
      LongPressCnt++;
      return;
    }
  }
  ShortPressCnt++;
}


// The Arduino IDE Setup function -- called once upon reset
void setup()
{
  unsigned err;

  // Setup the Encoder pins to be inputs with pullups
  pinMode(Enc_A_PIN, INPUT);    // Use external 10K pullup and 100nf to gnd for debounce
  pinMode(Enc_B_PIN, INPUT);    // Use external 10K pullup and 100nf to gnd for debounce
  pinMode(Enc_PRESS_PIN, INPUT);// Use external 10K pullup and 100nf to gnd for debounce

  pinMode(LED1_PIN, OUTPUT);      
  pinMode(LED2_PIN, OUTPUT);     
  digitalWrite( LED1_PIN, HIGH); 
  digitalWrite( LED2_PIN, HIGH);

  // the i2c pins -- I2C mode will overwrite this 
  pinMode(18,INPUT);    // SDA
  pinMode(19,OUTPUT);   // SCL 
  digitalWrite( 18, LOW); 
  digitalWrite( 19, LOW);

  attachInterrupt(0, ISR_KnobTurn, FALLING);    // for the rotary encoder knob rotating
  attachInterrupt(1, ISR_ButtonPress, FALLING);    // for the rotary encoder knob push

#ifdef WITH_WIND 
  WindSetup() ;
#else ifdef WITH_RPM
  RPM_Setup();
#endif  

  wdt_enable(WDTO_2S);
 
  lcd.begin(LCD_COLS, LCD_ROWS);              // initialize the LCD columns and rows

  lcd.home ();                   // go home
  lcd.print("Baro-Hyg");
  lcd.setCursor ( 0, 1 );
  lcd.print("Display");

  // Serial.begin(57600);
  // Serial.print("Baro-Hyg-Temp-Wind Display\n");

  if ( (err = BMP085_init()) != 0)
  {
    lcd.setCursor ( 0, 0 );
    lcd.print("No Baro!");
    No_Baro = true;
  }
  else
    No_Baro = false;
  
  if ( (err = SI7021_init()) != 0)
  {
    lcd.setCursor ( 0, 1 );
    lcd.print("No Hygr!");
    No_Hygro = true;
  }
  else
    No_Hygro = false;

  if (No_Baro && No_Hygro)
  {
    if ( (err = TMP100_init()) != 0)
    {
      lcd.setCursor ( 0, 0 );
      lcd.print("NoTMP100");
      No_TMP100 = true;
    }
    else
    {
      No_TMP100 = false;
    }
  }

#ifndef WITH_WIND ||  WITH_RPM
  if ( No_Baro && No_Hygro && No_TMP100)
      while (1);          // Since there is nothing to measure let the watchdog catch it and reboot, Maybe a sensor gets plugged in soon.
 #endif
 
  BMP085_startMeasure( );    // initiate initial measure cycle
  SI7021_startMeasure( );    // initiate initial measure cycle
  TMP100_startMeasure( );

  EEPROM.get( 0, MetricDisplay);
  
  wdt_enable(WDTO_8S);  // set watchdog slower
  digitalWrite( LED1_PIN, LOW);   // Turn LEDs off
  digitalWrite( LED2_PIN, LOW);
}


// The Arduino IDE loop function -- Called contineously 
void loop()
{
  short adc_val;
  float Vbus_Volt;
  short rounded;
  float result ;
  static char PrevEncCnt = EncoderCnt;	// used to indicate that user turned knob
  static unsigned char PrevShortPressCnt = 0;
  static char p;
  float dewptC, TD_deltaC;
  float Temp_C;
  static bool REDledAlarm = false;
  static unsigned long t = millis();
 
 
  wdt_reset();

     
#ifdef WITH_WIND  
  WindRead();
#else ifdef WITH_RPM
  RPM_Read();
	
#endif
  BMP085_Read_Process();
  SI7021_Read_Process();
  TMP100_Read_Process();
  
 
  // anything to display ?
  if ( t + UPDATE_PER > millis() && EncoderCnt == PrevEncCnt && ShortPressCnt == PrevShortPressCnt)
    return;

  // update every n sec
  t = millis();

  adc_val = analogRead(VBUS_ADC);
  Vbus_Volt = adc_val * VBUS_ADC_BW;
 
 // Note: Temperatur can come from the Barometer or Hygrometer temp reading, but hyg has more resolution
  TD_deltaC = 99.0;         // init to huge value in case no Hygrometer present, 
  if (No_Hygro == false )
  {
    Temp_C = HygReading.TempC;
    dewptC = DewPt( Temp_C, HygReading.RelHum);
    TD_deltaC = Temp_C - dewptC;
  }
  else if (No_Baro == false )
	 Temp_C = BaroReading.TempC ;
  else if (No_TMP100 == false) 
	 Temp_C = TMP100_TempC;
  else
    Temp_C = -300.0 ;  // Impossible value, will never be displayed 
    
  
  // Alarms -- Turn the red light on and switch to the alarming display, TD-Alarm has priority over voltage
  if (Vbus_Volt < VOLT_LOW_ALARM  || Vbus_Volt > VOLT_HIGH_ALARM )
  {
     if (! REDledAlarm)
      EncoderCnt = V_Bus; 		// switch to the Alarming display once

      REDledAlarm = true;

  }

  if ( TD_deltaC < TD_DELTA_ALARM)
  {
      if (! REDledAlarm)
        EncoderCnt = TD_spread; 		// switch to the Alarming display once

      REDledAlarm = true;
  }


  // This alarm illuminates the blue LED, but doesn't lock the display to it
  if (Temp_C < FREEZE_ALARM )
    digitalWrite( LED2_PIN, HIGH);
  else
    digitalWrite( LED2_PIN, LOW);

  // Start of the individual readings display 
  lcd.home(  );  // Don't use LCD clear because of screen flicker

  re_eval:
// Limit this display to valid choices, wrap around if invalid choice is reached
  if (EncoderCnt < 0 ) 
        EncoderCnt = DISP_END-1;
  if (EncoderCnt >= DISP_END)
        EncoderCnt = 0; 
  
  switch (EncoderCnt)
  {
    case V_Bus:
      lcd.print("Voltage ");
      lcd.setCursor ( 0, 1 );
      lcd.print( Vbus_Volt ) ;
      lcd.print(" V  ");
      if (Vbus_Volt > VOLT_LOW_ALARM  && Vbus_Volt < VOLT_HIGH_ALARM )
      {
        REDledAlarm = false;
      }
      break;
      
    case D_Alt:
      if (No_Baro)
      {
        EncoderCnt += EncoderDirection;
        goto re_eval;
      }
      lcd.print("Dens Alt");
      lcd.setCursor ( 0, 1 );
      if (MetricDisplay)
      {
        rounded = DensityAlt(BaroReading.BaromhPa, BaroReading.TempC) + 0.5;
        lcd.print(rounded ) ; 
        lcd.print(" M     ");      
      }
      else
      {
        rounded = MtoFeet( DensityAlt(BaroReading.BaromhPa, BaroReading.TempC)) + 0.5;
        rounded /= 10;
        lcd.print(rounded * 10) ; // limit to 10ft resolution
        lcd.print(" ft    ");
      }
     
      break;

    case Alt:
      if (No_Baro)
      {
        EncoderCnt += EncoderDirection;
        goto re_eval;
      }
      
      if (LongPressCnt)   // entering setup
      {
        Alt_Setting_adjust( );
        LongPressCnt =0;
        lcd.home (  );
        EncoderCnt = Alt;    // restore the Alt display item
      }
      
      lcd.print(" Alt *  ");
      lcd.setCursor ( 0, 1 );

      if (MetricDisplay)
      {
        rounded = Altitude(BaroReading.BaromhPa, AltimeterSetting) + 0.5;
        lcd.print( rounded);
        lcd.print(" M     ");
      }
      else
      {
        rounded = MtoFeet(Altitude(BaroReading.BaromhPa, AltimeterSetting)) + 0.5;
        lcd.print( rounded);
        lcd.print(" ft    ");
      }
      break;

    case Station_P:
      if (No_Baro)
      {
        EncoderCnt += EncoderDirection;
        goto re_eval;
      }
      lcd.print("Pressure");
      lcd.setCursor ( 0, 1 );
      
      if (MetricDisplay)
      {
        lcd.print( BaroReading.BaromhPa );
        lcd.print("hPa");
      }
      else
      {
        lcd.print( hPaToInch(BaroReading.BaromhPa) );
        lcd.print("\"Hg");
      }
      break;

    case Rel_Hum:
      if (No_Hygro)
      {
        EncoderCnt += EncoderDirection;
        goto re_eval;
      }
      lcd.print("Humidity");
      lcd.setCursor ( 0, 1 );
      rounded = HygReading.RelHum  + 0.5;
      lcd.print( rounded  );
      lcd.print(" % RH  ");
      break;

    case Temp:
      if (No_Hygro && No_Baro && No_TMP100)
      {
        EncoderCnt += EncoderDirection;
        goto re_eval;
      }

      if (LongPressCnt)   // toggle between imperial and metric display
      {
        if ( MetricDisplay != 0 )
          MetricDisplay = 0;
        else
          MetricDisplay = 1; 
         
        EEPROM.put( 0, MetricDisplay);
        LongPressCnt =0;
      }
      
      lcd.print(" Temp * ");
      lcd.setCursor ( 0, 1 );
      if ( MetricDisplay)
      {
        lcd.print( Temp_C);
        lcd.print(char(223)); // degree symbol
        lcd.print("C   ");
      }
      else
      {
        lcd.print( CtoF( Temp_C) );
        lcd.print(char(223)); // degree symbol
        lcd.print("F   ");
      }
      break;

    case DewPoint:
      if (No_Hygro)
      {
        EncoderCnt += EncoderDirection;
        goto re_eval;
      }
      lcd.print("DewPoint");
      lcd.setCursor ( 0, 1 );
      
      if (MetricDisplay)
      {
       rounded = dewptC + 0.5;
       lcd.print( rounded );
       lcd.print(char(223)); // degree symbol
       lcd.print("C   ");
      }
      else
      {
       rounded = CtoF(dewptC) + 0.5;
       lcd.print( rounded );
       lcd.print(char(223)); // degree symbol
       lcd.print("F   ");
      }
      break;
      
#ifdef WetBulbTemp

    case  T_WetBulb:
    
      if (No_Hygro)
      {
        EncoderCnt += EncoderDirection;
        goto re_eval;
      }
      
      lcd.print("Wet Bulb");
      lcd.setCursor ( 0, 1 );
      if ( No_Baro)
        result = T_wetbulb_C(Temp_C,942.0,HygReading.RelHum); // in the absence of a Barometer reading I take the pressure at ~2000ft in standard Atmos.  
      else
        result = T_wetbulb_C(Temp_C,BaroReading.BaromhPa,HygReading.RelHum);
      if (MetricDisplay)
      {
        lcd.print( result);
        lcd.print(char(223)); // degree symbol
        lcd.print("C   ");
      }
      else
      {
        lcd.print( CtoF(result));
        lcd.print(char(223)); // degree symbol
        lcd.print("F   ");
      }
      break;
#endif      

    case TD_spread:
      if (No_Hygro)
      {
        EncoderCnt += EncoderDirection;
        goto re_eval;
      }
      lcd.print("TDspread");
      lcd.setCursor ( 0, 1 );
  
      if (MetricDisplay)
      {
        rounded = (Temp_C - dewptC) + 0.5; // for deg C
        lcd.print( rounded );
        lcd.print(char(223)); // degree symbol
        lcd.print("C     ");
      }
      else
      {
        rounded = (CtoF(Temp_C) - CtoF(dewptC)) + 0.5; // for deg F
        lcd.print( rounded );
        lcd.print(char(223)); // degree symbol
        lcd.print("F     ");
      }
      if ( TD_deltaC > TD_DELTA_ALARM)
        REDledAlarm = false;            // turn alarm off
      break;
      
#ifdef WITH_WIND
    case Wind_SPD:
      lcd.print("Wind SPD");
      lcd.setCursor ( 0, 1 );
      if (MetricDisplay)
      {
        lcd.print( WindSpdMPH * KMpMILE);
        lcd.print(" KMH   ");
      }
      else
      {
        lcd.print( WindSpdMPH);
        lcd.print(" MPH   ");
      }
      t -= UPDATE_PER;  // fastest readout 
      break;

    case Wind_GST:
      lcd.print("Wind GST");
      lcd.setCursor ( 0, 1 );
      if (MetricDisplay)
      {
        lcd.print( WindGustMPH * KMpMILE);
        lcd.print(" KMH   ");
      }
      else
      {
        lcd.print( WindGustMPH);
        lcd.print(" MPH   ");
      }
      break;

    case Wind_AVG:
      lcd.print("Wind AVG");
      lcd.setCursor ( 0, 1 );
      if ( MetricDisplay)
      {
        lcd.print( WindAvgMPH * KMpMILE);
        lcd.print(" KMH   ");
      }
      else
      {
        lcd.print( WindAvgMPH);
        lcd.print(" MPH   ");
      }
      break;

    case Wind_DIR:
      if  ( LongPressCnt) 
      {
          WindDirCal(  );   // blocking until completed
          lcd.home (  );
          LongPressCnt =0;
          EncoderCnt = Wind_DIR;    // restore the current display item
      }    
      
      lcd.print("Wnd DIR*");
      lcd.setCursor ( 0, 1 );
      lcd.print( WindDir);
      lcd.print(" ");
      lcd.print(char(223)); // degree symbol
      lcd.print("     ");
      t -= UPDATE_PER;  // fastest readout 
      
      break;
      
#endif    
#ifdef WITH_RPM
    case RPM:
      lcd.print("  RPM   ");
      lcd.setCursor ( 0, 1 );
      lcd.print( RPM_);
      lcd.print("      ");
	  break;
#endif
    default:
      // go in the same direction as last knob input from user and re-evalute again.
      // will wrap around in re_eval;
      EncoderCnt += EncoderDirection;
      goto re_eval;
      break;
  }

  BMP085_startMeasure(  );    // initiate an other measure cycle on the Barometer
  SI7021_startMeasure(  );    // initiate an other measure cycle on the Hygrometer
  TMP100_startMeasure(  );    // initiate an other measure cycle on the Stand alone Thermometer

  PrevEncCnt = EncoderCnt;
  PrevShortPressCnt = ShortPressCnt;

  // Blink the RED alarm led
  if (REDledAlarm)
  {
    if (digitalRead(LED1_PIN) )
      digitalWrite( LED1_PIN, LOW);
    else
      digitalWrite( LED1_PIN, HIGH);

  }
  else
    digitalWrite( LED1_PIN, LOW);

}

