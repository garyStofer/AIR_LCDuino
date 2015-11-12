// Sketch to read Bosh BMP085/ BMP180 and SI hygrometer and display various aviation relatred readings on 8x2 lcd display

// Warning : This sketch needs to be compiled and run on a device with Optiboot because watchdog doesn't work under std BL
// Last compiled and tested with Arduino IDE 1.6.6

#include <LiquidCrystal.h>
#include <avr/wdt.h>
#include "BMP085_baro.h"
#include "SI_7021.h"
#include "Atmos.h"
#include "Wind.h"

// uncomment for Anemometer and Vane readout
#define WITH_WIND 

#define LCD_COLS 8
#define LCD_ROWS 2

//With a TD delta of less that 3degC (~5degF) you can expect fog, so therefore I set the default for the alarm a bit higher than that.
#define TD_DELTA_ALARM	4.0 // in degC when the alarm is to come on
#define VOLT_LOW_ALARM 11.0
#define VOLT_HIGH_ALARM 15.0
#define FREEZE_ALARM 1.0


// The pin definitions are per obfuscated Arduino pin defines -- see aka for ATMEL pin names as found on the MEGA328P spec sheet
#define Enc_A_PIN 2     // This generates the interrupt, providing the click  aka PD2 (Int0)
#define Enc_B_PIN 14    // This is providing the direction aka PC0,A0
#define Enc_PRESS_PIN 3	// aka PD3 ((Int1)
#define LED1_PIN 10			// aka PB2
#define LED2_PIN 17			// aka PC3,A3
#define VBUS_ADC 7			// ADC7
#define VBUS_ADC_BW  (5.0*(14+6.8)/(1024*6.8))		//adc bit weight for voltage divider 14.0K and 6.8k to gnd.


#define UPDATE_PER 2000   // in ms second, this is the measure interval and also the LCD update interval

#define LONGPRESS_TIMEOUT 100000

enum Displays {
  V_Bus = 0,
  D_Alt,
  Alt,
  Station_P,
  Rel_Hum,
  Temp,
  DewPoint,
  TD_spread,
#ifdef WITH_WIND
  Wind_DIR,
  Wind_SPD,
  Wind_AVG,
  Wind_GST,
#endif
 };

char EncoderCnt = D_Alt;    // Default startup display
char EncoderPressedCnt = 0;
unsigned char ShortPressCnt = 0;
unsigned char LongPressCnt = 0;

static float AltimeterSetting = STD_ALT_SETTING;

LiquidCrystal lcd(9, 8, 6, 7, 4, 5); // in 4 bit interface mode


/* Note regarding contrast control */
// Contrast contrrol simply using a PWM doesn't work because the PWM needs to be filtered with a R-C, but the LCD itself is pulling the LCD control
// input up, so the R-C from the MCU would have to be relatively low in impendance <300ohms, making for a big capacitor and lots of current
// Would have to feed the filtered voltage into an OP-Amp to drive the LCD input properly.
// For now the hardware has a resistor pair that sets the contrast .
/*


  This ISR handles the reading of a quadrature encoder knob and in- or decrements  two global encoder count  variables
  depending on the direction of the encoder and the state of the button. The encoder functions at 1/4 the maximal possible resolution and generally provides one inc/dec
  per mechanical detent.

  One of the quadrature inputs is used to trigger this interrupt handler, which checks the other quadrature input to decide the direction.
  It is assumed that the direction  quadrature signal is not bouncing while
  the first phase is causing the initial interrupt as it's signal is 90deg opposed.

  Two encoder count variables are being modified by this ISR
    A: EncoderCnt when the knob is turned without the button being press
    B: EncoderPressCnt when the knob is  turned while the button is also pressed. This happens only after a LONG press timeout
*/
void ISR_KnobTurn( void)
{
  if ( digitalRead( Enc_PRESS_PIN ) )
  {
    if ( digitalRead( Enc_B_PIN ) )
      EncoderCnt++;
    else
      EncoderCnt--;

  }
  else
  {
    if ( digitalRead( Enc_B_PIN ) )
      EncoderPressedCnt++;
    else
      EncoderPressedCnt--;
  }

}


