/* 
 * File:   WindVane.h
 * Author: Gary Stofer
 *
 * Created on Nov 7, 2015, 9:35 AM
 */

#include "Arduino.h"
#include "build_opts.h"
#ifdef WITH_WIND
#ifndef WIND_VANE_H
#define	WIND_VANE_H
#define WIND_SAMPLE_PER 1000   // one second, this is the measure interval

#ifdef	__cplusplus
extern "C" {
#endif

extern unsigned char WindGustMPH;
extern unsigned char WindSpdMPH;
extern unsigned char WindAvgMPH;
extern long WindDir; 

extern void WindDirCal( void );
extern void WindSetup(void);
extern void WindRead(void);

#ifdef	__cplusplus
}
#endif
#endif
#endif	


