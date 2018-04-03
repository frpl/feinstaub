#include <DHT.h>
#include <DHT_U.h>

#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <string.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// get the SDFat-Library from here: https://github.com/greiman/SdFat/tree/master/SdFat

#include <SdFat.h>
#include <SdFatUtil.h>

/* FP, 2017 (frplgmx.de)
   this code is licensed under CC BY-NC-SA 4.0 -
   http://creativecommons.org/licenses/by-nc-sa/4.0/
*/

// for debugging the SDS011 sensor has to be removed !

// #define SERDEBUG 1

/* 
   in the original configuration an "Ardulogger" board is being used. This board
	 features an SD-card slot (SPI-port), RTC clock (I2C port) and reacts like
	 an Arduino UNO (with serial programming interface). The board is available in
	 small quantities only, but any UNO with additional SD-card module and RTC module
	 will do fine.
	 
   programming the Ardulogger board:
   - supply power to the Ardulogger board
   - connect a FTDI converter (f.e. the Adafruit FTDI friend), set to 3.3V
   - use these settings: Arduino Uno, FTDI com port, AVRISP mkII
*/

#define DHTPIN  A1
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

float t, f;
int temp, feuchte;

unsigned pm25;
unsigned pm10;
float fpm25;
float fpm10;
unsigned sec; // second

RTC_DS1307 rtc;

const uint8_t sdChipSelect = 10; // chipselect
const uint8_t spiSpeed = SPI_HALF_SPEED;
SdFat sd;
SdFile myfile;

// some definitions

// the version number is written to the myfile
char version[16] = "FS-Logger 1.0"; // new SdFat lib

String line;
String datelin;

DateTime rtctime;

float sensor_value;

char cbuffer[80];            // character buffer for different purposes
char datestr[20];            // set date string
char timestr[20];            // set time string

LiquidCrystal_I2C lcd(0x3F, 16, 2); // note: some use address 0x2f 

char lcd_line1[20];
char lcd_line2[20];

// Dummy Interupt Handler for WDT

ISR(WDT_vect) {
  wdt_reset();                     // tell the WDT ALL'S ok ( needed ??? )
  wdt_disable();                   // disable watchdog for now
}

// callback routine for setting date/time on myfile

void dateTime(uint16_t* date, uint16_t* time) {
  // already set: DateTime rtctime = rtc.now();
  *date = FAT_DATE( rtctime.year(), rtctime.month(), rtctime.day() );
  *time = FAT_TIME( rtctime.hour(), rtctime.minute(), rtctime.second() );
}

void setup() {

  char month[4];
  char day[3];
  char year[5];
  char *ptr;
  int n;

  // init dht-lib
  dht.begin();

  // reserve fixed RAM space
  line.reserve(80);
  datelin.reserve(10);

  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  //         1234567890123456
  lcd.print(F("Feinstaub-Logger"));
  lcd.setCursor(0, 1);
  lcd.print(F("Ver 1.0, FP 2017"));
  delay (1000);

  // setup hardware

  Serial.begin(9600); // for reading SDS011 sensor

#ifdef SERDEBUG
  Serial.println (F("start debugging..."));
#endif

  // init the RTC object

  Wire.begin();
  rtc.begin();
  rtctime = rtc.now();

  if (! rtc.isrunning()) {                       // If RTC is still not running, then
    rtc.adjust(DateTime(__DATE__, __TIME__));    // load bogus date & time
    lcd.clear();
    lcd.print(F("date undef."));
    delay (800);
  }

  // check, if a file named "datetime.txt" exists,
  // read date & time from this file and set RTC clock
	// date must be in this format: Apr 29 2013 9:51

#ifdef SERDEBUG
  Serial.println (F("check for datetime.txt"));
#endif

  if (!myfile.open ("datetime.txt", O_RDWR)) {
#ifdef SERDEBUG
    Serial.println(F("no datetime file"));
#endif
  }
  else {
    //         1234567890123456
    lcd.clear();
    lcd.print(F("datetime.txt"));
    lcd.setCursor(0, 1);
    lcd.print(F("found..."));
    delay (800);

    while ((n = myfile.fgets(cbuffer, sizeof(cbuffer))) > 0) {

      // there might not just a simple linefeed, but CLRLF (0d,0a) under "DOS"
      if (cbuffer[n - 1] == '\n') {
        cbuffer[n - 1] = 0; // mark end of string
      }

      if (strlen(cbuffer)) { // skip empty lines
        if (cbuffer[0] != '#') { // skip comments

          // date must be in this format: Apr 29 2013 9:51
          ptr = strtok (cbuffer, " \t,-");
          if (ptr != NULL) strcpy (month, ptr);
          ptr = strtok (NULL, " \t,-");
          if (ptr != NULL) strcpy (day, ptr);
          ptr = strtok (NULL, " \t,-");
          if (ptr != NULL) strcpy (year, ptr);
          ptr = strtok (NULL, " \t,-");
          if (ptr != NULL) strcpy (timestr, ptr);

          // DateTime needs two char-arrays (datstr, timestr)

          strcpy (datestr, month);
          strcat (datestr, " ");
          strcat (datestr, day);
          strcat (datestr, " ");
          strcat (datestr, year);
          rtc.adjust(DateTime(datestr, timestr));
          rtc.begin();
        }
      }
    }

    myfile.close();
    sd.remove("datetime.txt");      // deletes the set time file
    lcd.clear(); lcd.print(F("Datum u. Zeit"));
    lcd.setCursor (0, 1); lcd.print (F("eingestellt"));
    delay (800);
  }


  // do not install interrupt handler - interrupt 0 on pin 2 (Arduino Uno)
  // attachInterrupt(0, count, CHANGE);

  // set date time callback function
  SdFile::dateTimeCallback(dateTime);

#ifdef SERDEBUG
  Serial.println ("checking time");
  Serial.println (getDate());
  Serial.println (F("entering main getdate now..."));
#endif

  // open myfile and write a header to the log file

  if (!myfile.open ("DATALOG.TXT", O_WRITE | O_APPEND | O_CREAT)) {
    lcd.clear();
    lcd.print(F("SD write err"));
    lcd.setCursor (0, 1);
    // lcd.print("<reset>");
#ifdef SERDEBUG
    Serial.print (F("file open error "));
    Serial.println (myfile.getError(), HEX);
#endif
  }

  myfile.println();
  myfile.println();
  myfile.print("power up:");
  myfile.print(getDate());
  myfile.println(getTime());
  myfile.println();
  myfile.print (F("version   "));
  myfile.println (String(version));
  myfile.print (F("free RAM  "));
  myfile.println (freeRAM(), DEC);
  myfile.println();
  myfile.println(F("Date      Time  PM10    PM25   Temp  Hum"));
  myfile.close();

  lcd.clear();
  lcd.print ("start logging...");
  delay (1000);
  lcd.clear();
}

