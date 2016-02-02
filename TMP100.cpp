/*
File:   TMP100.cpp
Author: Gary Stofer

Created on JAN 18, 2016, 5.25 PM
*/


/* Functions to initialzie and read the TMP100 temperatur sensor  */

#include "TMP100.h"

//////////////////////////////////////////// Humidity sensor SI 7021 //////////////////////

#define TMP100_ADDR 0x94		// This is with ADD0 strapped high and ADD1 pulled low	
#define TMP100_Temp_Reg 0x00
#define TMP100_Ctrl_Reg 0x01
#define TMP100_12BitConfig 0x60
#define TMP100_ShutdownBit 0x01
#define TMP100_OneShotBit 0x80

// Structure to read the on chip calibration values
enum _TMP100_SM {
	SM_START = 0,
	SM_Wait_Results,
	SM_Read_Results,
	SM_ERROR,
	SM_STOP,
	SM_IDLE,
	SM_NOTFOUND
};
static int ThisState = SM_NOTFOUND;

float TMP100_TempC;
static unsigned const char conf_reg = TMP100_12BitConfig | TMP100_ShutdownBit;

// See if the Device can be addressed -- Listen for the ACK on address
unsigned short
TMP100_init(void)
{
	unsigned short BusErr = 0;

	i2c_init();

	

	if ((BusErr = i2c_start( TMP100_ADDR + I2C_WRITE  )) != 0 )
	{
		ThisState = SM_NOTFOUND;
	}
	else
	{
		i2c_write( TMP100_Ctrl_Reg);			//Address the control register
		i2c_write( conf_reg );					// Set 12 bit ADC mode with sensor shutdown in between readings
    ThisState = SM_IDLE;
	}

	i2c_stop(); // and finish by transition into stop state
	return ( BusErr );     // i2c bus could not be opened, or device not attached

}


void
TMP100_startMeasure(void)
{
	if (ThisState == SM_IDLE)
	ThisState = SM_START;
}

void
TMP100_Read_Process(void )
{
	static unsigned long t;

	union {
		unsigned char  bytes[2];
		unsigned short val;
	} ADC_TEMP;

	switch (ThisState)
	{
	case SM_START: // initiate the Temp and RH  conversion
		if ( i2c_start( TMP100_ADDR + I2C_WRITE  ) != 0) // if the device fails to ack the address
		{
			ThisState = SM_ERROR;
			break;
		}

		i2c_write( TMP100_Ctrl_Reg);
		i2c_write( conf_reg | TMP100_OneShotBit );
		i2c_stop();
		t = millis();
		ThisState++;
		break;

	case SM_Wait_Results:   // The conversion should take 320ms at 12 bit res
		if ((millis() - t) > 400) // wait for the result to arrive
		{
			ThisState++;

		}
		break;

	case SM_Read_Results:
		if ( i2c_start( TMP100_ADDR + I2C_WRITE  ) != 0) // Check for ACK-- If conversion is not ready yet device repsonds with NACK
		{
			ThisState = SM_ERROR;
			break;
		}

		i2c_write( TMP100_Temp_Reg);			 // switch to the temp registor for the following reads
		i2c_rep_start( TMP100_ADDR + I2C_READ ); // restart in Read mode

		ADC_TEMP.bytes[1] = i2c_readAck(); // read Temp data msb
		ADC_TEMP.bytes[0] = i2c_readNak(); // read Temp data lsb
		i2c_stop();

		ADC_TEMP.val >>= 4;			// lower 4 bits are unused in 12 bit mode
		TMP100_TempC = ADC_TEMP.val * 0.0625;    // resolution in 12 bit mode


		ThisState = SM_IDLE;
		break;

	case SM_ERROR:
		TMP100_TempC = -303.0;   // impossible numbers
		i2c_stop();
		ThisState++;
		break;

	case SM_IDLE: // park here until someone starts the process again.
		break;

	case SM_NOTFOUND:   // initially not found -- never escapes from here.
		TMP100_TempC = -303.0;   // Equally impossible numbers

		break;
	}

}

