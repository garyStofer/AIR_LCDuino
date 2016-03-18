/* 
 * File:   RPM.h
 * Author: Gary Stofer
 *
 * Created on Feb 2, 2016, 19:35
 */

#include "Arduino.h"
#include "build_opts.h"
#ifdef WITH_RPM


#ifndef RMP_H
#define	RPM_H
#define SAMPLE_PER 1000   // one second, this is the measure interval

#ifdef	__cplusplus
extern "C" {
#endif

extern short int RPM_; 

extern void RPM_Setup(void);
extern void RPM_Read(void);

#ifdef	__cplusplus
}
#endif
#endif
#endif	