// main loop

void loop() {

#ifdef SERDEBUG
  Serial.print(".");
#endif

  // things to do in every loop

  // show date and time on lcd

  strcpy (lcd_line1, getDate());
  strcat (lcd_line1, getTime());
  lcd.setCursor (0, 0);
  lcd.print (lcd_line1);

  sec = rtctime.second();

  /* show values on display

     second 0-5  temperature / humidity
     second 5-10 dust sensor data
     starting sensor at second 35
     record sensor values at 55
  */

  if (sec % 10 < 5) {
    t = dht.readTemperature();
    f = dht.readHumidity();

    // Check if any reads failed and exit early (to try again).
    if (isnan(t) || isnan(f)) {
      temp = 0;
      feuchte = 0;
    } else {
      temp = t;
      feuchte = f;
    }
    sprintf (lcd_line2, "F/T  %3d%%   %3dC", feuchte, temp);
    lcd.setCursor (0, 1);
    lcd.print (lcd_line2);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("   PM2.5    PM10");
    lcd.setCursor (0, 1);
    lcd.print ("   xxxxx   xxxxx");
  }

  if (sec == 40) {
    lcd.setCursor (0, 1);
    lcd.print ("sensor on...");
    delay (1000);
  }

  // Werte in Logdatei schreiben (1x pro Minute)

  if (sec == 0) {
    if (!myfile.open ("DATALOG.TXT", O_WRITE | O_APPEND | O_CREAT)) {
      lcd.setCursor (0, 1);
      lcd.print(F("cannot write log"));
      delay (1000);
    } else {
      lcd.setCursor (0, 1);
      lcd.print(F("writing log...  "));
      sprintf (cbuffer, "%s, %s, %3d, %3d\n", getDate(), getTime(), temp, feuchte);
      myfile.write (cbuffer);
      myfile.close();
      delay (500);
    }
    lcd.setCursor (0, 1);
    lcd.print ("sensor off...");
  }
  delay (1000);
}

// some standard date formats...

char *getTime() { // time in format "HH:MM"
  rtctime = rtc.now();
  sprintf (timestr, "%02d:%02d", rtctime.hour(), rtctime.minute());
  return (timestr);
}

char *getDate() { // date in format "DD.MM.YYYY "
  rtctime = rtc.now();
  sprintf (datestr, "%02d.%02d.%d ", rtctime.day(), rtctime.month(), rtctime.year());
  return (datestr);
}

// start SDS011 sensor

void start_SDS() {
  const uint8_t start_SDS_cmd[] = {0xAA, 0xB4, 0x06, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x06, 0xAB};
  Serial.write(start_SDS_cmd, sizeof(start_SDS_cmd));
}

// stop SDS011 sensor

void stop_SDS() {
  const uint8_t stop_SDS_cmd[] = {0xAA, 0xB4, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x05, 0xAB};
  Serial.write(stop_SDS_cmd, sizeof(stop_SDS_cmd));
}

// nice routine from Adafruit:

int freeRAM(void)
{
  extern int  __bss_end;
  extern int  *__brkval;
  int free_memory;
  if ((int)__brkval == 0) {
    free_memory = ((int)&free_memory) - ((int)&__bss_end);
  }
  else {
    free_memory = ((int)&free_memory) - ((int)__brkval);
  }
  return free_memory;
}