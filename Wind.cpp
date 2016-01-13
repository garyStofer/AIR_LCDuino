#include "Wind.h"
#include <avr/wdt.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>

// The pin definitions are per obfuscated Arduino pin defines -- see aka for ATMEL pin names as found on the MEGA328P spec sheet

#define WIND_SPEED_PIN 16	// aka PC2 (ADC2)
#define WIND_DIR_ADC 6		// ADC6

#define WIND_GUST_PER ( 10 * 60000/ WIND_SAMPLE_PER )   // that is 10 minutes

#define ANEMO_CONST	(2.5)		// For Vortex/Inspeed wind cups 
#define ANEMO_COUNT_Rev	16	// For high fidelity opto interrupter pickup with 8 fingers

struct tagCalData
{
  int WDir_min;
  int WDir_max;
  int WDir_offs;
} WindCal = {70, 660, 0};

// array to keep 10 minutes of wind data for gust and average calculations
static unsigned char Wind_Gust[WIND_GUST_PER];
static volatile unsigned short WindCnt = 0;

// Globals for reporing the wind data
unsigned char WindGustMPH;
unsigned char WindSpdMPH;
unsigned char WindAvgMPH;
long WindDir;    // needs to be long for overflow protection in math

// Pin change interrupt to capture the edges of the wind speed interrupter
ISR(PCINT1_vect)
{
  // check PCINT1 interrupt flags for the wind_count pin if any other pin change interrupts are used in this code
  WindCnt++;  // count every edge from the wind sensor
}

void WindSetup()
{
  pinMode(WIND_SPEED_PIN, INPUT);    // the Windspeed count

  // enable the pin change interrupt for PC2, aka, A0, aka pin14
  // Note: this enabling of the pinchange interrupt pin has to go hand in hand with the chosen wind speed pin above

  PCMSK1 |= 1 << PCINT10 ; // enable PCinterupt10 , aka PC2
  PCICR |= 1 << PCIE1; // enable PCinterupt 1 vector

  // Read from eeprom
  // TODO: this needs to go further out in scope if the EEPROM is used to store other setup related items such as metric/imperial display etc
  EEPROM.get(2, WindCal);

}

extern unsigned char ShortPressCnt;
extern unsigned char EncoderCnt;        // decalared unsigned for this modules use
extern LiquidCrystal lcd;

void
WindDirCal( void )
{
  unsigned short adc_val;
  unsigned char p;
  unsigned off = 0;

  WindCal.WDir_min = 0xffff;
  WindCal.WDir_max = 0;

  ShortPressCnt = 0;


  while ( !ShortPressCnt )
  {
    adc_val = analogRead(WIND_DIR_ADC);// read the vane position

    if ( adc_val > WindCal.WDir_max)
      WindCal.WDir_max = adc_val;

    if (adc_val < WindCal.WDir_min)
      WindCal.WDir_min = adc_val;

    lcd.setCursor ( 0, 0 );
    lcd.print("Min ");
    lcd.print( WindCal.WDir_min);
    lcd.print("   ");
    lcd.setCursor ( 0, 1 );
    lcd.print("Max ");
    lcd.print(WindCal.WDir_max);
    lcd.print("   ");
    wdt_reset();
  }

  ShortPressCnt = 0;

  if ( WindCal.WDir_offs > 255 )
     off = 255;
  else
     off = 0;
   
  EncoderCnt = p = WindCal.WDir_offs - off;
  lcd.clear();
  lcd.print("Offset N");

  while ( !ShortPressCnt )
  {
    wdt_reset();

    if (  EncoderCnt == 0 && p == 255)    // to continue past 255
      off = 255;

    if (  EncoderCnt == 255 && p == 0)   // as it comes below 255
    { if (off != 0)
        off = 0;
      else
        EncoderCnt++;       // lock at 0

    }
    if (off + EncoderCnt >= 360 )
      EncoderCnt--;       // lock at 359

    p = EncoderCnt;

    WindCal.WDir_offs = off + EncoderCnt;
    lcd.setCursor ( 0, 1 );
    lcd.print(WindCal.WDir_offs);
    lcd.print(" ");
    lcd.print(char(223)); // degree symbol
    lcd.print("     ");   // fill to end of line



  }
  // Store min & max in EEprom
  // TODO: this needs to go further out in scope if the EEPROM is used to store other setup related items such as metric/imperial display etc
  EEPROM.put(2, WindCal);
}


void WindRead()
{
  unsigned short adc_val;
  unsigned short wind_count;
  unsigned short n;
  float wind_speed;
  unsigned long t_now = 0;
  unsigned long t_sample = 0;
  static unsigned long t_next = 0;
  static unsigned short GustNdx = 0;


  if ( (t_now = millis()) < t_next) // wait until next update period
  {
    // not time yet
    return;
  }


  // disable the interrupt so we can get a two byte variable without getting interrupted
  PCICR = 0;  // disable PCinterupt 1 vector
  wind_count = WindCnt;     // get the count from the pin change interrupt
  WindCnt = 0;
  t_sample = WIND_SAMPLE_PER + (t_now - t_next);
  t_next = t_now + WIND_SAMPLE_PER; // every second we update the display with new data
  PCICR |= 1 << PCIE1; // enable PCinterupt 1 vector

  // calc and store the current wind speed
  wind_speed = (wind_count * ANEMO_CONST) / ANEMO_COUNT_Rev;   // 2.5 miles/rev/sec; div by counts per revolution.
  // correct for actual sampling periode
  wind_speed = wind_speed * WIND_SAMPLE_PER / t_sample;
  WindSpdMPH = wind_speed + 0.5; \

  Wind_Gust[GustNdx] = WindSpdMPH; // store current measure wind speed in uchar, use rounding.
  GustNdx = ++GustNdx % WIND_GUST_PER;  // Advance to next position, wrap around

  // go through the 1 second array and take the peak and average  for the last 10 minutes.
  // note average will not be ready for first 10 minuntes after turning the device on

  WindGustMPH  = wind_speed = 0;
  for ( unsigned short i = 0; i < WIND_GUST_PER; i++)
  {
    wind_speed += Wind_Gust[i];

    if (Wind_Gust[i] > WindGustMPH)
      WindGustMPH = Wind_Gust[i] ;    // this is the highest in the last 10 minutes
  }

  wind_speed /= WIND_GUST_PER;
  WindAvgMPH = wind_speed + 0.5;



  // calc and store the current wind dir
  adc_val = analogRead(WIND_DIR_ADC);    // read the input pin

  WindDir = ((adc_val - WindCal.WDir_min) *  360L) / (WindCal.WDir_max - WindCal.WDir_min);
  WindDir += WindCal.WDir_offs;

  WindDir = WindDir %360;    // to make the offset  wrap around 



}
