# AIR_LCDuino
~Uino based LCD display instrument to indicate Atmosphere and Wind related measurements. 

![](https://raw.githubusercontent.com/garyStofer/AIR_LCDuino/master/pics/IMG_20151030_095914.jpg)
This Arduino-IDE based project is for a simple Air-data display (Weather station) display based on a custom ATMEGA 328P based PCB which incorporates a standard 8x2 LCD (HD44780) and a rotary knob to select the various readings and settings. 

The sensors used are the Air-data and Wind sensors from the Wunder-Weather-Station project and can be found here [http://wws.us.to](http://wws.us.to)

The EagleCAD files for the PCB can be found in the hardware folder. The base PCB can be ordered from OSH PCB by searching for [Air_LCDuino](https://oshpark.com/shared_projects/kT6TDwLU) under shared Projects.

![](https://raw.githubusercontent.com/garyStofer/AIR_LCDuino/master/pics/IMG_20151112_133546.jpg)

The instrument PCB and housing uses a 2.25" standard aircraft round instrument hole pattern. The CAD files for a CNC routed enclosure can be found in the mechanical folder. You will need a copy of CamBam software to open the .cb source file, but the G-code .ngc file should run on most CNC routers with minor modification. There is a .dxf file that shows the basic dims of the PCB and LCD stack-up. 


The following readings are displayed:
- Bus Voltage 
- Density Altitude
- Standard Altitude (Standard Atmosphere)
- Compensated Altitude (by entering current sea level pressure)
- Station Pressure
- Humidity %RH
- Temperature
- Dew point
- Temperature Dewpt. Spread 
- Wind Direction
- Wind Speed current ( 1 second)
- Wind Average (10 minutes running average)
- Wind Gust ( last 10 minutes )

The two LEDs indicate alarm situations 

Blue LED comes on when Temp <= 1deg C
Red LED comes on when Vbus is < 11 or >15 volt 
Red LED comes on when Temp-Dewpt spread is <8 deg F 

![](https://raw.githubusercontent.com/garyStofer/AIR_LCDuino/master/pics/IMG_20151030_102445.jpg)

![](https://raw.githubusercontent.com/garyStofer/AIR_LCDuino/master/pics/IMG_20151112_134115.jpg)



  