/*
  ISR to handle the button press interrupt. Two modes of button presses are recognized. A short, momentary press and a long, timing out press.
   While the hardware de bounced button signal is sampled for up to TIMEOUT time in this ISR no other code is being executed. If the time-out occurs
   a long button press-, otherwise a short button press is registered. Timing has to be done by a software counter since interrupts are disabled and function millis() and micros()
   don't advance in time. Software timing is of course CPU clock dependent an d therefore has to be adjusted to the clock frequency if it is not running at 16Mhz.
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


void setup()
{
  unsigned err;

  // Setup the Encoder pins to be inputs with pullups
  pinMode(Enc_A_PIN, INPUT);    // Use external 10K pullup and 100nf to gnd for debounce
  pinMode(Enc_B_PIN, INPUT);    // Use external 10K pullup and 100nf to gnd for debounce
  pinMode(Enc_PRESS_PIN, INPUT);// Use external 10K pullup and 100nf to gnd for debounce

  pinMode(LED1_PIN, OUTPUT);      // the LED
  pinMode(LED2_PIN, OUTPUT);      // the LED

  attachInterrupt(0, ISR_KnobTurn, FALLING);    // for the rotary encoder knob rotating
  attachInterrupt(1, ISR_ButtonPress, FALLING);    // for the rotary encoder knob push

#ifdef WITH_WIND 
  WindSetup() ;
#endif  
  wdt_enable(WDTO_2S);

  lcd.begin(LCD_COLS, LCD_ROWS);              // initialize the LCD columns and rows

  lcd.home ();                   // go home
  lcd.print("Baro-Hyg");
  lcd.setCursor ( 0, 1 );
  lcd.print("Display");

  // Serial.begin(57600);
  // Serial.print("Baro-Hyg Display\n");

  if ( (err = BMP085_init()) != 0)
  {
    lcd.clear ();
    lcd.print("No Baro!");
    while (1);          // let the watchdog catch it and reboot.
  }
  else
    BMP085_startMeasure( );    // initiate an other measure cycle


  if ( (err = SI7021_init()) != 0)
  {
    lcd.setCursor ( 0, 1 );
    lcd.print("No Hygr!");
    while (1);          // let the watchdog catch it and reboot.
  }
  else
    SI7021_startMeasure(  );    // initiate an other measure cycle

  wdt_enable(WDTO_8S);  // set slower

}



void loop()
{
  short adc_val;
  float Vbus_Volt;
  short rounded;
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
#endif
  BMP085_Read_Process();
  SI7021_Read_Process();

  if ( t + UPDATE_PER > millis() && EncoderCnt == PrevEncCnt && ShortPressCnt == PrevShortPressCnt)
    return;

  // update every n sec
  t = millis();


  adc_val = analogRead(VBUS_ADC);
  Vbus_Volt = adc_val * VBUS_ADC_BW;
  Temp_C = HygReading.TempC ;				// Note: This can come from the Barometer or Hygrometer temp reading, but hyg has more resolution
  dewptC = DewPt( Temp_C, HygReading.RelHum);
  TD_deltaC = Temp_C - dewptC;


  // Alarms -- Turn the red light on and switch to the pertinent display, TD-Alarm has priority over voltage
  if (Vbus_Volt < VOLT_LOW_ALARM  || Vbus_Volt > VOLT_HIGH_ALARM )
  {
    if (!REDledAlarm)
      EncoderCnt = V_Bus; 		// switch to the Alarming display

    REDledAlarm = true;

  }

  if ( TD_deltaC < TD_DELTA_ALARM)
  {
    if (!REDledAlarm)
      EncoderCnt = TD_spread; 		// switch to the Alarming display

    REDledAlarm = true;
  }


  // This alarm illuminates the blue LED, but doesn't lock the display to it
  if (Temp_C < FREEZE_ALARM )
    digitalWrite( LED2_PIN, HIGH);
  else
    digitalWrite( LED2_PIN, LOW);

  lcd.home(  );  // Don't use LCD clear because of sreen flicker



  switch (EncoderCnt)
  {
    case D_Alt:
      lcd.print("Dens Alt");
      lcd.setCursor ( 0, 1 );
      rounded = MtoFeet( DensityAlt(BaroReading.BaromhPa, BaroReading.TempC)) + 0.5;
      rounded /= 10;
      lcd.print(rounded * 10) ; // limit to 10ft resolution
      lcd.print(" ft    ");
      break;

    case Alt:
      if (LongPressCnt)   // entering setup
      {
        unsigned char sp = ShortPressCnt;
        p = EncoderCnt;
        while ( ShortPressCnt == sp )	// set QNH
        {
          if (EncoderCnt != p)
            wdt_reset();
  
          if (EncoderCnt > p)
            AltimeterSetting += 0.25;
          else if (EncoderCnt < p)
            AltimeterSetting -= 0.25;
  
          p = EncoderCnt;
  
          lcd.setCursor ( 0, 0 );
          lcd.print("Set QNH ");
          lcd.setCursor ( 0, 1 );
          lcd.print( hPaToInch(AltimeterSetting) );
          lcd.print("\"Hg");
  
        }
        LongPressCnt =0;
      }
      lcd.print(" Alt *  ");
      lcd.setCursor ( 0, 1 );
      rounded = MtoFeet(Altitude(BaroReading.BaromhPa, AltimeterSetting)) + 0.5;
      lcd.print( rounded);
      lcd.print(" ft    ");
      EncoderCnt = Alt;		// restore the Alt display item
      break;

    case Station_P:
      lcd.print("Pressure");
      lcd.setCursor ( 0, 1 );
      lcd.print( hPaToInch(BaroReading.BaromhPa) );
      lcd.print("\"Hg");

      break;

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

    case Rel_Hum:
      lcd.print("Humidity");
      lcd.setCursor ( 0, 1 );
      rounded = HygReading.RelHum  + 0.5;
      lcd.print( rounded  );
      lcd.print(" % RH  ");
      break;

    case Temp:
      lcd.print("  Temp  ");
      lcd.setCursor ( 0, 1 );
      lcd.print( CtoF( Temp_C) );
      lcd.print(char(223)); // degree symbol
      lcd.print("F   ");
      break;

    case DewPoint:
      lcd.print("DewPoint");
      lcd.setCursor ( 0, 1 );
      rounded = CtoF(dewptC) + 0.5;
      lcd.print( rounded );
      lcd.print(char(223)); // degree symbol
      lcd.print("F   ");
      break;

    case TD_spread:
      lcd.print("TDspread");
      lcd.setCursor ( 0, 1 );
      // rounded = CtoF( TD_deltaC )+0.5;  // for deg C
      rounded = (CtoF(Temp_C) - CtoF(dewptC)) + 0.5; // for deg F
      lcd.print( rounded );
      lcd.print(char(223)); // degree symbol
      lcd.print("F   ");
      if ( TD_deltaC > TD_DELTA_ALARM)
        REDledAlarm = false;
      break;
#ifdef WITH_WIND
    case Wind_SPD:
      lcd.print("Wind SPD");
      lcd.setCursor ( 0, 1 );
      lcd.print( WindSpdMPH);
      lcd.print(" MPH   ");
      t -= UPDATE_PER;  // fastest readout 
      break;

    case Wind_GST:
      lcd.print("Wind GST");
      lcd.setCursor ( 0, 1 );
      lcd.print( WindGustMPH);
      lcd.print(" MPH   ");
      break;

    case Wind_AVG:
      lcd.print("Wind AVG");
      lcd.setCursor ( 0, 1 );
      lcd.print( WindAvgMPH);
      lcd.print(" MPH   ");
      break;

    case Wind_DIR:
      if  ( LongPressCnt) 
      {
          WindDirCal(  );   // blocking until completed
          lcd.setCursor ( 0, 0 );
          LongPressCnt =0;
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
    default:
      // Limit this display to valid choices
      if ( EncoderCnt < 0 )
        EncoderCnt = 0;
      else
        EncoderCnt--;


  }

  BMP085_startMeasure(  );    // initiate an other measure cycle
  SI7021_startMeasure(  );    // initiate an other measure cycle

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

