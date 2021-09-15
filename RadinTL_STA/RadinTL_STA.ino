/*RADIN Tankloader (tank side) by Blaise Lapierre 05/09/2021
 * 
 * Check the variable below till the first function in order to setup the station!
 * The station function is to send the level in the tank & sensor voltage to the AP (pumping base).
 * This sketch support OTA (over the air) update, so you can, after the first program
 * uploaded by cable, upload new program over wifi without cable (set password below).
 * If the OTA access point is available when the program start, it connect on it, you can't use the station
 * because it's only for update, you can use your phone as access point for example.
 * 
 * Beware of the pinout of ESP8266 board (what's written on the board don't necessarily match reality).
 * 
 * Materials:
 * ESP8266 board wemos D1R2 (or clone).
 * ADS1115, connect it on +5v (not +3.3), GND, SDA/SCL pins.
 * 
 * ADS receive the sensor signal on A0, ATTENTION never exceed 5v on that signal input (ads supply is 5v).
 * You can adjust the ADS resolution below if your sensor output voltage is smaller.
 * 
 * Set your station by changing the parameters marked with #### at the end of the line.
 * 
 * Take note that in this program the tank sensor output voltage decrease while the water level increase!
 * Please take note of it when you'll build your electronic gauge, if it's the opposite logic you'll have
 * to rewrite the code part that calculate the level.
 * 
 * Calibration values as you can see are set for a 3100L tank, voltage change from +-3.7V when empty
 * to +-1.7v when full.
 * Load known volume in your tank, connect base & station together, press display button.
 * The string received show calculated volume (L) and voltage (in mV), note them and repeat
 * the operation since you got enough calibration points.
 * Then update this sketch with these values and upload it to the station board (OTA or cable).
 */

//Check you have all these libraries installed
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

//OTA router to connect on
const char* ssid = "your_OTA_Acces_point_name";//#### Enter router network name to connect with for OTA
const char* password = "your_OTA_AP_password";//#### Enter router network password to connect with for OTA
byte OTAstate = 1;//by default, program will start in OTA mode (if the OTA AP is available)

//ADS115
Adafruit_ADS1115 ads;
const float multiplier = 0.1875F;// set ADS115 resolution, see below
/*±6411mV 187.5uV
  ±4096mV 125uV
  ±2048mV 62.5uV
  ±1024mV 31.25uV
  ±512mV 15.625uV
  ±256mV 7.8125uV
*/

// Set WiFi credentials to connect with base umping station
#define WIFI_SSID "RadinTL_AP"//#### same as in the pumping base program
#define WIFI_PASS "RadinTL_AP_password"//#### same as in the pumping base program
IPAddress server(192,168,4,15);     // IP address of the AP
WiFiClient client;
 

// Volume data
// As you can understand, voltage decrease when volume in the tank increase!
int volumeMax = 3100;
int CalibVol[]= {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900, 3000, 3100}; //table of volume points (test values) ####
int CalibVolt[]= {3746, 3720, 3700, 3633, 3566, 3499, 3433, 3366, 3300, 3233, 3167, 3100, 3033, 2967, 2900, 2834, 2767, 2701, 2634, 2568, 2501, 2435, 2368, 2302, 2235, 2169, 2102, 2036, 1969, 1903, 1836, 1770}; //table of volt points (test values) ####
int tension = 0;
int volume = 0;
byte error;
 
void setup() {

    //OTA setup, the program try connect to OTA router first
  Serial.begin(115200);
  delay(1000);
  Serial.println("Trying to connect OTA access point!");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Retrying...");
    delay(5000);
    if(millis()> 7000){OTAstate = 0; break;}
  }

  if(OTAstate == 1){//if OTA is possible, do that setup
    // Hostname defaults to esp8266-[ChipID]
    ArduinoOTA.setHostname("Tank loader STA");

    // No authentication by default
    ArduinoOTA.setPassword((const char *)"1234");

    ArduinoOTA.onStart([]() {
      Serial.println("Start");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("End of setup");
    }//end if OTA possible
  

  if(OTAstate == 0){//if OTA not possible, do normal mode setup
    Serial.println("OTA Disabled!");

    ads.begin();// start ADS1115
 
    // Setup IO
    pinMode(2, INPUT);
   
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);           // connects to the WiFi AP of base pumping station
    Serial.println();
    Serial.println("Connection to the AP");
  
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
    }

    //Display connection data
    Serial.println();
    Serial.println("Connected");
    Serial.print("LocalIP:"); Serial.println(WiFi.localIP());
    Serial.println("MAC:" + WiFi.macAddress());
    Serial.print("Gateway:"); Serial.println(WiFi.gatewayIP());
    Serial.print("AP MAC:"); Serial.println(WiFi.BSSIDstr());
  }//end ota state == 0
}//end setup
  
