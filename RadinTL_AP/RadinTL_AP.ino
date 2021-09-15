/*RADIN Tankloader (pumping base) by Blaise Lapierre 05/09/2021
 * 
 * Check the variable below till the first function in order to setup the Base!
 * The base allow to receive level of the tank and to laod it in automatic mode, timer mode or manual mode.
 * This sketch support OTA (over the air) update, so you can, after the first program
 * uploaded by cable, upload new program over wifi without cable (set password below).
 * If the OTA access point is available when the program start, it connect on it, you can't use the pumping base
 * because it's only for update, you can use your phone as access point for example.
 * 
 * Beware of the pinout of ESP8266 board (what's written on the board don't necessarily match reality).
 * 
 * Materials:
 * ESP8266 board wemos D1R2 (or clone).
 * I2C LCD1601 screen, connect it on +5v, GND, SDA/SCL pins.
 * 3.3v (that's important) relay, connect +3.3v, GND, and signal on pin14.
 * 
 * Set your base by changing the parameters marked with #### at the end of the line.
 * Set DSP value at 2, to first display the button value returned when you press each button,
 * note these value and update in the program, then reset DSP value as origin.
 */

//Check you have all these libraries installed
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h> // include i/o class header
hd44780_I2Cexp lcd;

//OTA values
const char* ssid = "your_OTA_Acces_point_name";//####
const char* password = "your_OTA_AP_password";//####
byte OTAstate = 1;

//No OTA values
#define AP_SSID "RadinTL_AP"//####
#define AP_PASS "RadinTL_AP_password"//####

//ESP8266_wificom_AP code
WiFiServer server(80);
IPAddress IP(192,168,4,15);
IPAddress mask = (255, 255, 255, 0);
 
// Wireless data variables
int T;
int V;
byte E = 1; //0= no error 1=connection problem 2= actual volume too loww or too high
byte prevE;

unsigned long lastgettime = 0;
String laststringget;
String stringget;

// Switches variables
int SW[] = {31, 80, 3, 155, 320};// first value=top button"+" second=dowm"-" third=lest:display fourth=right:mode fift=start/stop ####
byte SWtolerance = 11;
bool prevSWpos[] = {0, 0, 0, 0, 0};// previous swith position
bool SWpos[] = {0, 0, 0, 0, 0};// actual swith position
unsigned long SWtime;// time since switch is pressed
int increment;// how much target volume is increased
int i = 0;// update time when to increase target volume

// Display and loading mode variables
byte DSP;// 0=Volume, 1=packet, 2= Analog button value, eventually, set "DSP = 2;" if you want the pumping base start on button value display mode ####
byte prevDSP;// value of DSP in the previous loop
byte MODE;// 0=Pump to volume target 1=Pump during time 2=Pump while key pressed
byte prevMODE;
unsigned int targetV;
unsigned int actualV;
byte timer;
unsigned long timer_started_time;
unsigned long packet_display_mem;
unsigned int dispcount = 1;
unsigned int switchcount = 1;

// Pump variable
byte relaypin = 14;
boolean relay = 0;
boolean mode_zero_pumping = 0;
int decon_delay = 5000;

// Safety (if mode automatic, pumping and level don't increase)
unsigned long levelchecktime;
byte leveldelay = 12;
int actualVatcheck;
byte levelmargin = 10;

/*ERROR LIST :
 * 1 = connection problem
 * 2 = automatic mode, pumping but level don't increase
*/


