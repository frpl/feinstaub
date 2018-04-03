/*
   Feinstaubsensor mit I2C-Display
   Arduino Pro Mini

	 FP, 2017 (frplgmx.de)
   this code is licensed under CC BY-NC-SA 4.0 -
   http://creativecommons.org/licenses/by-nc-sa/4.0/
	 
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// some displays have a different i2c-address:
// LiquidCrystal_I2C lcd(0x3f, 16, 2);

uint8_t buffer[40];
char ausgabe[20];
unsigned short i;

unsigned pm25;
unsigned pm10;
float fpm25;
float fpm10;

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

void print_digit (uint8_t digit) {
  //Serial.print ("print_digit: ");
  //Serial.println (digit, HEX);

  if (digit < 10) {
    lcd.write ('0' + digit);
  } else {
    lcd.write ('A' + digit - 10);
  }
}

// for debugging purposes

void print_hex(uint8_t val) {
  //Serial.print ("print_hex: ");
  //Serial.println (val, HEX);

  uint8_t lval = val & 0x0f;
  uint8_t hval = (val & 0xf0) >> 4;
  print_digit (hval);
  print_digit (lval);
}


void setup() {
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  //         1234567890123456
  lcd.print("Feinstaubsensor");
  lcd.setCursor(0, 1);
  lcd.print("FP Okt 2017");
  delay (1400);
  lcd.clear();
  //         1234567890123456
  lcd.print("Einheit:       ");
  lcd.setCursor(0, 1);
  lcd.print("Mikrogramm/m^3 ");
  delay (1000);
  lcd.clear();
  Serial.begin(9600);
  start_SDS();
}


void loop() {
  if (Serial.available()) {
    // clear buffers, reset everything for a new AIS message
    memset (buffer,  0, sizeof(buffer));
    Serial.readBytesUntil(0xab, buffer, sizeof(buffer));
    i = 0;
		
    /* raw data for debugging purposes
    lcd.clear();
    
    while ((buffer[i] != 0xab) && (i < sizeof(buffer))) {
      print_hex(buffer[i]);
      i++;
    }
    */
		
    // report
		
    lcd.setCursor(0, 0);
    lcd.print("   PM2.5    PM10");
    lcd.setCursor (0, 1);
    if ((buffer[0] == 0xaa) && (buffer[1] == 0xC0)) {
      pm25 = buffer[2] + (buffer[3] << 8);
      pm10 = buffer[4] + (buffer[5] << 8);
      fpm25 = (float) (pm25 / 10.0);
      fpm10 = (float) (pm10 / 10.0);
			
      //                 1234567890123456
      //                   1234.5  1234.5

      if (pm25 < 20000) {
        dtostrf (fpm25, 8, 1, ausgabe);
        lcd.print (ausgabe);
      } else {
        lcd.print ("       -");
      }
      if (pm10 < 20000) {
        dtostrf (fpm10, 8, 1, ausgabe);
        lcd.print (ausgabe);
      } else {
        lcd.print ("       -");
      }
    }
  } // if Serial.available()
}