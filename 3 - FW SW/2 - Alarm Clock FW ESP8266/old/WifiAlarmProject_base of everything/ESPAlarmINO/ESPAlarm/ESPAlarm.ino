/*
  ESP Alarm

  ESP Alarmis connected with an Android application via WiFi using ESP8266 module. You can add/modify/delete/activate/de-activate alarms using the Android app when the device is up and connected with the same WiFi network which your phone is connected to.

  This sketch is written for a tutorial published on http://allaboutcircuits.com/

  Please refer to the website to see the tutorial and to download mobile application


  Created on Jan 2017
  Hardware and arduino code is by Yahya Tawil
  yahya.tawil_at_gmail.com
*/

//22 Byte for every alarm
//B0:ID
//B1:Active/Deactive
//B2:Hour
//B3:Minuit
//B4:Rep Day
//B5:sound 1..3
//B6..B21: Title 16 Byte

//Address = ID * 22

//Avalaible EEPROM size in Arduino UNO is 1024B

// 20 Alarm -> 20*22 = 440 B        000..439
// 16B Welcome Message              444..459
// 1B snooze time                   460

//1 Sun //2 Mon //3 Tue //4 Wed //5 Thu //6 Fri //7 Sat

#include <TFT_ILI9163C.h>
#include <SoftwareSerial.h>
#include <DS1307RTC.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <gfxfont.h>

#include "pitches.h"

#define DEBUG 1
//#define SWDebug // Disable(comment)/Enable(uncomment) software serial debugging using (PIN 2,3)
#define HTTPResponse // Disable(comment)/Enable(uncomment) Response from your device as HTTP response
//#define DebugOnTFT // Disable(comment)/Enable(uncomment) Printing debugging information on the TFT screen
#define ESPBaudRate 115200
#define HWSBaudRate 115200
#define SSBaudRate 9600

#define MaxRecordsNum 20
#define MaxRecordSize 22

#define Store_ID_ID 0
#define Store_ID_AD 1
#define Store_ID_HH 2
#define Store_ID_MM 3
#define Store_ID_Rep 4
#define Store_ID_Sound 5
#define Store_ID_Title 6

#define WelcomeMsgSize 16
#define WelcomeMsgAdd 444
#define SnoozeAdd     460

#define ReconnectPressVal 3
#define SnoozePin 6
#define OnOffPin 7
#define BuzzerPin 8

//uncomment Serial.*** if you want to use HW serial port (PIN 0,1) with ESP8266
//uncomment esp8266.*** if you want to use SW serial port (PIN 2,3) with ESP8266
//SoftwareSerial esp8266(2, 3); // make RX Arduino line is pin 2, make TX Arduino line is pin 3.
#define esp8266_Available() Serial.available() //esp8266.available()
#define esp8266_Find(ARG) Serial.find(ARG) //esp8266.find(ARG)
#define esp8266_Read() Serial.read() //esp8266.read()
#define esp8266_Write(ARG1,ARG2) Serial.write(ARG1,ARG2) //esp8266.write(ARG1,ARG2)
#define esp8266_Print(ARG) Serial.print(ARG) //esp8266.print(ARG)

#define TimePosOnScreen() tft.setCursor(10, 18)
#define DatePosOnScreen() tft.setCursor(2, 36)

#define TimeRecXYPos 0, 17
#define DateRecXYPos 0, 35
#define FreeSpaceXYPos 0, 52
#define FaddingXYPos 20, 70

#define TimeRecXYLen 128, 16
#define DateRecXYLen 128, 16

#ifdef SWDebug
SoftwareSerial SWSerial(2, 3); // make RX Arduino line is pin 2, make TX Arduino line is pin 3.
#endif
/*************/

tmElements_t tm;
String IP;

//const char *monthName[12]  = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
//const char *DayName[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

char RTC_Sec, RTC_Min, RTC_Hour, RTC_day, RTC_Mon, RTC_year, DayOfWeek;
char Sound_en, S_loop, snoozeTime, snoozeTimer, Sound_enTimer, activealarm, PressLong, SnoozePressLong;

unsigned long Active_Alarms_H [24];
unsigned long Active_Alarms_M [60];
unsigned long Active_Alarms_D [7];
unsigned char SnoozedAlarmsTimer[MaxRecordsNum];
char Rep_Alarms[MaxRecordsNum];
unsigned long SnoozedAlarms;

/*************/

/***Commands**/
#define Set "SET"
#define Add "ADD" 
#define Del "DEL" 
#define Sync "SYN" 
#define Debug "DEB"
#define ResetMem "RES"

#define strResponseToSet "CT"
#define strResponseToAdd "AT"
#define strResponseToDel "DT"
#define strResponseToSyn "ST"
#define strResponseToRes "RT"
/*************/

// Color definitions
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define AACOrange 0xFC09

#define __CS 10
#define __DC 9

typedef struct _Alarm_t
{
  int ID;
  char AD;
  int Hour;
  int Minuit;
  int Rep;
  int sound;
  String Title;

} Alarm_t;