void switches(){
  // Switches position update
  int Azero = analogRead(A0);//
  
  for(byte x = 0; x <= 4; x++){// update position of switches 0 to 4
    prevSWpos[x] = SWpos[x];
    if(Azero>= SW[x]-SWtolerance && Azero <= SW[x]+SWtolerance){SWpos[x] = 1;}// set if switch is pressed
    else{SWpos[x] = 0;}// or not pressed
  }
  yield();
  
  // PLUSs & MINUS SWITCHES
  // Switches positions update
  for(byte u = 0; u <=1; u++){
    if(SWpos[u] == 1 && prevSWpos[u] == 0){prevSWpos[u] = SWpos[u]; SWtime = millis();}// if switch just pressed, set actual position as previous and start time counter

    // If mode volume set (mode=0)
    if(MODE == 0 && SWpos[u] == 1 && prevSWpos[u] == 1){// if switch is pressed and was already previously
      if(millis()-SWtime < 2000 && u == 0){increment = 10;}// if pressed since less than this time, adjust increment
      if(millis()-SWtime >= 2000 && u == 0){increment = 100;}// if pressed since less than this time, adjust increment
      if(millis()-SWtime < 2000 && u == 1){increment = -10;}// if pressed since less than this time, adjust increment
      if(millis()-SWtime >= 2000 && u == 1){increment = -100;}// if pressed since less than this time, adjust increment
      if(millis() >= SWtime+(300*i)){targetV = targetV + increment; i++;}// if time to increase target volume is coming, increment from pre-defined value and set next time to increase
      if(targetV > 9999){targetV = 9999;}
    }
    // If mode timer set (mode=1)
    if(MODE == 1 && SWpos[u] == 1 && prevSWpos[u] == 1){// if switch is pressed and was already previously
      if(u == 0){increment = 1;}// if pressed, adjust increment
      //if(millis()-SWtime >= 2000 && u == 0){increment = 100;}// if pressed since less than this time, adjust increment (line to add long press on switch value)
      if(u == 1){increment = -1;}// if pressed, adjust increment
      //if(millis()-SWtime >= 2000 && u == 1){increment = -100;}// if pressed since less than this time, adjust increment (line to add long press on switch value)
      if(millis() >= SWtime+(300*i)){timer = timer + increment; i++;}// if time to increase target volume is coming, increment from pre-defined value and set next time to increase
    }
    
    if(SWpos[u] == 0 && prevSWpos[u] == 1){SWtime = 0; i=0;}
  }
  yield();

  //Pump activation, pump mode, display mode switch
  for(byte j = 2; j <=4; j++){
    if(SWpos[j] == 1 && prevSWpos[j] == 0){
      if(j == 4 && relay == 1){
        relay = 0;
        pinMode(relaypin, INPUT);
        if(MODE == 1){lcd.setCursor(4, 1); lcd.print("      ");}
        break;
       }
      if(j == 4 && relay == 0 && MODE == 0 && E == 0 && targetV > actualV){relay = 1; mode_zero_pumping = 1; pinMode(relaypin, OUTPUT); levelchecktime = millis(); actualVatcheck = actualV; break;} //**
      if(j == 4 && relay == 0 && MODE == 1 && timer > 0){relay = 1; pinMode(relaypin, OUTPUT); timer_started_time = millis();}//***
      if(j == 4 && relay == 0 && MODE == 2){relay = 1; pinMode(relaypin, OUTPUT);}//**
      if(j == 4 && DSP >= 2){relay = 0;}
      if(j == 3){MODE++;}
      if(j == 3 && MODE > 2){MODE = 0;}
      if(j == 2 && DSP <= 2){DSP++;}
      if(j == 2 && DSP >= 3){DSP = 0;}
    }
    //prevSWpos[j] = SWpos[j];  
  }
  yield();

  /*
  //Switches & variable display on serial monitor
  Serial.print("Azero = ");Serial.println(Azero);
  Serial.print("SWpos = ");Serial.print(SWpos[0]);Serial.print(SWpos[1]);Serial.print(SWpos[2]);Serial.print(SWpos[3]);Serial.println(SWpos[4]);
  Serial.print("prevSWpos = ");Serial.print(prevSWpos[0]);Serial.print(prevSWpos[1]);Serial.print(prevSWpos[2]);Serial.print(prevSWpos[3]);Serial.println(prevSWpos[4]);
  Serial.print("increment = ");Serial.println(increment);
  Serial.print("targetV = ");Serial.print(targetV);Serial.print( " timer = ");Serial.println(timer);
  Serial.print("Mode = ");Serial.println(MODE);
  Serial.print("Display = ");Serial.println(DSP);
  Serial.print("Relay = ");Serial.println(digitalRead(relaypin));
  Serial.println("");
  */
}


