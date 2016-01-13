#include "BMP085_baro.h"
#include <avr/wdt.h>
#include <LiquidCrystal.h>

//////////////////////////////////////////// BMP805 sensor  //////////////////////
#define BMP085_I2C_Addr 0xEE

#define BMP085_EEprom 0xAA
#define BMP085_Ctrl_Reg 0xF4
#define BMP085_ReadADC 0xF6
#define BMP085_ConvTemp 0x2E	// 16 bits, conversion time 4.5ms
//#define BMP085_ConvPress 0x34   // 16 bits 1 internal sample 4.5Ms
#define BMP085_ConvPress 0xF4   // 16 bits 8 internal samples 25.5ms

// Structure to read the on chip calibration values
union {
	unsigned short dat[11];
	struct tag_Coeff{
			short   AC1,AC2,AC3;
			unsigned short AC4,AC5,AC6;
			short	B_1;
			short B2;
			short 	MB, MC,MD;
		} Coeff;
}BMP085_Cal;

struct tag_baroReadings BaroReading;
float AltimeterSetting = STD_ALT_SETTING;

enum _BMP085_READ_SM {
    SM_START = 0,
    SM_Wait_for_Temp,
    SM_Read_Temp,
    SM_Wait_for_Press,
    SM_Read_Press,
    SM_Calc_Press,
    SM_ERROR,
    SM_IDLE,
    SM_NOTFOUND
};



// the state machine var
static  int ThisState = SM_NOTFOUND;

unsigned 
BMP085_init()
{
    byte i;
    unsigned short BusErr =0;

    delay(11);        // Device has a 10 ms startup delay 
    i2c_init();
    
    if ((BusErr = i2c_start( BMP085_I2C_Addr +I2C_WRITE  )) !=0 )
    {
        ThisState = SM_NOTFOUND;
        i2c_stop(); // and finish by transition into stop state
        return ( BusErr );     // i2c bus could not be opened, or device not attached
    }
    else
      ThisState = SM_IDLE;
    

    if (	i2c_write( BMP085_EEprom) == 0) 	// internal register address for calibration coefficients
    {
        i2c_rep_start( BMP085_I2C_Addr +I2C_READ ); // restart in Read mode now

        // Gather the 10 cal values
      	for (i=0; i <= 10; i++)
  	    {
              BMP085_Cal.dat[i]=(i2c_readAck() << 8);
              BMP085_Cal.dat[i]+=i2c_readAck();
  	    }
      	i2c_readNak();   // read one more byte to send a Nack 
    }
    else
    {
          ThisState = SM_NOTFOUND;
          BusErr = 3; // The device failed to acknowledged an address cycle on the EEPROM address
    }
    i2c_stop(); // and finish by transition into stop state

    return BusErr ;

}


