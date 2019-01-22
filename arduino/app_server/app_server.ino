#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS3231.h>
#include <ArduinoJson.h>

RtcDS3231<TwoWire> Rtc(Wire);
IPAddress local_ip(192,168,5,1);
IPAddress gateway(192,168,5,1);
IPAddress subnet(255,255,255,0);

const int RELAY1=4;
const int RELAY2=5;

char clientid[25];

char *ssid = "";
char *password = "12345678";

// Define a web server at port 80 for HTTP
ESP8266WebServer server(80);

struct Settings {
   const int relay_1_id = 1;
   int relay_1;
   long relay_1_lastupdate;
   const int relay_2_id = 2;
   int relay_2;
   long relay_2_lastupdate;
};

Settings conf = {};


void setup() {
  Serial.begin(57600);

  setupClientId();
  
  delay(10000);
  
  Serial.println("Starting");  
  setupRTC();
  setupAP();
  setupWebServer();
}

void setupClientId()
{
  uint32_t chipid=ESP.getChipId();
  snprintf(clientid,25,"SB-%08X",chipid);
  ssid = clientid;
}


void setupAP()
{
  //set-up the custom IP address
  WiFi.mode(WIFI_AP); 
  
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet); 
  IPAddress myIP = WiFi.softAPIP(); //Get IP address
  Serial.print("HotSpot IP:");
  Serial.println(myIP);
}


void setupWebServer()
{
  server.on ( "/", handleRoot );
  server.on ( "/alarms/get", getAlarms );
  server.on ( "/alarms/set", HTTP_POST, setAlarms );
  server.on ( "/time/get", getClockTime );
  server.on ( "/time/set", HTTP_POST, setClockTime );
  server.on ( "/switch/1/set", HTTP_POST, toggleSwitchA );
  server.on ( "/switch/1/get", HTTP_GET, readSwitchA ); 
  server.on ( "/switch/2/set", HTTP_POST, toggleSwitchB );
  server.on ( "/switch/2/get", HTTP_GET, readSwitchB );  
  server.on ( "/", handleRoot );
  server.onNotFound ( handleNotFound );
  
  server.begin();
  Serial.println("HTTP server started");
}

void handleRoot()
{
  server.send(200, "text/plain", "hello"); 
}

void handleNotFound()
{
  server.send(404, "text/plain", "Not found"); 
}

void readSwitchA()
{
  StaticJsonBuffer<100> jsonBuffer;
  RtcDateTime now = Rtc.GetDateTime();
  JsonObject& root = jsonBuffer.createObject();
  root["id"] = conf.relay_1_id;
  root["state"] = conf.relay_1;
  root["lastupdate"] = conf.relay_1_lastupdate;

  String data;
  root.printTo(data);

  server.send(200, "application/json", data); 
}

void toggleSwitchA()
{
  if(conf.relay_1 == 0)
  {
    conf.relay_1=1;
    //digitalWrite(RELAY1, LOW);
  }
  else
  {
    conf.relay_1=0;
    //digitalWrite(RELAY1, HIGH);
  }

  StaticJsonBuffer<100> jsonBuffer;
  RtcDateTime now = Rtc.GetDateTime();
  JsonObject& root = jsonBuffer.createObject();
  root["id"] = conf.relay_1_id;
  root["state"] = conf.relay_1;
  root["lastupdate"] = conf.relay_1_lastupdate;

  String data;
  root.printTo(data);

  server.send(200, "application/json", data);
}


void readSwitchB()
{
  StaticJsonBuffer<100> jsonBuffer;
  RtcDateTime now = Rtc.GetDateTime();
  JsonObject& root = jsonBuffer.createObject();
  root["id"] = conf.relay_2_id;
  root["state"] = conf.relay_2;
  root["lastupdate"] = conf.relay_2_lastupdate;

  String data;
  root.printTo(data);

  server.send(200, "application/json", data); 
}

void toggleSwitchB()
{
  if(conf.relay_2 == 0)
  {
    conf.relay_2=1;
    //digitalWrite(RELAY2, LOW);
  }
  else
  {
    conf.relay_2=0;
    //digitalWrite(RELAY2, HIGH);
  }

  StaticJsonBuffer<100> jsonBuffer;
  RtcDateTime now = Rtc.GetDateTime();
  JsonObject& root = jsonBuffer.createObject();
  root["id"] = conf.relay_2_id;
  root["state"] = conf.relay_2;
  root["lastupdate"] = conf.relay_2_lastupdate;

  String data;
  root.printTo(data);

  server.send(200, "application/json", data);
}

void getAlarms()
{
  StaticJsonBuffer<200> jsonBuffer;
  RtcDateTime dt = Rtc.GetDateTime();
  JsonObject& root = jsonBuffer.createObject();
  root["data"] = "";

  String data;
  root.printTo(data);

  server.send(200, "application/json", data); 
}

void getClockTime()
{
  StaticJsonBuffer<200> jsonBuffer;
  RtcDateTime now = Rtc.GetDateTime();
  printDateTime(now);
  JsonObject& root = jsonBuffer.createObject();
  root["dd"] = now.Day();
  root["hh"] = now.Hour();
  root["mm"] = now.Minute();
  root["ss"] = now.Second();

  String data;
  root.printTo(data);

  server.send(200, "application/json", data); 
}

void setAlarms()
{
  
}

void setClockTime()
{  
  if (server.hasArg("plain")== false){
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(server.arg("plain"));
      int yy = root["yy"];
      int mm = root["mm"];
      int dd = root["dd"];
      int h = root["h"];
      int i = root["i"];
      int s = root["s"];
    
      //RtcDateTime(uint16_t year, uint8_t month, uint8_t dayOfMonth, uint8_t hour, uint8_t minute, uint8_t second)
      RtcDateTime currentTime = RtcDateTime(yy,mm,dd,h,i,s); //define date and time object
      Rtc.SetDateTime(currentTime);
  }  
}

void setupRTC()
{
    Serial.print("compiled: ");
    Serial.print(__DATE__);
    Serial.println(__TIME__);
    
    Rtc.Begin();

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    printDateTime(compiled);
    Serial.println();


    if (!Rtc.IsDateTimeValid()) 
    {
        Serial.println("RTC lost confidence in the DateTime!");
        Rtc.SetDateTime(compiled);
    }

    if (!Rtc.GetIsRunning())
    {
        Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled) 
    {
        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        Rtc.SetDateTime(compiled);
    }
    else if (now > compiled) 
    {
        Serial.println("RTC is newer than compile time. (this is expected)");
    }
    else if (now == compiled) 
    {
        Serial.println("RTC is the same as compile time! (not expected but all is fine)");
    }

    // never assume the Rtc was last configured by you, so
    // just clear them to your needed state
    Rtc.Enable32kHzPin(false);
    Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone); 
}

void loop() {
  // put your main code here, to run repeatedly:
  server.handleClient();
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}