void Display(){
  Serial.println("Display function called");
  if(DSP != prevDSP){lcd.clear();}//erase sreen if DSP value change

  
  if(DSP == 1 && (millis() - packet_display_mem) >= 1500){ // If you want to display packet
    packet_display_mem = millis();
    lcd.clear();
    lcd.setCursor(1, 0); lcd.print("Last packet :");
    if(stringget != ""){lcd.setCursor(1, 1); lcd.print(stringget);}
    else{lcd.setCursor(1, 1); lcd.print(laststringget);}
  }
    
    
  if(DSP == 2){ // If you want to display Azero
    lcd.clear();    
    int a = analogRead(A0);
    int b = analogRead(A0);
    int c = analogRead(A0);
    int d = analogRead(A0);
    int e = analogRead(A0);
    int f = analogRead(A0);
    int g = analogRead(A0);
    int h = analogRead(A0);
    int i = analogRead(A0);
    int j = analogRead(A0);
    int SWval = (a+b+c+d+e+f+g+h+i+j)/10;
    lcd.setCursor(1, 0); lcd.print("Button val. :");
    lcd.setCursor(1, 1); lcd.print(SWval);}
    
  String connection = "";
  Serial.println("millis()-lastgettime= ");Serial.println(millis()-lastgettime);
  if(/*E == 0 && */millis()-lastgettime < decon_delay){connection = "Conn.";Serial.println("str connection = conn");}
  if(/*E == 0 && */millis()-lastgettime >= decon_delay){connection = "Disco.";Serial.println("str connection = " + connection);}

  // If mode changed update all display
  if(MODE == 0 && prevMODE != 0 && DSP == 0 || DSP == 0 && prevDSP != 0){//if mode 0 from another mode or from other DSP
    if(MODE != 0){MODE = 0;}
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(connection);
    lcd.setCursor(6, 0); lcd.print("Vol: ");
    lcd.setCursor(11, 0); lcd.print(actualV);
    lcd.setCursor(15, 0); lcd.print("L");
    if(relay == 0){lcd.setCursor(0, 1); lcd.print("OFF");}
    if(relay == 1){lcd.setCursor(0, 1); lcd.print("ON!");}
    lcd.setCursor(4, 1); lcd.print("Target: ");
    lcd.setCursor(11, 1); lcd.print(targetV);
    lcd.setCursor(15, 1); lcd.print("L");
    if(E != 0){lcd.setCursor(6, 0); lcd.print("ERROR!!!!!");}
  }

  if(MODE == 1 && prevMODE != 1 && DSP == 0){
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(connection);
    lcd.setCursor(6, 0); lcd.print("TIMER MODE");
    if(relay == 0){lcd.setCursor(0, 1); lcd.print("OFF");lcd.setCursor(4, 1); lcd.print(timer);}
    lcd.setCursor(10, 1); lcd.print("Mins.");
  }

  if(MODE == 2 && prevMODE != 2 && DSP == 0){
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(connection);
    lcd.setCursor(6, 0); lcd.print("MAN. MODE");
    if(relay == 0){lcd.setCursor(0, 1); lcd.print("Pump OFF");}
    if(relay == 1){lcd.setCursor(0, 1); lcd.print("Pump ON!");}
  }

  // If mode stayed the same update only variables
  if(MODE == 0 && prevMODE == 0 && DSP == 0){
    if(E != 0 && prevE == 0){lcd.setCursor(6, 0); lcd.print("ERROR!!!!!");}
    if(E == 0 && prevE > 0){
      lcd.setCursor(6, 0); lcd.print("           ");
      lcd.setCursor(6, 0); lcd.print("Vol: ");
      lcd.setCursor(11, 0); lcd.print(actualV);
      lcd.setCursor(15, 0); lcd.print("L");
    }
    if(E == 0 && prevE == 0){lcd.setCursor(11, 0); lcd.print(actualV);}
    lcd.setCursor(0, 0); lcd.print(connection);
    if(relay == 0){lcd.setCursor(0, 1); lcd.print("OFF");}
    if(relay == 1){lcd.setCursor(0, 1); lcd.print("ON!");}
    lcd.setCursor(11, 1); lcd.print(targetV);
  }
  
  if(MODE == 1 && prevMODE == 1 && DSP == 0){
    lcd.setCursor(0, 0); lcd.print(connection);
    if(relay == 0){lcd.setCursor(0, 1); lcd.print("OFF");lcd.setCursor(4, 1); lcd.print(timer);}
    if(relay == 1){lcd.setCursor(0, 1); lcd.print("ON!");}
    if(relay == 1){
      int minPos = 4;//display position of minutes on the LCD
      int millisremaining = (timer*60000) - (millis()-timer_started_time);//how much milli seconds are remaining
      int Mremaining = millisremaining / 60000;//convert to decimal minutes
      int Sremaining = (millisremaining - (Mremaining*60000))/1000;//find remaining seconds
      lcd.setCursor(minPos, 1);lcd.print(Mremaining);//display minutes on LCD

      //decimal sign and seconds
      int dotPos;
      int sPos;
      if(Mremaining < 10){dotPos = minPos+1;}
      if(Mremaining >= 10 && Mremaining < 100){dotPos = minPos+2;}
      if(Mremaining >= 100){dotPos = minPos+3;}
      if(Sremaining < 10){lcd.setCursor(dotPos+1, 1); lcd.print("0"); sPos = dotPos+2;}//display a 0 if remaining seconds are < 10 and set seconds position
      else{sPos = dotPos+1;}//else write remaining second >=10   
      lcd.setCursor(dotPos, 1);lcd.print(".");//display the decimal sign
      lcd.setCursor(sPos, 1);lcd.print(Sremaining);//display remaining seconds
      int z=dotPos+3;
      while(z <= 9){//"Mins." is printed at position 10, write spaces after seconds
        lcd.setCursor(z, 1);lcd.print(" ");//display remaining seconds
        z++;//go to next digit
      }
    }
  }
  
  if(MODE == 2 && prevMODE == 2 && DSP == 0){
    lcd.setCursor(0, 0); lcd.print(connection);
    if(relay == 0){lcd.setCursor(0, 1); lcd.print("Pump OFF");}
    if(relay == 1){lcd.setCursor(0, 1); lcd.print("Pump ON!");}
  }

  prevMODE = MODE;//set previous value in memory for next loop
  prevE = E;//set previous value in memory for next loop
  prevDSP = DSP;//set previous value in memory for next loop
}