void
BMP085_startMeasure( void )
{
    if ( ThisState == SM_IDLE )
        ThisState = SM_START;
}
void
BMP085_Read_Process(void )
{
    static long T;
    static long X1,X2,X3;
    static long B5;
    static long  Up;
    static unsigned long t;
    static boolean bus_err = 0;        // in case the device becomes unresponsive - i.e. unplugged

    switch (ThisState)
    {
        case SM_START:
            // initiate the temperature Reading
            if ( i2c_start( BMP085_I2C_Addr +I2C_WRITE  ) != 0) // if the device fails to ack the address
            {
                ThisState = SM_ERROR;
                break;
            }

            i2c_write( BMP085_Ctrl_Reg); 	// internal register address
            i2c_write( BMP085_ConvTemp); 	// internal register address
            i2c_stop();

            t = millis();
            ThisState++;
            break;

        case SM_Wait_for_Temp:
            if (millis() - t > 5 ) // wait for 5 ms for the result to arrive
                ThisState++;
            break;

        case SM_Read_Temp:
        {   
            long Ut;
     
            // get Temp result now
            if ( i2c_start( BMP085_I2C_Addr +I2C_WRITE  ) != 0) // if the device fails to ack the address
            {
                ThisState = SM_ERROR;
                break;
            }
            i2c_write( BMP085_ReadADC); 	// internal command register address
            i2c_rep_start( BMP085_I2C_Addr +I2C_READ ); // restart in Read mode now

            Ut = (unsigned short)(i2c_readAck() << 8);
            Ut+=i2c_readNak();
            
            i2c_stop();

            X1 =(Ut - BMP085_Cal.Coeff.AC6) * BMP085_Cal.Coeff.AC5 / 32768L;
            X2 = BMP085_Cal.Coeff.MC * 2048L /(X1 + BMP085_Cal.Coeff.MD);
            B5 = X1+X2;
            T = (B5+8)/16;

            BaroReading.TempC = T/10.0;
           
            // initiate the Pressure reading
            if ( i2c_start( BMP085_I2C_Addr +I2C_WRITE  ) != 0) // if the device fails to ack the address
            {
                ThisState = SM_ERROR;
                break;
            }
                  
            i2c_write( BMP085_Ctrl_Reg); 	// internal command register address
            i2c_write( BMP085_ConvPress); 	// Command
            i2c_stop();

            t = millis();
            ThisState++;
            break;
        }
        case SM_Wait_for_Press:
            // wait for 40ms in this state
            if (millis() - t > 40 ) // wait for the result to arrive
                ThisState++;
            break;

        case SM_Read_Press:      // get Pressure result now
        {
            if ( i2c_start( BMP085_I2C_Addr +I2C_WRITE  ) != 0) // if the device fails to ack the address
            {
                ThisState = SM_ERROR;
                break;
            }

            i2c_write( BMP085_ReadADC); 	// internal register address
            i2c_rep_start( BMP085_I2C_Addr +I2C_READ ); // restart in Read mode now
   
            Up = (unsigned short) (i2c_readAck() << 8);
            Up +=  i2c_readNak();
           
            i2c_stop();

            ThisState++;
          }
            break;

        case SM_Calc_Press:
        {
            long p;
            unsigned long B4,B7;
            long B3,B6;

            B6=B5-4000L;
            X1 = (B6*B6)>>12;
            X1 *= BMP085_Cal.Coeff.B2;
            X1 >>= 11;

            X2 = BMP085_Cal.Coeff.AC2*B6;
            X2 >>= 11;

            X3=X1+X2;

            B3 = ((BMP085_Cal.Coeff.AC1 *4L + X3) +2) >> 2;
            X1 = (BMP085_Cal.Coeff.AC3* B6) >> 13;
            X2 = (BMP085_Cal.Coeff.B_1 * ((B6*B6) >> 12) ) >> 16;
            X3 = ((X1 + X2) + 2) >> 2;
            B4 = (BMP085_Cal.Coeff.AC4 * (unsigned long) (X3 + 32768L)) >> 15;
            B7 = ((unsigned long)(Up - B3) * 50000L );

            if (B7 < (unsigned long)0x80000000)
            {
                    p = (B7 << 1) / B4;
            }
            else
            {
                    p = (B7 / B4) << 1;
            }

            X1 = p >> 8;
            X1 *= X1;
            X1 = (X1 * 3038L) >> 16;
            X2 = (p * -7357L) >> 16;
            p += ((X1 + X2 + 3791L) >> 4);	// p in Pa

            BaroReading.BaromhPa = p/100.0; // in hPa
              
            ThisState = SM_IDLE;
        }
            break;

        case SM_ERROR:
 //    		Serial.print("Baro had i2C error");
			BaroReading.TempC = -273.0;
			BaroReading.BaromhPa =0.0;
			i2c_stop();
			ThisState++;
			break;

        case SM_IDLE: // park here until someone starts the process again.
            break;

        case SM_NOTFOUND:   // initially not found or could not read calibration values -- never escapes from here.
  //     	Serial.print("BMP085 not found");
            BaroReading.TempC = -301.0;   // Equally impossible numbers
            BaroReading.BaromhPa  =-1.0;
            break;     
    }

    
}

extern unsigned char ShortPressCnt;
extern char EncoderCnt;        
extern LiquidCrystal lcd;

void
Alt_Setting_adjust( void )
{     
        char p;
        
        ShortPressCnt = p = EncoderCnt =0;
        while ( !ShortPressCnt ) // set QNH
        {
           wdt_reset();
  
          if (EncoderCnt > p)
            AltimeterSetting += 0.25;
          else if (EncoderCnt < p)
            AltimeterSetting -= 0.25;
  
          p = EncoderCnt;
  
          lcd.setCursor ( 0, 0 );
          lcd.print("Set QNH ");
          lcd.setCursor ( 0, 1 );
          lcd.print( hPaToInch(AltimeterSetting) );
          lcd.print("\"Hg");
        }

}

