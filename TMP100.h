/* 
 * File:   TMP100.h
 * Author: Gary Stofer
 *
 * Created on JAN 18, 2016, 5.25 PM
 */

 
#include "Arduino.h"
#include "i2cmaster.h"

#ifndef TMP100_H
#define	TMP100_H

#ifdef	__cplusplus
extern "C" {
#endif

extern float TMP100_TempC; 
extern unsigned short TMP100_init(void);
extern void TMP100_startMeasure(void);
extern void TMP100_Read_Process(void );


#ifdef	__cplusplus
}
#endif

#endif	