void setup() {

  //OTA setup
  Serial.begin(115200);
  delay(1000);
  Serial.println("Trying to connect OTA access point!");
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(1, 0); lcd.print("OTA AP conn.");
  lcd.setCursor(1, 1); lcd.print("Please wait!");//"WAIT !"
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Retrying...");
    delay(5000);
    if(millis()> 5000){OTAstate = 0; break;}
  }
  

if(OTAstate == 1){//if OTA is possible
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("Tank Loader AP");

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
  lcd.clear();
  lcd.setCursor(1, 0); lcd.print("Ready for OTA");
  lcd.setCursor(1, 1); lcd.print("update !");
  Serial.println("End of setup");
  }//end if OTA possible


if(OTAstate == 0){//if OTA not possible, normal setup
 Serial.println("OTA Disabled!");

 //Serial.begin(115200);
 WiFi.mode(WIFI_AP);
 WiFi.softAP(AP_SSID, AP_PASS);
 WiFi.softAPConfig(IP, IP, mask);
 server.begin();
 Serial.println();
 Serial.println("Server started.");
 Serial.print("IP: "); Serial.println(WiFi.softAPIP());
 Serial.print("MAC:"); Serial.println(WiFi.softAPmacAddress());
 //end
  
    // Relay pin
  pinMode(relaypin, INPUT); // relay pin enable as output
  

  lcd.clear();
    // première initialisation LCD
    lcd.setCursor(6, 0); lcd.print("Vol: ");
    lcd.setCursor(11, 0); lcd.print(actualV);
    lcd.setCursor(15, 0); lcd.print("L");
    if(relay == 0){lcd.setCursor(0, 1); lcd.print("OFF");}
    if(relay == 1){lcd.setCursor(0, 1); lcd.print("ON!");}
    lcd.setCursor(4, 1); lcd.print("Target:");
    lcd.setCursor(11, 1); lcd.print(targetV);
    lcd.setCursor(15, 1); lcd.print("L");
    
    Serial.println("End of setup");
  }//end if OTA not possible
}//end setup



