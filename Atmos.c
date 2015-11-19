/* 
 * File:   Atmos.c
 * Author: Gary Stofer
 *
 * Created on December 18, 2013, 8:25 PM
 */

/* Function to calculate standard atmospheric items such as Pressure Altitude, Density Altitude, Dewpoint etc.. */
#include "math.h"
#include "Atmos.h"


#define Tsl (288.15)		  // standard atmosphere temp in Kelvin at sea level
#define Psl STD_ALT_SETTING	  // standard atmosphere pressure at sea level, aka QNE
#define LapsR (0.0065)		// standard atmosphere lapse rate
#define pwr_da (0.234969245)

  
 /*	 Calculate DewPoint from Temp and humidity

            Simple approximation formula: (not used)
            Td = Temp_c -(100-RH)/5
            tmp = (100 - tmp)/5;
            tmp = Temp_c - tmp;
            T_dewptF  = (tmp*9/5+32)*10;		// convert to 1/10 deg F


            Close approximation formula:    (used)
            This expression is based on the August?Roche?Magnus approximation for the saturation vapor
            pressure of water in air as a function of temperature. It is considered valid for
            0 ï¿½C < T < 60 ï¿½C
            1% < RH < 100%
            0 ï¿½C < Td < 50 ï¿½C

             Formula:
             a = 17.271
             b = 237.7 ï¿½C
             Tc = Temp in Celsius
             Td = Dew point in Celsius
             RH = Relative Humidity
             Y = (a*Tc /(b+Tc)) + ln(RH/100)
             Td = b * Y / (a - Y)
*/
 
 float 
 DewPt( float Tc, float RH )
 {
	float DewPtC;
    float Y;

	  // SI Hygro sends values >100% for condensing, Limit RH for all cases
	  if (RH >100 )
		  RH = 100;

     Y = ((17.271 * Tc) / (237.7+Tc ))+ log(RH/100);
     DewPtC = 237.7 * Y/(17.271-Y);
       
	  return DewPtC;

}
// Calculate actual altitude in meters if actual sea level pressure is known (Altimeter setting, aka QNH)
float 
Altitude ( float P_hPa, float P_sl)
{
	//Formula and precision from NOAA
	float P_alt = (1.0 - pow((P_hPa /P_sl),0.190284)) * 44307.7;
	return P_alt;
} 


// Calculate pressure altitude in meters from pressure in hecto Pascal (i.e. Millibar)
float
PressureAlt( float P_hPa)
{
	return Altitude(P_hPa, Psl);
} 


// Calculate Density altitude in meters from Temperature_c and static pressure in hPa
// Note: this is dry-airmass Da

float 
DensityAlt( float P_hPa, float Temp_C)
{
	float T = Temp_C + 273.15;
	
	float D_alt = (Tsl/LapsR) * ( 1- pow( ( (P_hPa/Psl) / (T/Tsl) ),pwr_da));
	
	return D_alt;
}

// Calculate WetBulb temp from Temp_C, Stationpressure_hPa and relative humidity.
// Formula from   https://www.easycalculation.com/weather/learn-dewpoint-wetbulb.php

float
T_wetbulb_C(float Temp_C, float Press_hPa, float Rh)
{
    float Twb = 0;                 // temp Wetbulb 
    float incr = 10;               // initial approach increment 
    short cursign,previoussign = 1;
    float e_diff = 1;              // Difference in vapor press the itterative code is driving to 0
    float e_guess;                 // The current itterations Vapor pressure calculated from the current itterations Wetbulb temp
 
    const float es = 6.112 * exp((17.67 * Temp_C) / (Temp_C + 243.5));    // Saturated Vapor Pressure
    const float e  = es * Rh /100.0;                                      // Vapor pressure 
    
   // es =  6.112 * exp(17.67 * Temp_C / (Temp_C + 243.5)); // Saturated Vapor pressure 
    // e = es * Rh /100.0;

    // itterative loop driving e_diff to 0 by calculating a wetbulb vapor pressure e_guess from an incremental Twb --
    // initially aproaching 0 by using 10 deg incrementgs in assumed Twb, then switching to 1/10 of that each time the 
    // sign changes until we are at 0 or close enough (0.005)
    
    while (abs(e_diff) > 0.005) 
    {
      e_guess = 6.112 * exp((17.67 * Twb) / (Twb + 243.5));
      e_guess = e_guess - Press_hPa * (Temp_C - Twb) * 0.00066 * (1 + (0.00115 * Twb));
      e_diff = e - e_guess; 
      
      if (e_diff == 0)
          break;        // we hit the target right on -- it's not going to get any better than this -- exit at once
  
      if (e_diff < 0)
        cursign = -1;
      else
        cursign = 1;
                          
      if (cursign != previoussign)    // when sign changes use aproach speed of 1/10 each time.
      {
              previoussign = cursign;
              incr = incr/10;
      }
      else
        incr = incr;                // obviously redundant but left here for clarity of code
              
     Twb = Twb + incr * previoussign; 
            
    }
   
    return Twb;
}

