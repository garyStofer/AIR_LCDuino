/* 
 * File:   SI_7021.cpp
 * Author: Gary Stofer
 *
 * Created on December 18, 2013, 8:25 PM
 */

/* Functions to initialzie and read the SI 7021 Relative humidity sensor device */

#include "SI_7021.h"

//////////////////////////////////////////// Humidity sensor SI 7021 //////////////////////

#define SI7021_ADDR (0x40<<1)
#define SI7021_RESET_CMD    0xFE
#define SI7021_Convert_CMD 0xF5        // Starts a temperature and RH conversion cycle, but does not hold CLK line until complete
#define SI7021_ReadPrevTemp_CMD 0xE0     // Read out the temperature reading from the previous conversion
#define SI7021_WriteUserRegister 0xE6    // used to turn the heater on and set ADC resolution
#define SI7021_ReadUserRegister  0xE7

// Structure to read the on chip calibration values
enum _SI7021_READ_SM {
    SM_START = 0,
    SM_Wait_Results,
    SM_Read_Results,
    SM_ERROR,
    SM_STOP,
    SM_IDLE,
	SM_NOTFOUND
};
static int ThisState = SM_NOTFOUND;

struct tag_HygReadings HygReading;

// See if the Device can be addressed -- Listen for the ACK on address
unsigned short
SI7021_init(void)
{
    unsigned short BusErr = 0;
	 
	i2c_init();
	 
	if ((BusErr = i2c_start( SI7021_ADDR +I2C_WRITE  )) !=0 )
  {
      ThisState = SM_NOTFOUND;
  } 
  else
    ThisState = SM_IDLE;
  
	 
	i2c_stop(); // and finish by transition into stop state
	return ( BusErr );     // i2c bus could not be opened, or device not attached
	 
}


void
SI7021_startMeasure(void)
{
    if(ThisState == SM_IDLE)
        ThisState = SM_START;
}

void
SI7021_Read_Process(void )
{
    static unsigned long t;
	
    union {
        unsigned char bytes[2];
        unsigned short val;
    } ADC_RH;

    union {
        unsigned char  bytes[2];
        unsigned short val;
    } ADC_TEMP;

    switch (ThisState)
    {
        case SM_START: // initiate the Temp and RH  conversion
			      if ( i2c_start( SI7021_ADDR +I2C_WRITE  ) != 0) // if the device fails to ack the address
            {
                ThisState = SM_ERROR;
                break;
            }
			      i2c_write( SI7021_Convert_CMD); 	// internal register address
            i2c_stop();
            t = millis();
            ThisState++;
            break;

        case SM_Wait_Results:   // The conversion should take a maximum of 20.4 ms
            if ((millis() - t) > 30) // wait for the result to arrive
                ThisState++;
            break;

        case SM_Read_Results:
			      if ( i2c_start( SI7021_ADDR +I2C_READ  ) != 0) // Check for ACK-- If conversion is not ready yet device repsonds with NACK
            {
                ThisState = SM_ERROR;
                break;
            }

            ADC_RH.bytes[1] = i2c_readAck();// read rh data msb
            ADC_RH.bytes[0] = i2c_readNak(); // read rh data lsb
            i2c_stop();

            // Collect the temperature data from the last conversion
			      if ( i2c_start( SI7021_ADDR + I2C_WRITE  ) != 0) // if the device fails to ack the address
            {
                ThisState = SM_ERROR;
                break;
            }
       		  i2c_write( SI7021_ReadPrevTemp_CMD); 	// internal register address
			      i2c_rep_start( SI7021_ADDR +I2C_READ ); // restart in Read mode now
            ADC_TEMP.bytes[1] = i2c_readAck(); // read Temp data msb
            ADC_TEMP.bytes[0] = i2c_readNak(); // read Temp data lsb
            i2c_stop();

            HygReading.TempC = (ADC_TEMP.val*175.72/65536) -46.85;    // Magic numbers from SI datasheet  
            HygReading.RelHum = (ADC_RH.val*125.0/65536)-6.0;         // Magic numbers from SI datasheet
    
            ThisState = SM_IDLE;
            break;

        case SM_ERROR:
            HygReading.TempC = -302.0;   // impossible numbers
            HygReading.RelHum = 1.0; 
			      i2c_stop();
            ThisState++;
            break;

        case SM_IDLE: // park here until someone starts the process again.
            break;
			
		case SM_NOTFOUND:   // initially not found -- never escapes from here.
            HygReading.TempC = -302.0;   // Equally impossible numbers
            HygReading.RelHum = 2.0; 
            break;   
    }

}