void loop() {

  if(OTAstate == 1){Serial.println("OTA is Active!");ArduinoOTA.handle();}//only for OTA, else is normal program
  
  if(OTAstate == 0){
    volcalc();//call the function that calculate volume in the tank
 
    //variables
    String T = "";// sensor vltage (mv)
    String V = "";// calculated volume in tank (L)
    String E = "";// error 0=ok 1=disconnected (not used here) 2=calculated volume out of range 3=voltage too low, out of range 4=calibration is missing

    //Sub-strings
    if(tension >= 1000)                 {T = "T" + String(tension);}
    if(tension < 1000 && tension >= 100){T = "T0" + String(tension);}
    if(tension < 100 && tension >= 10)  {T = "T00" + String(tension);}
    if(tension < 10)                    {T = "T000" + String(tension);}

    if(volume >= 1000)                 {V = "V" + String(volume);}
    if(volume < 1000 && tension >= 100){V = "V0" + String(volume);}
    if(volume < 100 && tension >= 10)  {V = "V00" + String(volume);}
    if(volume < 10)                    {V = "V000" + String(volume);}

    if(volume < 0 || volume > volumeMax && error != 3){error = 2; Serial.println("Volume out of range!");}
    E = "E" + String(error);
  
    //String to send + size + display on serial
    String tosend = T + V + E;//tension T (mv), volume V (L), E error code
    byte strsize = tosend.length();
    Serial.print("String T = ");Serial.println(T);
    Serial.print("String V = ");Serial.println(V);
    Serial.print("String E = ");Serial.println(E);
    Serial.print("String to send = ");Serial.println(tosend);
    Serial.print("String lenght = ");Serial.println(strsize);

    //Connect and send
    client.connect(server, 80);//connect to sever IP and port
    Serial.println("********************************");
    Serial.print("Byte sent to the AP: ");
    Serial.println(client.print(tosend + "\r")); // \r problem in ap sketch?
    String answer = client.readStringUntil('\r');
    Serial.println("From the AP: " + answer);
    client.flush();
    client.stop();
    Serial.println("********************************");
  }//end ota state == 0
}//end loop


void volcalc(){
  Serial.println("");
  Serial.println("### volcalc function called ! ###");
  
  // ADS reading
  int16_t adc0, adc1, adc2, adc3;  
  adc0 = ads.readADC_SingleEnded(0);// read voltage on ADS1115 A0 pin
  //adc1 = ads.readADC_SingleEnded(1);
  //adc2 = ads.readADC_SingleEnded(2);
  //adc3 = ads.readADC_SingleEnded(3);
  tension = (adc0 * multiplier); // division by 3.3 because amx value of ads1115 is 32768, this ensure value is no more than 10 000
  Serial.print("AIN0: "); Serial.println(tension);


  // Volume calculation
  int ADsupTAB = 0;
  int ADinfTAB = 0;
  boolean calc = true;

  if(tension >= 5){ // if value > 
    for (int a=31; a>=0; a--){ 
      if(CalibVolt[a] > 0 && CalibVolt[a] >= tension){
        ADsupTAB = a+1;
        ADinfTAB = a;
        Serial.print("Adress volt sup in TAB & inf in TAB = ") && Serial.print(a+1) && Serial.print("  ") && Serial.println(a);
      }//end if
  
      if(CalibVolt[a] == 0){   
        Serial.println("Can't calculate actual volume (calibration problem)");
        Serial.println();
        error = 4;
        calc = false;
        break;
      }//end if
    }//end for

    //Volume calculation (example in comment with tension = 1000)
    if(calc == true){
      int deltaVol = CalibVol[ADsupTAB]-CalibVol[ADinfTAB]; // 600-500=100L - 0-0=0
      int deltaVolt = CalibVolt[ADinfTAB]-CalibVolt[ADsupTAB]; // 2000-500=1500 analogread - 0-0=0
      int g = tension-CalibVolt[ADsupTAB]; // 1000-500=500 analogread - 0-0=0
      float coef = float(g)/float(deltaVolt); //0.333 multiplicator - 0
      //if(coef<1){
      volume = CalibVol[ADsupTAB]-(deltaVol*coef); //533.3L
      Serial.print("delta volume = ") && Serial.print(deltaVol) && Serial.print(" delta Volt = ") && Serial.println(deltaVolt);
      Serial.print("g = ") && Serial.print(g) && Serial.print(" coef = ") && Serial.println(coef);
      Serial.print("ACTUAL VOLUME = ") && Serial.println(volume);
      Serial.println();
      error = 0;
      //}//end if
    }
  }
  else{error = 3; Serial.print("Error = 3, tension = "); Serial.print(tension); Serial.println(" too low, out of range.");}
}//end of Volt read function
