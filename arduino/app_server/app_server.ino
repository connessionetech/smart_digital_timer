#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS3231.h>
#include <ArduinoJson.h>

RtcDS3231<TwoWire> Rtc(Wire);
IPAddress local_ip(192,168,5,1);
IPAddress gateway(192,168,5,1);
IPAddress subnet(255,255,255,0);

/* This are the WiFi access point settings. Update them to your likin */
const char *ssid = "NodeMcu";
const char *password = "12345678";

// Define a web server at port 80 for HTTP
ESP8266WebServer server(80);


void setup() {
  Serial.begin(57600);
  
  delay(10000);
  
  Serial.println("Starting");  
  setupRTC();
  setupAP();
  setupWebServer();
}


void setupAP()
{
  //set-up the custom IP address
  WiFi.mode(WIFI_AP); 
  
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet); 
  IPAddress myIP = WiFi.softAPIP(); //Get IP address
  Serial.print("HotSpt IP:");
  Serial.println(myIP);
}


void setupWebServer()
{
  server.on ( "/", handleRoot );
  server.on ( "/alarms/get", getAlarms );
  server.on ( "/alarms/set", setAlarms );
  server.on ( "/time/get", getClockTime );
  server.on ( "/time/set", setClockTime );
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