TFT_ILI9163C tft = TFT_ILI9163C(__CS, __DC);

void setup()
{
  tft.begin(); // Init for TFT screen

  Wire.begin(); // Init I2C for RTC chip

  Serial.begin(HWSBaudRate); // Serial port to send messages from arduino to computer

#ifdef SWDebug
  SWSerial.begin(SSBaudRate); // Software Serial port for printing debug messages
#endif

  PrintAACIntro(); //Print On the screen AAC Logo

  ConfigESP8266(); //Set Cconfiguration for ESP8266 like SSID, password, port number, ... etc

  ResetDateTime(); //Set default time for first run before sync with mobile time

  LoadArraies(); //load processing arries with values from EEPROM

  PrintInit(); // Print some informations on the screen

  digitalWrite(OnOffPin, true);
  digitalWrite(SnoozePin, true);

  pinMode(OnOffPin, INPUT_PULLUP);
  pinMode(SnoozePin, INPUT_PULLUP);
}


void loop()
{

#ifdef SWDebug
  if (SWSerial.read() == 'r')
  {
    for (int i = 0; i < 1024; i++)
      EEPROM.write(i, 0xFF);

    SWSerial.println("EEPROM Is Clear Now!");
  }
  if (SWSerial.read() == 'p')
    PrintStoredAlarms();
#endif

  if (PressLong > ReconnectPressVal)
  {
    PressLong = 0 ;
    setup(); //reconnect
  }

  if (Sound_en) // snoozeTime
  {

    if (S_loop)
    {
      tone(BuzzerPin, NOTE_C4);
    }
    else
      noTone(BuzzerPin);

    if (Sound_enTimer == 20 || PressLong > 0 || SnoozePressLong > 0)
    {
      Sound_en = 0;
      noTone(8);

      if (SnoozePressLong > 0)
      {
        SnoozePressLong = 0;
        SnoozedAlarms |= 1 << activealarm;
        SnoozedAlarmsTimer[activealarm];
      }
      else
      {
        PressLong = 0;
        unsigned char HH = LoadFromMem(activealarm, Store_ID_HH);
        unsigned char MM = LoadFromMem(activealarm, Store_ID_MM);

        if (Rep_Alarms[activealarm] != 'r')
        {
          Rep_Alarms[activealarm] = 'D' ;
          UpdateRec(activealarm, Store_ID_AD, 'D');
          Active_Alarms_H[HH] = Active_Alarms_H[HH] & (~(1 << activealarm));
          Active_Alarms_M[MM] = Active_Alarms_M[MM] & (~(1 << activealarm));
          Active_Alarms_D[DayOfWeek - 1] = Active_Alarms_D[DayOfWeek - 1] & (~(1 << activealarm));
        }
      }
      tft.fillRect(FreeSpaceXYPos, 128, 128, BLACK);
      FaddingText("OFF", FaddingXYPos);

    }
  }
  else
  {
    noTone(BuzzerPin);
  }

  RTC.read(tm);

  if (RTC_Hour != tm.Hour || RTC_Min != tm.Minute  || RTC_Sec != tm.Second )
  {
    Sound_enTimer++;
    S_loop ^= 1;

    if (digitalRead(OnOffPin) == false)
      PressLong++;

    if (digitalRead(SnoozePin) == false)
      SnoozePressLong++;

    unsigned long activeAlarms;
    if (RTC_Min != tm.Minute )
    {
      snoozeTimer++;
      activeAlarms = Active_Alarms_H[tm.Hour] & Active_Alarms_M[tm.Minute] & Active_Alarms_D[DayOfWeek - 1];
      for (int i = 0; i < MaxRecordsNum; i++)
      {
        if (SnoozedAlarms & (1 << i) && (SnoozedAlarmsTimer[i] < snoozeTime))
          SnoozedAlarmsTimer[i]++;
      }
      for (int i = 0; i < MaxRecordsNum; i++)
      {
        if ((activeAlarms & (1 << i)) &&  ((Rep_Alarms[i] == 'r') || ((Rep_Alarms[i] == 'O'))))
        {
          Sound_en = 1;
          Sound_enTimer = 0;
          activealarm = i;
#ifdef DebugOnTFT
          FaddingText("ON", FaddingXYPos);
#endif
          PrintAlarmTitle(i);
          break;
        }
      }
      for (int i = 0; i < MaxRecordsNum; i++)
      {
        if ((SnoozedAlarms & (1 << i)) && (SnoozedAlarmsTimer[i] == snoozeTime))
        {
          Sound_en = 1;
          Sound_enTimer = 0;
          SnoozePressLong = 0;
          activealarm = i;
          if (SnoozedAlarms & (1 << activealarm))
            SnoozedAlarms &= (~( 1 << activealarm));
#ifdef DebugOnTFT
          tft.setCursor(FreeSpaceXYPos);
          tft.setTextColor(WHITE);
          tft.setTextSize(1);
          tft.print(SnoozedAlarms);
          delay(2000);
          tft.fillRect(FreeSpaceXYPos, 128, 128, BLACK);

          String FaddingT = "Snoozed";
          FaddingT += activealarm;
          FaddingText(FaddingT, FaddingXYPos);
#endif
          PrintAlarmTitle(i);
          break;
        }
      }
    }

    RTC_Hour = tm.Hour;
    RTC_Min = tm.Minute;
    RTC_Sec = tm.Second;

    PrintTimeOnScreen(tm);
  }
  if (RTC_day != tm.Day || RTC_Mon != tm.Month || RTC_year != tm.Year)
  {
    if (RTC_day != tm.Day)
    {
      while (Sound_en); //don't change the day before finishing the current alarm
      if (DayOfWeek == 7)
        DayOfWeek = 1;
      else
        DayOfWeek++;
    }

    RTC_day = tm.Day;
    RTC_Mon = tm.Month;
    RTC_year = tm.Year;

    PrintDateOnScreen(tm);//todo refresh day of the week
  }
  if (esp8266_Available()) // check if the ESP is sending a message
  {
    if (esp8266_Find("+IPD,"))
    {
      delay(1000); // wait for the serial buffer to fill up (read all the serial data)

      int connectionId = esp8266_Read() - 48; // get the connection id so that we can then disconnect
      // subtract 48 because the read() function returns the ASCII decimal value and 0 (the first decimal number) starts at 48

      String closeCommand = "AT+CIPCLOSE=";//Setting up the close connecting command
      closeCommand += connectionId; // append connection id
      closeCommand += "\r\n";

      esp8266_Find('?'); // This character defines the start of commands in the our message body

      String InStream;
      InStream = (char) esp8266_Read();
      InStream += (char) esp8266_Read();
      InStream += (char) esp8266_Read();

#ifdef SWDebug
      if (DEBUG)
        SWSerial.println(InStream);
#endif
      if (InStream.equals(Sync)) //Example: SYN,08+20+2016,09:28:56,1
      {
        esp8266_Read();//Read ','

        char DBuf[10]; // 08+20+2016
        char TBuf[8];  // 09:28:56
        for (int i = 0; i < 10; i++)
          DBuf[i] = esp8266_Read();
        getDate(DBuf);

        esp8266_Read();//Read ','

        for (int i = 0; i < 8; i++)
          TBuf[i] = esp8266_Read();
        getTime(TBuf);

        RTC.write(tm);

        esp8266_Read();//Read ','

        DayOfWeek = esp8266_Read() - 48;

        RTC_day = tm.Day;
        RTC_Mon = tm.Month;
        RTC_year = tm.Year;
        RTC_Hour = tm.Hour;
        RTC_Min = tm.Minute;
        RTC_Sec = tm.Second;

        PrintDateOnScreen(tm);
        PrintTimeOnScreen(tm);

#ifdef HTTPResponse
        sendHTTPResponse(connectionId, strResponseToSyn);
#else
        sendResponse(connectionId, ResponseToSyn);
#endif
        sendCommand(closeCommand, 1000, DEBUG); // close connection

        FaddingText("Sync", FaddingXYPos);
      }

      else if (InStream.equals(Add) || InStream.equals(Del)) // ADD,ID,A(active)/D(deactive),HHMM,RepDays,SoundType,Title
      {
        Alarm_t Alarm_temp;

        esp8266_Read();//Read ','

        int Alarm_ID = (esp8266_Read() - 48);
        int Alarm_ID_dig2 = esp8266_Read();
        if (Alarm_ID_dig2 == ',')
          Alarm_temp.ID = Alarm_ID;
        else
        {
          Alarm_ID *= 10;
          Alarm_ID += (Alarm_ID_dig2 - 48);
          Alarm_temp.ID = Alarm_ID - 1;
          esp8266_Read();//Read ','
        }

        if (Alarm_ID > MaxRecordsNum)
        {
#ifdef HTTPResponse
          sendHTTPResponse(connectionId, "ERR");
#else
          sendResponse(connectionId, "ERR");
#endif
          sendCommand(closeCommand, 1000, DEBUG); // close connection
        }

        char Alarm_AD;

        Alarm_AD = esp8266_Read();

        Alarm_temp.AD = Alarm_AD ;

        if (Alarm_AD == 'A')
        {
          esp8266_Read();//Read ','
          int Alarm_HH = (esp8266_Read() - 48);
          int Alarm_HH_dig2 = (esp8266_Read() - 48);
          if (Alarm_HH_dig2 >= 0 && Alarm_HH_dig2 <= 9)
          {
            Alarm_HH *= 10;
            Alarm_HH += Alarm_HH_dig2;
          }

          Alarm_temp.Hour = Alarm_HH ;

          int Alarm_MM = (esp8266_Read() - 48);
          int Alarm_MM_dig2 = (esp8266_Read() - 48);
          if (Alarm_MM_dig2 >= 0 && Alarm_MM_dig2 <= 9)
          {
            Alarm_MM *= 10;
            Alarm_MM += Alarm_MM_dig2;
          }

          Alarm_temp.Minuit = Alarm_MM;
          esp8266_Read();//Read ','

          char Alarm_Rep = esp8266_Read() - 48;
          bool Rep_day[7] = {0, 0, 0, 0, 0, 0, 0}; // Sun Mon Tue Wed Thu Fri Sat
          int HEX_val;
          if ((Alarm_Rep + 48) == 'O' || (Alarm_Rep + 48) == 'F') // O - no repeat , F - all days , ex: 123 means repeat alarm on Sun+Mon+Tue
          {
            HEX_val = Alarm_Rep + 48;
            esp8266_Read();//Read ','
          }
          else
          {
            for (int i = 0; i < 6; i++)
            {
              if ((Alarm_Rep + 48) == ',') //end of rep segment
                break;
              else
              {
                Rep_day[Alarm_Rep - 1] = 1; // ex: Rep_day[1-1(Sun)]=1
                Alarm_Rep = esp8266_Read() - 48;
              }
            }
            HEX_val = 0;
            for (int i = 0; i < 7; i++) // convert to hex value
            {
              if (Rep_day[i])
              {
                HEX_val |= (1 << i);
              }
            }
          }
          Alarm_temp.Rep = HEX_val;

          int Alarm_ST = (esp8266_Read() - 48);
          Alarm_temp.sound = Alarm_ST;

          esp8266_Read();//Read ','

          char InStream;
          String Alarm_title;

          for (int i = 0; i < 16; i++)
          {
            InStream = esp8266_Read();
            Alarm_title += InStream;
            if (InStream == ';')
              break;
          }
          Alarm_temp.Title = Alarm_title;

          UpdateArraies(Alarm_temp);
          StoreAlarmToEEPROM(Alarm_temp);
        }
        else if (Alarm_AD == 'D' )
        {
          UpdateRec(Alarm_ID, Store_ID_AD, 'D');
          Rep_Alarms[Alarm_ID] = 'D';
        }
String Response;
          if(InStream.equals(Del))
          {
#ifdef HTTPResponse
          sendHTTPResponse(connectionId, strResponseToDel);
#else
          sendResponse(connectionId, strResponseToDel);
#endif
        sendCommand(closeCommand, 1000 , DEBUG); // close connection
          FaddingText("Deleted", FaddingXYPos);
          }

          else if(InStream.equals(Add))
          {
        Response = strResponseToAdd;
        Response +=  Alarm_ID;
#ifdef HTTPResponse
        sendHTTPResponse(connectionId, Response);
#else
        sendResponse(connectionId, Response);
#endif
        sendCommand(closeCommand, 1000 , DEBUG); // close connection

        FaddingText("Added!", FaddingXYPos);
          }

#ifdef DebugOnTFT
        tft.setTextSize(1);
        tft.setTextColor(WHITE);
        tft.setCursor(FreeSpaceXYPos);
        for (int j = 0; j <  7; j++)
        {
          tft.print("D>");
          tft.print(Active_Alarms_D[j]);
        }
        tft.print(",M>");
        tft.print(Active_Alarms_M[Alarm_temp.Minuit]);
        tft.print(",H>");
        tft.print(Active_Alarms_H[Alarm_temp.Hour]);
        tft.print(",>R");
        tft.print(Rep_Alarms[Alarm_ID]);
        delay(10000);
        tft.fillRect(FreeSpaceXYPos, 128, 128, BLACK);

        tft.setCursor(FreeSpaceXYPos);
        for (int j = 0; j <  MaxRecordSize; j++)
        {
          tft.print(EEPROM.read(Alarm_ID * MaxRecordSize + j));
          tft.print(',');
        }
        delay(10000);
        tft.fillRect(FreeSpaceXYPos, 128, 128, BLACK);
#endif

      }

      else if (InStream.equals(Set)) // ex: SET,5,Hello; --- 5Min snooze time and "Hello" is the welcome meassage
      {
        char temp, temp_dig2, SnoozeTime;
        char msg[16];
        esp8266_Read();//Read ','
        temp = esp8266_Read() - 48;
        temp_dig2 = esp8266_Read();
        if (temp_dig2 == ',')
          SnoozeTime = temp;
        else
          SnoozeTime = temp * 10 + (temp_dig2 - 48);
        for (int i = 0; i < 16; i++)
        {
          temp = esp8266_Read();
          msg[i] = temp;
          if (temp == ';')
            break;
        }

#ifdef HTTPResponse
        sendHTTPResponse(connectionId, strResponseToSet);
#else
        sendResponse(connectionId, ResponseToSet);
#endif
        sendCommand(closeCommand, 1000 , DEBUG); // close connection

        UpdateSetting(msg, SnoozeTime);
        FaddingText("Set!", FaddingXYPos);
      }
#ifdef DebugOnTFT
      else if (InStream.equals(Debug)) //Printing bebugging informations on the screen
      {
        //  SWSerial.println("\tID\tAD\tHour\tMin\tRep\tsound");

        tft.setTextSize(1);
        tft.setTextColor(WHITE);
        tft.setCursor(FreeSpaceXYPos);
        for (int i = 0; i < MaxRecordsNum; i++) // 20 Alarm
        {
          tft.setTextColor(RED);
          tft.print(i);
          tft.print("#");
          tft.setTextColor(WHITE);
          for (int j = 0; j <  MaxRecordSize; j++)
          {
            tft.print(EEPROM.read(i * MaxRecordSize + j));
            tft.print('|');
          }
          if (tft.getCursorY() > 90 || i == (MaxRecordsNum - 1))
          {
            delay(5000);
            tft.fillRect(FreeSpaceXYPos, 128, 128, BLACK);
            tft.setCursor(FreeSpaceXYPos);
          }

        }

        for (int j = 0; j <  60; j++)
        {
          tft.setTextColor(RED);
          tft.print(j);
          tft.print("M>");
          tft.setTextColor(WHITE);
          tft.println(Active_Alarms_M[j]);
          if (tft.getCursorY() > 120 || j == 59)
          {
            delay(3000);
            tft.fillRect(FreeSpaceXYPos, 128, 128, BLACK);
            tft.setCursor(FreeSpaceXYPos);
          }
        }

        for (int j = 0; j <  24; j++)
        {
          tft.setTextColor(RED);
          tft.print(j);
          tft.print("H>");
          tft.setTextColor(WHITE);
          tft.println(Active_Alarms_H[j]);
          if (tft.getCursorY() > 120 || j == 23)
          {
            delay(3000);
            tft.fillRect(FreeSpaceXYPos, 128, 128, BLACK);
            tft.setCursor(FreeSpaceXYPos);
          }
        }

        for (int j = 0; j <  7; j++)
        {
          tft.setTextColor(RED);
          tft.print(j);
          tft.print("D>");
          tft.setTextColor(WHITE);
          tft.println(Active_Alarms_D[j]);
          if (tft.getCursorY() > 120 || j == 6 )
          {
            delay(3000);
            tft.fillRect(FreeSpaceXYPos, 128, 128, BLACK);
            tft.setCursor(FreeSpaceXYPos);
          }
        }

        for (int j = 0; j <  MaxRecordsNum; j++)
        {
          tft.setTextColor(RED);
          tft.print(j);
          tft.print(">R");
          tft.setTextColor(WHITE);
          tft.println(Rep_Alarms[j]);
          if (tft.getCursorY() > 120 || j == (MaxRecordsNum - 1))
          {
            delay(3000);
            tft.fillRect(FreeSpaceXYPos, 128, 128, BLACK);
            tft.setCursor(FreeSpaceXYPos);
          }
        }

      }
#endif
      else if (InStream.equals(ResetMem))
      {
        sendCommand(closeCommand, 1000 , DEBUG); // close connection
        for (int i = 0; i < 1024; i++)
          EEPROM.write(i, 0xFF);

#ifdef HTTPResponse
        sendHTTPResponse(connectionId, strResponseToRes);
#else
        sendResponse(connectionId, ResponseToRes);
#endif

        FaddingText("RESET", FaddingXYPos);
        setup();
      }

      else // unsupported command is recived
      {
#ifdef HTTPResponse
        sendHTTPResponse(connectionId, "ERR");
#else
        sendResponse(connectionId, "ERR");
#endif
        sendCommand(closeCommand, 1000, DEBUG); // close connection
      }
    }
  }
}

