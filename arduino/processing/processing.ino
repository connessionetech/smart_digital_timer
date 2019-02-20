#include <ArduinoJson.h>
#include <EEPROM.h>
#include <stdlib.h>
#include <TimeLib.h>


/* for normal hardware wire use below */
#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS3231.h>
RtcDS3231<TwoWire> Rtc(Wire);
/* for normal hardware wire use above */



int eeAddress = 0;
boolean debug = true;
int counter;

const String processed = "1:20:15:0,1,2,3,4,5:s1:0:1550582771|2:20:15:0,1,2,3,4,5:s1:1:1550582771|3:20:15:0,1,2,3,4,5:s2:0:1550582771|4:20:15:0,1,2,3,4,5:s2:1:1550582771";
const int MAX_SCHEDULES = 20;
const int EEPROM_MAX_LIMIT = 512;
const int EEPROM_START_ADDR = 0;
const unsigned long SECONDS_IN_30_YEARS = 946708560;

struct ScheduleItem {
   int parent_index;
   int hh;
   int mm;
   int dow;
   char* target;
   int target_state;
   long reg_timestamp;
   long scheduled_time;
};


struct Settings {
   int schedule_string_length;
   String schedule_string;
};

ScheduleItem user_schedules[MAX_SCHEDULES] = {};
Settings conf = {};



void setupEeprom()
{
  // start eeprom
  //EEPROM.begin(EEPROM_MAX_LIMIT);
  eeAddress = 0;
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

void setup() 
{
  Serial.begin(57600);
  
  setupEeprom();
  setupRTC();  

  collectSchedule();
  sortSchedule();
  evaluate();
}


void loop() { 
  if (!Rtc.IsDateTimeValid()) 
  {
      Serial.println("RTC lost confidence in the DateTime!");
  }
}


/*
 *  Converts epoch seconds since 1970 jan 1 
 *  to epoch seconds since 2000 jan 1
 */
long fromEpoch(long unsigned seconds)
{
  return seconds - SECONDS_IN_30_YEARS;
}



/*
 *  Converts epoch seconds since 2000 jan 1 
 *  to epoch seconds since 1970 jan 1
 */
long toEpoch(long unsigned seconds)
{
  return seconds + SECONDS_IN_30_YEARS;
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

void evaluate()
{
   
}

void sortSchedule()
{
   
}

void collectSchedule()
{
  debugPrint("collecting");
  
  // clear array
  memset(&user_schedules[0], 0, sizeof(user_schedules));
   
  char sch[processed.length()+1];
  processed.toCharArray(sch, processed.length()+1);
  debugPrint(processed);
  
  int itempos;
  char *tok, *sav1 = NULL;
  tok = strtok_r(sch, "|", &sav1);

  // collect schedules
  counter=0;
  while (tok) 
  {
      int sch_index;
      int sch_hh;
      int sch_mm;
      int sch_days[6];
      int sch_total_days;
      char* sch_target;
      int sch_target_state;
      long sch_timestamp;
    
      char *subtok, *sav2 = NULL;
      subtok = strtok_r(tok, ":", &sav2);
      itempos = -1;
      
      while (subtok) 
      {
        itempos++;
        
        if(itempos == 0)
        {
          sch_index = (int)subtok;
        }
        else if(itempos == 1)
        {
          sch_hh = (int)subtok;
        }
        else if(itempos == 2)
        {
          sch_mm = (int)subtok;
        }
        else if(itempos == 3)
        {
          int total_days = countChars( subtok, ',' ) + 1;
          int days[total_days];
          int j = 0;
          char *toki, *savi = NULL;
          toki = strtok_r(subtok, ",", &savi);
          while (toki) 
          {
            sch_days[j] = (int)toki;
            toki = strtok_r(NULL, ",", &savi);
          }

          sch_total_days = total_days;
        }
        else if(itempos == 4)
        {
          sch_target = subtok;
        }
        else if(itempos == 5)
        {
          sch_target_state = (int)subtok;
        }
        else if(itempos == 6)
        {
          sch_timestamp = strtol(subtok,NULL,10);
        }      
          
        subtok = strtok_r(NULL, ":", &sav2);
      }


      // break into individual schedules
      for(int k=0;k<sch_total_days;k++)
      {
        ScheduleItem item;
        int dow = sch_days[k];

        item.parent_index = sch_index;
        item.hh = sch_hh;
        item.mm = sch_mm;
        item.dow = dow;
        item.target = sch_target;
        item.target_state = sch_target_state;
        item.reg_timestamp = sch_timestamp;
        item.scheduled_time = 0; // eval timestamp
        
        user_schedules[counter] = item; 
        counter++;
      }      

   
      tok = strtok_r(NULL, "|", &sav1);
  }

  debugPrint(String(counter) + "items");
}


void debugPrint(String message) {
  if (debug) {
    Serial.println(message);
  }
}

int countChars( char* s, char c )
{
    return *s == '\0'
              ? 0
              : countChars( s + 1, c ) + (*s == c);
}
