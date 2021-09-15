# Radin-Tankloader
The Radin Tankloader is made to load tank to a specified level without connection between the pumping base and the station it can be used as a timer or manually.
You can display values that the tank station send and analog button outpup values.

The tank unit send to the pumping station continuously the tank sensor voltage output (mV), the calculated tank level (L) and the eventual error code.
The pumping station regarding the required level entered by the user will swith of the relay (so the pump) when the desired level is reached.

Component of pumping base :
- Wemos D1R2 (esp8266) wifi board of equivalent chinese clone
- 220AC to minimum 7VDC and maximum 12VDC power supply
- 5 buttons analog board
- relay 3.3v (important)
- LCD1601 screen with i2c module

Component of tank station :
- Wemos D1R2 (esp8266) wifi board of equivalent chinese clone
- ADS1115
- power source if not already available
- analog gauge level sensor if not already available

The "AP" .ino is fr the pumping base, the "STA" .ino is for the station (tank).
All others infrmations should be located in the begining of each .ino files.