/**********************************Start********************************/
/****************************Memory Functions***************************/
/***********************************************************************/

void UpdateSetting(char * msg, char snooze)
{
  for (int i = 0; i < WelcomeMsgSize; i++)
  {
    EEPROM.write(WelcomeMsgAdd + i, *(msg + i));
    if (*(msg + i) == ';')
      break;
  }
  EEPROM.write(SnoozeAdd, snooze);
  snoozeTime = snooze;
}


unsigned char LoadFromMem(int id, int Store_id)
{
  unsigned int add = id * MaxRecordSize + Store_id   ;
  return EEPROM.read(add);
}

void StoreAlarmToEEPROM(Alarm_t alarm)
{
  //store number in EEPROM
  unsigned int add = alarm.ID * MaxRecordSize;

#ifdef SWDebug
  SWSerial.println("Alarm:");
  SWSerial.println(alarm.ID);
  SWSerial.println(alarm.AD);
  SWSerial.println(alarm.Hour);
  SWSerial.println(alarm.Minuit);
  SWSerial.println(alarm.Rep);
  SWSerial.println(alarm.sound);
#endif
  EEPROM.update(add++, (char)alarm.ID);
  EEPROM.update(add++, (char)alarm.AD);
  EEPROM.update(add++, (char)alarm.Hour);
  EEPROM.update(add++, (char)alarm.Minuit);
  EEPROM.update(add++, (char)alarm.Rep);
  EEPROM.update(add++, (char)alarm.sound);

  for (int i = 0; i < alarm.Title.length(); i++)
    EEPROM.update(add++, alarm.Title[i]);

}

