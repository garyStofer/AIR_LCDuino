/* 
 * File:   SI_7021.h
 * Author: Gary Stofer
 *
 * Created on December 18, 2013, 8:25 PM
 */

 
#include "Arduino.h"
#include "i2cmaster.h"

#ifndef SI_7021_H
#define	SI_7021_H

#ifdef	__cplusplus
extern "C" {
#endif

struct tag_HygReadings
{
    float TempC;
    float RelHum;
	float DewptC;
};

extern struct tag_HygReadings HygReading; 
extern unsigned short SI7021_init(void);
extern void SI7021_startMeasure(void);
extern void SI7021_Read_Process(void );


#ifdef	__cplusplus
}
#endif

#endif	/* SI_7021_H */


