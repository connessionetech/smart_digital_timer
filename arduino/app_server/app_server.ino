#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS3231.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

RtcDS3231<TwoWire> Rtc(Wire);
IPAddress local_ip(192,168,5,1);
IPAddress gateway(192,168,5,1);
IPAddress subnet(255,255,255,0);

const int RELAY1=4;
const int RELAY2=5;
const int MAX_SCHEDULES = 20;

char clientid[25];
char *ssid = "";
char *password = "12345678";

int eeAddress = 0;
boolean debug = true;

// Define a web server at port 80 for HTTP
ESP8266WebServer server(80);

struct Settings {
   const int r1_id = 1;
   int r1;
   long r1_updated;
   const int r2_id = 2;
   int r2;
   long r2_updated;
   int schedule_string_length;
   char schedule_string[350];
};

struct Schedule {
   int index;
   int hh;
   int mm;
   int ss;
   int dow;
   char* target;
   int recurring;
   int target_state;
};

Settings conf = {};
Schedule schedules[MAX_SCHEDULES] = {};

void setup() {
  Serial.begin(57600);

  setupEeprom();
  setupClientId();
  
  delay(10000);
  
  Serial.println("Starting");  
  setupRTC();
  //setupAP();
  setupSta();
  setupWebServer();
}


void setupEeprom()
{
  // start eeprom
  EEPROM.begin(512);
  eeAddress = 0;
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


void setupSta()
{
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin("Tomato24", "bhagawadgita@123");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

/**
 * Setup webserver
 */
void setupWebServer()
{
  server.on ( "/", handleRoot );
  server.on ( "/alarms/get", HTTP_GET, getAlarms );
  server.on ( "/alarms/set", HTTP_POST, setAlarms );
  server.on ( "/time/get", HTTP_GET, getClockTime );
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
  response["id"] = conf.r1_id;
  response["state"] = conf.r1;
  response["lastupdate"] = conf.r1_updated;
  response.printTo(data);

  server.send(200, "application/json", data); 
}

/**
 * Toggle state of switch A
 */
void toggleSwitchA()
{
  String data;
  
  if(conf.r1 == 0)
  {
    conf.r1=1;
    //digitalWrite(RELAY1, LOW);
  }
  else
  {
    conf.r1=0;
    //digitalWrite(RELAY1, HIGH);
  }

  StaticJsonBuffer<100> responseBuffer;
  RtcDateTime now = Rtc.GetDateTime();
  JsonObject& response = responseBuffer.createObject();
  response["id"] = conf.r1_id;
  response["state"] = conf.r1;
  response["lastupdate"] = conf.r1_updated;
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
  root["id"] = conf.r2_id;
  root["state"] = conf.r2;
  root["lastupdate"] = conf.r2_updated;

  String data;
  root.printTo(data);

  server.send(200, "application/json", data); 
}

/**
 * Toggle state of switch B
 */
void toggleSwitchB()
{
  if(conf.r2 == 0)
  {
    conf.r2=1;
    //digitalWrite(RELAY2, LOW);
  }
  else
  {
    conf.r2=0;
    //digitalWrite(RELAY2, HIGH);
  }

  StaticJsonBuffer<100> jsonBuffer;
  RtcDateTime now = Rtc.GetDateTime();
  JsonObject& root = jsonBuffer.createObject();
  root["id"] = conf.r2_id;
  root["state"] = conf.r2;
  root["lastupdate"] = conf.r2_updated;

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

  DynamicJsonBuffer responseBuffer;
  JsonObject& response = responseBuffer.createObject();
  
  // read from eeprom
  readSchedules(); 
   
  char schedules[conf.schedule_string_length];
  strcpy(schedules, conf.schedule_string);
    
  response["status"] = "success";
  
  JsonArray& items = response.createNestedArray("data");
  int elements = strlen(schedules);
  int itempos;
  char *tok, *sav1 = NULL;
  tok = strtok_r(schedules, "|", &sav1);
  
  while (tok) 
  {
      JsonObject& item = items.createNestedObject();
      char *subtok, *sav2 = NULL;
      subtok = strtok_r(tok, ":", &sav2);
      itempos = -1;
      
      while (subtok) 
      {
        itempos++;
        
        if(itempos == 0)
        {
          item["o"] = subtok;
        }
        else if(itempos == 1)
        {
          item["h"] = subtok;
        }
        else if(itempos == 2)
        {
          item["m"] = subtok;
        }
        else if(itempos == 3)
        {
          item["s"] = subtok;
        }
        else if(itempos == 4)
        {
          item["d"] = subtok;
        }
        else if(itempos == 5)
        {
          item["rp"] = subtok;
        }
        else if(itempos == 6)
        {
          item["tr"] = subtok;
        }
        else if(itempos == 7)
        {
          item["st"] = subtok;
        }
        else
        {
          items.add(item);
        }
        
        subtok = strtok_r(NULL, ":", &sav2);
      }
            
      tok = strtok_r(NULL, "|", &sav1);
  }

  response.printTo(data);
  server.send(200, "application/json", data); 
}


String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}


/**
 * Get RTC Time
 */
void getClockTime()
{
  String data;
  
  StaticJsonBuffer<100> responseBuffer;
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
  StaticJsonBuffer<100> outBuffer;
  JsonObject& response = outBuffer.createObject();
      
  if (server.hasArg("plain")== false)
  {
    response["status"] = "error";
    response.printTo(data);
    server.send(400, "application/json", data);
  }
  else
  {
      DynamicJsonBuffer inBuffer;
      JsonObject& inObj = inBuffer.parseObject(server.arg("plain"));
      
      // Test if parsing succeeds.
      if (!inObj.success()) {
          response["status"] = "error";
          response.printTo(data);
          server.send(400, "application/json", data);
      }     

      // get schdules
      JsonArray& items = inObj["data"];
      
      // verify that we have less than max schedules
      if(items.size() > MAX_SCHEDULES){
          response["status"] = "error";
          response["message"] = "Requested number of schedules exceeds max allowed!";
          response.printTo(data);
          server.send(400, "application/json", data);
      }

      String content = jsonToSchedules(items);
      unsigned int schedulesStringLength = content.length() + 1;  
      char schedules[schedulesStringLength];
      content.toCharArray(schedules, schedulesStringLength);
      
      //serialize schedules to eeprom
      strcpy(conf.schedule_string, schedules);
      writeSchedules();

      response["status"] = "success";
      response.printTo(data);
      server.send(200, "application/json", data);
  }  
}


/**
 * Converts schedules string to json
 */
String jsonToSchedules(JsonArray& items)
{
  String schedules = "";

  int index = 0;
  for (auto& item : items) {
    int o = item["o"];
    int h = item["h"];
    int m = item["m"];
    int s = item["s"];
    int d = item["d"];
    int r = item["rp"];
    const char* t = item["tr"];
    int st = item["st"];

    schedules = schedules + o + ":" + h + ":" + m + ":" + s + ":" + d + ":" + r + ":" + t + ":" + st; // schedule string
    index++;

    if(index < items.size()){
      schedules = schedules + "|"; // boundary
    }
  }

  return schedules;
}



/**
 * Sets RTC time
 */
void setClockTime()
{  
  String data;

  StaticJsonBuffer<100> responseBuffer;
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


void prepareToEvalSchedules()
{
  // read from eeprom
  if(conf.schedule_string_length == 0){
    readSchedules(); 
  }
   
  char schedules[conf.schedule_string_length];
  strcpy(schedules, conf.schedule_string);
  
  int elements = strlen(schedules);
  int itempos;
  char *tok, *sav1 = NULL;
  tok = strtok_r(schedules, "|", &sav1);

 
  while (tok) 
  {
      Schedule schedule;
    
      char *subtok, *sav2 = NULL;
      subtok = strtok_r(tok, ":", &sav2);
      itempos = -1;
      
      while (subtok) 
      {
        itempos++;
        
        if(itempos == 0)
        {
          schedule.index = (int)subtok;
        }
        else if(itempos == 1)
        {
          schedule.hh = (int)subtok;
        }
        else if(itempos == 2)
        {
          schedule.mm = (int)subtok;
        }
        else if(itempos == 3)
        {
          schedule.ss = (int)subtok;
        }
        else if(itempos == 4)
        {
          schedule.dow = (int)subtok;
        }
        else if(itempos == 5)
        {
          schedule.recurring = (int)subtok;
        }
        else if(itempos == 6)
        {
          schedule.target = subtok;
        }
        else if(itempos == 7)
        {
          schedule.target_state = (int)subtok;
        }
        else
        {
          // collect in array
        }
        
        subtok = strtok_r(NULL, ":", &sav2);
      }
            
      tok = strtok_r(NULL, "|", &sav1);
  }
}


void loop() 
{
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

void writeSchedules()
{
  eeAddress = 50;

  char *schedules = conf.schedule_string;
  conf.schedule_string_length = strlen(schedules);
  debugPrint("Wrting schedules length " + String(conf.schedule_string_length));
  EEPROM.write(eeAddress, conf.schedule_string_length);

  eeAddress++;
  debugPrint("Wrting schedules " + String(conf.schedule_string));
  writeEEPROM(eeAddress, strlen(schedules), schedules);
  
  EEPROM.commit();
}

void writeEEPROM(int startAdr, int len, char* writeString) {
  //yield();
  for (int i = 0; i < len; i++) {
    EEPROM.write(startAdr, writeString[i]);
    startAdr++;
  }
}

void readSchedules()
{
  eeAddress = 50;

  debugPrint("reading schedules length " + String(conf.schedule_string_length));
  conf.schedule_string_length = EEPROM.read(eeAddress);
  
  eeAddress++;
  char schedules[conf.schedule_string_length];
  readEEPROM(eeAddress, conf.schedule_string_length, schedules);
  debugPrint("reading schedules " + String(conf.schedule_string));
  strcpy(conf.schedule_string, schedules);
}

void readEEPROM(int startAdr, int maxLength, char* dest) {
  for (int i = 0; i < maxLength; i++) {
    dest[i] = char(EEPROM.read(startAdr));
    startAdr++;
  }
}

void eraseSettings()
{
  debugPrint("Erasing eeprom...");
  
  for (int i = 0; i < 512; i++)
    EEPROM.write(i, 0);
}

void debugPrint(String message) {
  if (debug) {
    Serial.println(message);
  }
}