void UpdateRec(int id, int Store_id, int val)
{
  unsigned int add = id  * MaxRecordSize + Store_id;
  EEPROM.write(add, val);
}

void DelfromMem(int ID)
{
  for (int i = 0; i < MaxRecordSize; i++)
    EEPROM.write((ID  * MaxRecordSize ) + i, 0xFF);
}

void PrintStoredAlarms()
{
#ifdef SWDebug
  SWSerial.println("\tID\tAD\tHour\tMin\tRep\tsound");
  for (int i = 0; i < MaxRecordsNum; i++) // 20 Alarm
  {

    SWSerial.print(i);
    SWSerial.print('\t');
    for (int j = 0; j <  MaxRecordSize; j++)
    {
      SWSerial.print(EEPROM.read(i * MaxRecordSize + j));
      SWSerial.print("\t");
    }
    SWSerial.print("\n");
  }
#endif
}

void LoadArraies()
{
  unsigned char storedH, storedM, storedD, storedA;
  snoozeTime = 5; //default value

  for (int i = 0; i < MaxRecordsNum; i++)
  {
    storedA = EEPROM.read(i * MaxRecordSize + Store_ID_AD);
    if (storedA == 'D')
    {
      Rep_Alarms[i] = storedA;
      continue;
    }
    else if (storedA == 'A')
    {
      storedH = EEPROM.read(i * MaxRecordSize + Store_ID_HH);
      storedM = EEPROM.read(i * MaxRecordSize + Store_ID_MM);
      storedD = EEPROM.read(i * MaxRecordSize + Store_ID_Rep);

      if (storedH > 24 || storedM > 60)
        continue;
      else
      {
        Active_Alarms_H[storedH] |= 1 << i;
        Active_Alarms_M[storedM] |= 1 << i;
      }

      if (storedD == 'O')
      {
        Active_Alarms_D[DayOfWeek - 1] |= 1 << i;
        Rep_Alarms[i] = storedD;
        continue;
      }
      for (int j = 0; j < 7; j++)
      {
        if (storedD & (1 << j) || storedD == 'F' )
        {
          Active_Alarms_D[j] |= 1 << i;
          Rep_Alarms[i] = 'r';
        }
      }
    }
  }
}

