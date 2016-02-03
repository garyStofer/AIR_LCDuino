#include "RPM.h"

#ifdef WITH_RPM
// The pin definitions are per obfuscated Arduino pin defines -- see aka for ATMEL pin names as found on the MEGA328P spec sheet

#define RPM_PIN 16	// aka PC2 (ADC2)

static volatile unsigned short RPM_Cnt = 0;		// counting variable -- gets incremented on each edge of the rpm sensor , each revolution
short int RPM_; 
// Pin change interrupt to capture the edges of the wind speed interrupter
ISR(PCINT1_vect)
{
  // check PCINT1 interrupt flags for the wind_count pin if any other pin change interrupts are used in this code
  RPM_Cnt++;  // count every edge from the wind sensor
}

void RPM_Setup()
{
  pinMode(RPM_PIN, INPUT);    // the Windspeed count

  // enable the pin change interrupt for PC2, aka, A0, aka pin14
  // Note: this enabling of the pinchange interrupt pin has to go hand in hand with the chosen wind speed pin above

  PCMSK1 |= 1 << PCINT10 ; // enable PCinterupt10 , aka PC2
  PCICR |= 1 << PCIE1; // enable PCinterupt 1 vector
  RPM_ = 0;
}


void RPM_Read()
{
  unsigned long t_now = 0;
  unsigned long t_sample = 0;
  static unsigned long t_next = 0;

  if ( (t_now = millis()) < t_next) // wait until next update period
  {
    // not time yet
    return;
  }


  // disable the interrupt so we can get a two byte variable without getting interrupted
  PCICR = 0;  // disable PCinterupt 1 vector
  RPM_ = RPM_Cnt* 30;     // get the count from the pin change interrupt and compute rpm -- interrupt gets both edges, so only multiply by half
  RPM_Cnt = 0;
  t_next = t_now + SAMPLE_PER; // every second we update the display with new data
  PCICR |= 1 << PCIE1; // enable PCinterupt 1 vector

}
#endif