void loop() {

  if(OTAstate == 1){Serial.println("OTA is Active!");ArduinoOTA.handle();}//only for OTA, else is normal program
  
  if(OTAstate == 0){

    //Serial.print("Azero = ");Serial.println(analogRead(A0));// button test

    if(millis()-lastgettime >= decon_delay && E != 1){Serial.println("connection PROBLEM!"); E = 1; pinMode(relaypin, INPUT);}
    //if(millis()-lastgettime < decon_delay){E = 0;}
    if(mode_zero_pumping == 1 && E == 0){Serial.println("Conn OK, restarting pump!"); pinMode(relaypin, OUTPUT);}//**
    if(MODE == 0 && actualV >= targetV - 20 && mode_zero_pumping == 1){Serial.println("Volume pumping FINISHED!"); mode_zero_pumping = 0; pinMode(relaypin, INPUT);}
    if(MODE == 1 && relay == 1 && millis()-timer_started_time >= timer*60000){Serial.println("Timer FINISHED!"); pinMode(relaypin, INPUT); relay = 0; prevMODE = 0; timer = 0; lcd.clear();}

    if(MODE == 0 && relay == 1 && millis() >= (levelchecktime+(leveldelay*6000))){ //Safety if mode automatic, pumping and level don't increase
      if(actualV <= actualVatcheck){Serial.println("NO WATER FLOW!"); E = 2; pinMode(relaypin, INPUT);}
      if(actualV > actualVatcheck + levelmargin){Serial.print("level safety when pumping OK params : "); Serial.print(levelchecktime); Serial.print("s, margin ");Serial.print(levelmargin);Serial.println("L");}
      levelchecktime = millis();
    }
    
    if(stringget != "") {
      laststringget = stringget;//actual stringget become prev. string
      //stringget = request;//actual stringget updated
      lastgettime = millis();//update last time packet was read
      Serial.print("Millis()= ");Serial.println(millis());
      Serial.print("lastgettime= ");Serial.println(lastgettime);
      String Tstring = stringget.substring(1, 5);
      String Vstring = stringget.substring(6, 10);
      String Estring = stringget.substring(11, 12);
      //Serial.print("Tstring = ");Serial.println(Tstring);Serial.print("Vstring = ");Serial.println(Vstring);
      T = Tstring.toInt();
      V = Vstring.toInt();
      E = Estring.toInt();
      actualV = V;
      Serial.print("T= ");Serial.print(T); Serial.print(" V= ");Serial.print(V); Serial.print(" E= ");Serial.println(E);
    }
  yield();

  if(millis() >= 100*switchcount){switchcount++; switches();}// call switches function every XXXms
  if(millis() >= 500*dispcount){dispcount++; Display();}// call display function every XXXms
  stringget = "";//reset string

  WiFiClient client = server.available();
  if (!client) {return;}
  stringget = client.readStringUntil('\r');
  Serial.println("********************************");
  Serial.println("Received from the station: " + stringget);
  client.flush();
  Serial.print("Number of byte sent to the station: ");
  Serial.println(client.println("AP received : " + stringget + "\r"));
  Serial.println("********************************");
  
  }//end OTA == 0
}