void UpdateArraies(Alarm_t Alarm_temp)
{
  unsigned char H, M, D;

  if (Rep_Alarms[Alarm_temp.ID] == 'r' || Rep_Alarms[Alarm_temp.ID] == 'O')
  {
    H = EEPROM.read(Alarm_temp.ID * MaxRecordSize + Store_ID_HH);
    M = EEPROM.read(Alarm_temp.ID * MaxRecordSize + Store_ID_MM);

    Active_Alarms_H[H] = Active_Alarms_H[H] & (~(1 << Alarm_temp.ID));
    Active_Alarms_M[M] = Active_Alarms_M[M] & (~(1 << Alarm_temp.ID));
    for (int i = 0; i < 7; i++)
      Active_Alarms_D[i] = Active_Alarms_D[i] & (~(1 << Alarm_temp.ID));
    Rep_Alarms[Alarm_temp.ID] = 'D';
  }

  Active_Alarms_H[Alarm_temp.Hour] |= 1 << Alarm_temp.ID;
  Active_Alarms_M[Alarm_temp.Minuit] |= 1 << Alarm_temp.ID;
  D = Alarm_temp.Rep;
  if (D == 'O')
  {
    Rep_Alarms[Alarm_temp.ID] = 'O';
    Active_Alarms_D[DayOfWeek - 1] |= 1 << Alarm_temp.ID;
    return;
  }
  for (int j = 0; j < 7; j++)
  {
    if (D & (1 << j) || D == 'F' )
    {
      Active_Alarms_D[j] |= 1 << Alarm_temp.ID;
    }
  }
  Rep_Alarms[Alarm_temp.ID] = 'r';
}

