# Radin-Tankloader

"Radin" mean greedy in french!
Similar functions commercial devices cost +-400€...

The Radin Tankloader is made to load tank to a specified level, without connection between the pumping base and the station it can be used as a timer or manually.
You can display values that the tank station sent and analog button output values.

The tank unit send to the pumping station continuously the tank sensor voltage output (mV), the calculated tank level (L) and the eventual error code.
The pumping station regarding the required level entered by the user will switch off the relay (so the pump) when the desired level is reached.

Component of pumping base :
- Wemos D1R2 (esp8266) wifi board or equivalent chinese clone
- 220AC to minimum 7VDC and maximum 12VDC power supply (old phone charger, old RC battery charger...etc)
- 5 buttons analog board type "keyes_AD_key kb45037"
- relay 3.3v (important) type "HL-51 v1.0"
- LCD1602 screen with it's i2c module

Component of tank station :
- Wemos D1R2 (esp8266) wifi board or equivalent chinese clone
- ADS1115
- power source if not already available on the tank
- KCD11 switch
- analog gauge level sensor if not already on the tank

The "AP" .ino is for the pumping base, the "STA" .ino is for the station (tank).
All others informations should be located in the begining of each .ino files.
If you have a problem with libraries, download the .zip file and place the content in your arduino library folder.
