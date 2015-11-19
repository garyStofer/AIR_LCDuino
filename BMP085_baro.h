/* 
 * File:   BMP085_baro.h
 * Author: Gary Stofer
 *
 * Created on May 7, 2013, 9:35 AM
 */

#include "Arduino.h"
#include "i2cmaster.h"
#include "Atmos.h"


#ifndef BMP085_BARO_H
#define	BMP085_BARO_H

#define hPaToInch( hPa ) (hPa  * 0.02952998) // Conversion factor to inches Hg at 32F -- number and precision from Wikipedia 
#define CtoF( tC ) ( tC / 0.5555555555 +32)
#define MtoFeet( meters) (meters *3.28084)

#ifdef	__cplusplus
extern "C" {
#endif

struct tag_baroReadings
{
    float TempC;
    float BaromhPa;
};

extern  struct tag_baroReadings BaroReading;
extern  float AltimeterSetting;


extern unsigned  BMP085_init(void);
extern void BMP085_startMeasure( void );
extern void BMP085_Read_Process(void );
extern void Alt_Setting_adjust( void );

#ifdef	__cplusplus
}
#endif

#endif	/* BMP085_BARO_H */