/*********************************END***********************************/
/****************************Memory Functions***************************/
/***********************************************************************/

/**********************************Start********************************/
/****************************Date Time Functions************************/
/***********************************************************************/

void ResetDateTime()
{
  getDate("01+01+2000");
  getTime("00:00:00");
  DayOfWeek = 1;

  RTC.write(tm);
  delay(100);
  RTC.read(tm);

  RTC_Hour = tm.Hour;
  RTC_Min = tm.Minute;
  RTC_Sec = tm.Second;
  RTC_day = tm.Day;
  RTC_Mon = tm.Month;
  RTC_year = tm.Year;
}

bool getTime(const char *str)
{
  int Hour, Min, Sec;
  if (sscanf(str, "%d:%d:%d", &Hour, &Min, &Sec) != 3) return false;
  tm.Hour = Hour;
  tm.Minute = Min;
  tm.Second = Sec;
  return true;
}

bool getDate(const char *str)
{
  int Month, Day, Year;
  if (sscanf(str, "%d+%d+%d", &Month, &Day, &Year) != 3) return false;
  tm.Day = Day;
  tm.Month = Month;
  tm.Year = CalendarYrToTm(Year);
  return true;
}

/*********************************END***********************************/
/****************************Date Time Functions************************/
/***********************************************************************/

