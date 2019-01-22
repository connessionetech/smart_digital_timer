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

/**
 * Generate unique client ID
 */
void setupClientId()
{
  uint32_t chipid=ESP.getChipId();
  snprintf(clientid,25,"SB-%08X",chipid);
  ssid = clientid;
}

/**
 * Initialize access point
 */
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

/**
 * Setup webserver
 */
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

/**
 * Handles root request
 */
void handleRoot()
{
  server.send(200, "text/plain", "hello"); 
}

/**
 * Handles 404 requests
 */
void handleNotFound()
{
  server.send(404, "text/plain", "Not found"); 
}

/**
 * Read state of switch A
 */
void readSwitchA()
{
  String data;
  
  StaticJsonBuffer<100> responseBuffer;
  RtcDateTime now = Rtc.GetDateTime();
  JsonObject& response = responseBuffer.createObject();
  response["id"] = conf.relay_1_id;
  response["state"] = conf.relay_1;
  response["lastupdate"] = conf.relay_1_lastupdate;
  response.printTo(data);

  server.send(200, "application/json", data); 
}

/**
 * Toggle state of switch A
 */
void toggleSwitchA()
{
  String data;
  
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

  StaticJsonBuffer<100> responseBuffer;
  RtcDateTime now = Rtc.GetDateTime();
  JsonObject& response = responseBuffer.createObject();
  response["id"] = conf.relay_1_id;
  response["state"] = conf.relay_1;
  response["lastupdate"] = conf.relay_1_lastupdate;
  response.printTo(data);
  
  server.send(200, "application/json", data);
}

/**
 * Read state of switch B
 */
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

/**
 * Toggle state of switch B
 */
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

/**
 * Read alarm schedules
 */
void getAlarms()
{
  String data;
  String content = "20:15:30:7:1:s1:0|21:15:30:7:1:s2:1";
  
  StaticJsonBuffer<200> responseBuffer;
  RtcDateTime dt = Rtc.GetDateTime();
  JsonObject& response = responseBuffer.createObject();
  response["status"] = "success";
  response["data"] = content;
  response.printTo(data);

  server.send(200, "application/json", data); 
}

/**
 * Get RTC Time
 */
void getClockTime()
{
  String data;
  
  StaticJsonBuffer<200> responseBuffer;
  RtcDateTime now = Rtc.GetDateTime();
  printDateTime(now);
  JsonObject& response = responseBuffer.createObject();
  response["dd"] = now.Day();
  response["hh"] = now.Hour();
  response["mm"] = now.Minute();
  response["ss"] = now.Second();
  response.printTo(data);

  server.send(200, "application/json", data); 
}

/**
 * Set alarm schedules
 */
void setAlarms()
{
  String data;
  
  if (server.hasArg("plain")== false)
  {
      StaticJsonBuffer<200> inBuffer;
      JsonObject& inObj = inBuffer.parseObject(server.arg("plain"));
      //char data[400] = inObj["data"];

      String content = "20:15:30:7:1:s1:0|21:15:30:7:1:s2:1";
  
      StaticJsonBuffer<200> outBuffer;
      RtcDateTime dt = Rtc.GetDateTime();
      JsonObject& root = outBuffer.createObject();
      root["status"] = "success";
      root["data"] = content;
      root.printTo(data);
  }  
  
  server.send(200, "application/json", data);
}


/**
 * Sets RTC time
 */
void setClockTime()
{  
  String data;

  StaticJsonBuffer<200> responseBuffer;
  RtcDateTime dt = Rtc.GetDateTime();
  
  if (server.hasArg("plain")== false){
      // Read request data
      StaticJsonBuffer<200> requestBuffer;
      JsonObject& request = requestBuffer.parseObject(server.arg("plain"));
      int yy = request["yy"];
      int mm = request["mm"];
      int dd = request["dd"];
      int h = request["h"];
      int i = request["i"];
      int s = request["s"];
    
      //RtcDateTime(uint16_t year, uint8_t month, uint8_t dayOfMonth, uint8_t hour, uint8_t minute, uint8_t second)
      RtcDateTime currentTime = RtcDateTime(yy,mm,dd,h,i,s); //define date and time object
      Rtc.SetDateTime(currentTime);
    }  

    // Prepare response data
    JsonObject& response = responseBuffer.createObject();
    response["status"] = "success";
    response["data"] = "";
    response.printTo(data);

    server.send(200, "application/json", data);
}

/**
 * Initialize RTC
 */
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
  server.handleClient();
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

/**
 * Print date time from RTC
 */
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