/**********************************Start********************************/
/****************************TFT Screen Functions***********************/
/***********************************************************************/
void tftprint2digits(int number)
{
  if (number >= 0 && number < 10) {
    tft.print('0');
  }
  tft.print(number);
}

void PrintTimeOnScreen(tmElements_t t)
{
  tft.fillRect(TimeRecXYPos, TimeRecXYLen, WHITE);
  TimePosOnScreen();
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tftprint2digits(t.Hour);
  tft.write(':');
  tftprint2digits(t.Minute);
  tft.write(':');
  tftprint2digits(t.Second);
  tft.print("\r\n");
}

void PrintDayOfWeek()
{
  tft.setCursor(120, 2);
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tft.print('D');
  tft.drawRoundRect(110, 0, 17, 12, 3, WHITE);
  tft.fillRoundRect(113, 1, 6, 10, 3, BLACK);
  tft.setCursor(113, 2);
  tft.setTextColor(WHITE);
  tft.print((int)DayOfWeek);
}
void PrintDateOnScreen(tmElements_t t)
{
  tft.fillRect(DateRecXYPos, DateRecXYLen, WHITE);
  DatePosOnScreen();
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tft.print(t.Day);
  tft.print('/');
  //tft.print(monthName[t.Month - 1]);
  tft.print(t.Month);
  tft.write('/');
  tft.print(tmYearToCalendar(t.Year));
  PrintDayOfWeek();
}

void PrintAACIntro()
{
  tft.fillScreen();
  tft.drawLine(0, 15, 128, 15, WHITE);
  tft.drawRoundRect(16, 20, 20, 28, 3, WHITE);
  tft.fillRoundRect(9, 25, 8, 3, 1, WHITE);
  tft.fillRoundRect(9, 33, 8, 3, 1, WHITE);
  tft.fillRoundRect(9, 40, 8, 3, 1, WHITE);
  tft.fillRoundRect(35, 25, 8, 3, 1, WHITE);
  tft.fillRoundRect(35, 33, 8, 3, 1, WHITE);
  tft.fillRoundRect(35, 40, 8, 3, 1, WHITE);
  tft.fillRoundRect(50, 20, 3, 28, 0, WHITE);
  tft.setCursor(15, 50);
  tft.setTextColor(AACOrange);
  tft.setTextSize(1);
  tft.println("ALL ABOUT");
  tft.setCursor(15, 60);
  tft.setTextSize(2);
  tft.println("CIRCUITS");
  tft.drawLine(0, 75, 128, 75, WHITE);
}

void PrintWelcomMsg()
{
  delay(2000);
  tft.fillScreen();
  tft.setCursor(0, 0);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  unsigned char temp;

  temp = EEPROM.read(WelcomeMsgAdd);
  if (temp == 255)
  {
    return;
  }

  tft.setTextSize(2);
  for (int i = 0; i < 16; i++)
  {
    temp = EEPROM.read(WelcomeMsgAdd + i);
    if (temp == ';')
      break;
    else
      tft.print((char)temp);
  }
  delay(3000);
}

void PrintConInfo()
{
  tft.fillScreen();
  tft.setCursor(0, 0);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.print(IP);
  tft.print(",1337");
}

void PrintInit()
{
  PrintWelcomMsg();
  PrintConInfo();
  PrintTimeOnScreen(tm);
  PrintDateOnScreen(tm);
}

void TextOnScreen(String Text, char x, char y)
{
  tft.setCursor(x, y);
  tft.print(Text);
  delay(200);
}

void PrintAlarmTitle(unsigned char id)
{
  unsigned int add = id * MaxRecordSize + Store_ID_Title ;
  tft.setCursor(0, 70);
  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  for (int i = 0; i < 16 && EEPROM.read(add + i) != ';' ; i++)
  {
    tft.print((char)EEPROM.read(add + i));
  }
}

void FaddingText(String Text, char x, char y)
{
  tft.setTextSize(3);

  tft.setTextColor(WHITE);
  TextOnScreen(Text, x, y);
  delay(800);

  tft.setTextColor(0xBDF7);
  TextOnScreen(Text, x, y);

  tft.setTextColor(0x9CB2);
  TextOnScreen(Text, x, y);

  tft.setTextColor(0x738E);
  TextOnScreen(Text, x, y);

  tft.setTextColor(0x5AAA);
  TextOnScreen(Text, x, y);

  tft.setTextColor(0x39A6);
  TextOnScreen(Text, x, y);

  tft.setTextColor(0x18E3);
  TextOnScreen(Text, x, y);

  tft.setTextColor(BLACK);
  TextOnScreen(Text, x, y);
}

/*********************************END***********************************/
/****************************TFT Screen Functions***********************/
/***********************************************************************/


/**********************************Start********************************/
/******************Communication With Module Functions******************/
/***********************************************************************/
bool ConfigESP8266()
{
  String res;
  // esp8266.begin(ESPBaudRate); // Software Serial port to send messages from arduino to ESP8266
  // IMPORTANT: don't miss with the delays !
  tft.setTextColor(AACOrange);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.print("Connecting");//85,5,5
  sendCommand("AT+RST\r\n", 1000, DEBUG); // reset module
  tft.print("#");
  sendCommand("AT+CWMODE=1\r\n", 1000, DEBUG); // configure as access point
  tft.print("#");
  sendCommand("AT+CWJAP=\"YAAT\",\"66204146630408\"\r\n", 1000, DEBUG); //**** CHANGE SSID and PASSWORD ACCORDING TO YOUR NETWORK ******//
  tft.print("#");//We need to wait 5Sec after CWJAP command
  delay(1000);
  tft.print("#");
  delay(1000);
  tft.print("#");
  delay(1000);
  tft.print("#");
  delay(1000);
  tft.print("#");
  sendCommand("AT+CIPMUX=1\r\n", 1000, DEBUG); // configure for multiple connections
  tft.print("#");
  sendCommand("AT+CIPSERVER=1,1337\r\n", 1000, DEBUG); // turn on server on port 1337
  tft.print("#");
  IP = sendCommand("AT+CIFSR\r\n", 1000, DEBUG); // get ip address

  tft.fillScreen(AACOrange);
  tft.setTextSize(2);
  tft.setCursor(0, 64);
  tft.setTextColor(WHITE);
  if (IP.length() <= 25)
  {
    IP = "NOT Avaliable";
    tft.print(" Not \r\n Connected :(");
    return 0;
  }
  else
  {
    IP = IP.substring(25, 38); // Get the IP part from the command response
    tft.print("Connected :)");
    return 1;
  }
}

String sendCommand(String command, const int timeout, bool debug)
{
  String response = "";

  esp8266_Print(command); // send the read character to the esp8266

  long int time = millis();

  while ((time + timeout) > millis())
  {
    while (esp8266_Available()) // Read the response from ESP
    {
      char c = esp8266_Read();
      response += c;
    }
  }
#ifdef SWDebug
  if (debug == true)
    SWSerial.print(response);
#endif

  return response;
}

String sendData(String command, const int timeout, bool debug)
{
  String response = "";

  int dataSize = command.length();
  char data[dataSize];
  command.toCharArray(data, dataSize);

  esp8266_Write(data, dataSize); // send the read character to the esp8266

  long int time = millis();

  while ( (time + timeout) > millis())
  {
    while (esp8266_Available()) // Read the response from ESP
    {
      char c = esp8266_Read();
      response += c;
    }
  }
#ifdef SWDebug
  if (debug == true)
  {
    Serial.println("\r\n====== Data From Arduino ======");
    Serial.write(data, dataSize);
    Serial.println("\r\n========================================");
    SWSerial.print(response);
  }
#endif

  return response;
}

void sendResponse(int connectionId, String content)
{
  String cipSend = "AT+CIPSEND=";
  cipSend += connectionId;
  cipSend += ",";
  cipSend += content.length();
  cipSend += "\r\n";
  content += " "; // There is a bug in this code: the last character of "content" is not sent, I cheated by adding this extra space
  sendCommand(cipSend, 1000, DEBUG);
  sendData(content, 1000, DEBUG);
}

//#ifdef HTTPResponse
//void sendHTTPResponse(int connectionId, String content)
//{
//  // build HTTP response
//  String httpResponse;
//  String httpHeader;
//  httpHeader = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n";
//  httpHeader += "Content-Length: ";
//  httpHeader += content.length();
//  httpHeader += "\r\n";
//  httpHeader += "Connection: close\r\n\r\n";
//  httpResponse = httpHeader + content + " "; // There is a bug in this code: the last character of "content" is not sent, I cheated by adding this extra space
//  String cipSend = "AT+CIPSEND=";
//  cipSend += connectionId;
//  cipSend += ",";
//  cipSend += httpResponse.length();
//  cipSend += "\r\n";
//  sendCommand(cipSend, 1000, DEBUG);
//  sendData(httpResponse, 1000, DEBUG);
//}
//#endif

const char httpHeader1[] PROGMEM = {"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n"}; // 57 char

#ifdef HTTPResponse
void sendHTTPResponse(int connectionId, String content)
{
  // build HTTP response
  String httpResponse;
  String httpHeader;
  httpHeader = "Content-Length: ";
  httpHeader += content.length();
  httpHeader += "\r\n";
  httpHeader += "Connection: close\r\n\r\n";
  httpResponse = httpHeader + content + " "; // There is a bug in this code: the last character of "content" is not sent, I cheated by adding this extra space
  String cipSend = "AT+CIPSEND=";
  cipSend += connectionId;
  cipSend += ",";
  cipSend += httpResponse.length() + 57;
  cipSend += "\r\n";
  sendCommand(cipSend, 1000, DEBUG);
  for (int i = 0; i < 57; i++)
  {
    char myChar = pgm_read_byte_near(httpHeader1 + i);
    Serial.print(myChar);
  }
  sendData(httpResponse, 1000, DEBUG);
}
#endif

/*********************************END***********************************/
/******************Communication With Module Functions******************/
/***********************************************************************/
