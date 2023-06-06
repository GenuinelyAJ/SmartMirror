// Header file includes
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <DHT.h>
#include <SPI.h>
#include <Wire.h>
#include "Font7Seg.h"
#include "RTClib.h"
#include <LiquidCrystal_I2C.h> 
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// Define the number of devices we have in the chain and the hardware interface
// NOTE: These pin numbers will probably not work with your hardware and may
// need to be adapted
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4 // Define the number of displays connected
#define CLK_PIN   52
#define DATA_PIN  51
#define CS_PIN    53
#define SPEED_TIME 75 // Speed of the transition
#define PAUSE_TIME  0
#define MAX_MESG   20

// RCWL - 0516+ Microwave Sensor
#define Sensor 2

// These are for the clock
#define DS1307_ADDRESS 0x68

// These are for the temperature
#define DHTPIN 8
#define DHTTYPE DHT22
#define TIMEDHT 1000

// Global variables
RTC_DS1307 rtc;
uint8_t wday, mday, month, year;
uint8_t hours, minutes, seconds;

char szTime[9];    // mm:ss\0
char szMesg[MAX_MESG + 1] = "";

float humidity, celsius, fahrenheit;

uint8_t degC[] = { 6, 3, 3, 56, 68, 68, 68 }; // Deg C
uint8_t degF[] = { 6, 3, 3, 124, 20, 20, 4 }; // Deg F

uint8_t clear = 0x00;

uint32_t timerDHT = TIMEDHT;

DHT dht(DHTPIN, DHTTYPE);

// Hardware SPI connection7
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Battery Gauge
byte gauge_empty[8] =  {B11111, B00000, B00000, B00000, B00000, B00000, B00000, B11111};    // empty middle piece
byte gauge_fill_1[8] = {B11111, B10000, B10000, B10000, B10000, B10000, B10000, B11111};    // filled gauge - 1 column
byte gauge_fill_2[8] = {B11111, B11000, B11000, B11000, B11000, B11000, B11000, B11111};    // filled gauge - 2 columns
byte gauge_fill_3[8] = {B11111, B11100, B11100, B11100, B11100, B11100, B11100, B11111};    // filled gauge - 3 columns
byte gauge_fill_4[8] = {B11111, B11110, B11110, B11110, B11110, B11110, B11110, B11111};    // filled gauge - 4 columns
byte gauge_fill_5[8] = {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111};    // filled gauge - 5 columns
byte gauge_left[8] =   {B11111, B10000, B10000, B10000, B10000, B10000, B10000, B11111};    // left part of gauge - empty
byte gauge_right[8] =  {B11110, B00010, B00011, B00001, B00001, B00011, B00010, B11110};    // right part of gauge - empty

byte gauge_mask_left[8] = {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111};  // mask for rounded corners for leftmost character
byte gauge_mask_right[8] = {B11110, B11110, B11111, B11111, B11111, B11111, B11110, B11110}; // mask for rounded corners for rightmost character

byte warning_icon[8] = {B00100, B00100, B01110, B01010, B11011, B11111, B11011, B11111};     // warning icon - just because we still have one custom character left

byte gauge_left_dynamic[8];   // left part of gauge dynamic - will be set in the loop function
byte gauge_right_dynamic[8];  // right part of gauge dynamic - will be set in the loop function


int cpu_gauge;       // value for the CPU gauge
char buffer[16];         // helper buffer to store C-style strings (generated with sprintf function)
int move_offset = 0;     // used to shift bits for the custom characters
float voltage;
const int gauge_size_chars = 16;       // width of the gauge in number of characters
char gauge_string[gauge_size_chars + 1]; // string that will include all the gauge character to be printed

int parsed_int_from_serial;

int offTimer = 0;

void beginDS1307()
{
  // Read the values ​​(date and time) of the DS1307 module
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(clear);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_ADDRESS, 0x07);

  seconds = bcdToDec(Wire.read());
  minutes = bcdToDec(Wire.read());
  hours = bcdToDec(Wire.read() & 0xff);
  wday = bcdToDec(Wire.read());
  mday = bcdToDec(Wire.read());
  month = bcdToDec(Wire.read());
  year = bcdToDec(Wire.read());
}

uint8_t decToBcd(uint8_t value)
{
  return ((value / 10 * 16) + (value % 10));
}

uint8_t bcdToDec(uint8_t value)
{
  return ((value / 16 * 10) + (value % 16));
}

// Code for reading clock time
void getTime(char *psz, bool f = true)
{
  sprintf(psz, "%02d%c%02d", hours, (f ? ':' : ' '), minutes);
}

// Code for reading clock date
void getDate(char *psz)
{
  char  szBuf[10];
  sprintf(psz, "%d %s %04d", mday , mon2str(month, szBuf, sizeof(szBuf) - 1), (year + 2000));
}

// Code for get Temperature
void getTemperature()
{
  // Wait for a time between measurements
  if ((millis() - timerDHT) > TIMEDHT) {
    // Update the timer
    timerDHT = millis();

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    humidity = dht.readHumidity();

    // Read temperature as Celsius (the default)
    celsius = dht.readTemperature();

    // Read temperature as Fahrenheit (isFahrenheit = true)
    fahrenheit = dht.readTemperature(true);

    // Check if any reads failed and exit early (to try again)
    if (isnan(humidity) || isnan(celsius) || isnan(fahrenheit)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }
  }
}

// Get a label from PROGMEM into a char array
char *mon2str(uint8_t mon, char *psz, uint8_t len)
{
  static const __FlashStringHelper* str[] =
  {
    F("Jan"), F("Feb"), F("Mar"), F("Apr"),
    F("May"), F("June"), F("July"), F("Aug"),
    F("Sep"), F("Oct"), F("Nov"), F("Dec")
  };

  strncpy_P(psz, (const char PROGMEM *)str[mon - 1], len);
  psz[len] = '\0';

  return (psz);
}

char *date2str(uint8_t code, char *psz, uint8_t len)
{
  static const __FlashStringHelper* str[] =
  {
    F("Sunday"), F("Monday"), F("Tuesday"),
    F("Wednesday"), F("Thursday"), F("Friday"),
    F("Saturday")
  };

  strncpy_P(psz, (const char PROGMEM *)str[code + 0], len);
  psz[len] = '\0';
  return (psz);
}

void setup(void)
{
  pinMode(Sensor,INPUT);
  Wire.begin();

  P.begin(2);
  P.setInvert(false);

  P.setZone(0,  MAX_DEVICES - 4, MAX_DEVICES - 1);
  P.setZone(1, MAX_DEVICES - 4, MAX_DEVICES - 1);

  P.displayZoneText(1, szTime, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(0, szMesg, PA_CENTER, SPEED_TIME, 0, PA_PRINT , PA_NO_EFFECT);

  P.addChar('$', degC);
  P.addChar('&', degF);

  dht.begin();
  
  lcd.init();                       // initialize the 16x2 lcd module
  lcd.backlight();                  // enable backlight for the LCD module
  
  Serial.begin(9600);  // open serial port, set data rate to 9600 bps

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initialized");
   lcd.init();                       // initialize the 16x2 lcd module
  lcd.backlight();                  // enable backlight for the LCD module
  lcd.createChar(7, gauge_empty);   // middle empty gauge
  lcd.createChar(1, gauge_fill_1);  // filled gauge - 1 column
  lcd.createChar(2, gauge_fill_2);  // filled gauge - 2 columns
  lcd.createChar(3, gauge_fill_3);  // filled gauge - 3 columns
  lcd.createChar(4, gauge_fill_4);  // filled gauge - 4 columns
  lcd.createChar(0, warning_icon); // warning icon - just because we have one more custom character that we could use
}

void loop(void)
{
  bool Detection = digitalRead(Sensor);
  // Battery Gauge
  float units_per_pixel = (gauge_size_chars * 5.0) / 100.0;    //  every character is 5px wide, we want to count from 0-100
  int value_in_pixels = round(cpu_gauge * units_per_pixel);    // cpu_gauge value converted to pixel width

  int tip_position = 0;      // 0= not set, 1=tip in first char, 2=tip in middle, 3=tip in last char

  if (value_in_pixels < 5) {
    tip_position = 1; // tip is inside the first character
  }
  else if (value_in_pixels > gauge_size_chars * 5.0 - 5) {
    tip_position = 3; // tip is inside the last character
  }
  else {
    tip_position = 2; // tip is somewhere in the middle
  }

  move_offset = 4 - ((value_in_pixels - 1) % 5);    // value for offseting the pixels for the smooth filling

  for (int i = 0; i < 8; i++) { // dynamically create left part of the gauge
    if (tip_position == 1) {
      gauge_left_dynamic[i] = (gauge_fill_5[i] << move_offset) | gauge_left[i]; // tip on the first character
    }
    else {
      gauge_left_dynamic[i] = gauge_fill_5[i]; // tip not on the first character
    }

    gauge_left_dynamic[i] = gauge_left_dynamic[i] & gauge_mask_left[i];                                 // apply mask for rounded corners
  }

  for (int i = 0; i < 8; i++) { // dynamically create right part of the gauge
    if (tip_position == 3) {
      gauge_right_dynamic[i] = (gauge_fill_5[i] << move_offset) | gauge_right[i]; // tip on the last character
    }
    else {
      gauge_right_dynamic[i] = gauge_right[i]; // tip not on the last character
    }

    gauge_right_dynamic[i] = gauge_right_dynamic[i] & gauge_mask_right[i];                                // apply mask for rounded corners
  }

  lcd.createChar(5, gauge_left_dynamic);     // create custom character for the left part of the gauge
  lcd.createChar(6, gauge_right_dynamic);    // create custom character for the right part of the gauge

  for (int i = 0; i < gauge_size_chars; i++) { // set all the characters for the gauge
    if (i == 0) {
      gauge_string[i] = byte(5); // first character = custom left piece
    }
    else if (i == gauge_size_chars - 1) {
      gauge_string[i] = byte(6); // last character = custom right piece
    }
    else {                                                        // character in the middle, could be empty, tip or fill
      if (value_in_pixels <= i * 5) {
        gauge_string[i] = byte(7); // empty character
      }
      else if (value_in_pixels > i * 5 && value_in_pixels < (i + 1) * 5) {
        gauge_string[i] = byte(5 - move_offset); // tip
      }
      else {
        gauge_string[i] = byte(255); // filled character
      }
    }
  }

  int sensorValue = analogRead(A0);
  voltage = sensorValue * (5.0 / 1023.0);

  if (value_in_pixels < 5) {
    tip_position = 1; // tip is inside the first character
  }
  else if (value_in_pixels > gauge_size_chars * 5.0 - 5) {
    tip_position = 3; // tip is inside the last character
  }
  else {
    tip_position = 2; // tip is somewhere in the middle
  }

  move_offset = 4 - ((value_in_pixels - 1) % 5);    // value for offseting the pixels for the smooth filling

 
 
  for (int i = 0; i < gauge_size_chars; i++) { // set all the characters for the gauge
    if (i == 0) {
      gauge_string[i] = byte(5); // first character = custom left piece
    }
    else if (i == gauge_size_chars - 1) {
      gauge_string[i] = byte(6); // last character = custom right piece
    }
    else {                                                        // character in the middle, could be empty, tip or fill
      if (value_in_pixels <= i * 5) {
        gauge_string[i] = byte(7); // empty character
      }
      else if (value_in_pixels > i * 5 && value_in_pixels < (i + 1) * 5) {
        gauge_string[i] = byte(5 - move_offset); // tip
      }
      else {
        gauge_string[i] = byte(255); // filled character
      }
    }
  }
      lcd.setCursor(0, 0);                        // move cursor to top left
      sprintf(buffer, "Battery:%3d%% ", cpu_gauge);    // set a string as CPU: XX%, with the number always taking at least 3 character
      lcd.print(buffer);                          // print the string on the display
      lcd.print("  ");

      lcd.setCursor(0, 1);             // move the cursor to the next line
      lcd.print(gauge_string);         // display the gauge
      Serial.println(voltage);

  if (voltage <= 5 && voltage > 4.6) {
      cpu_gauge = 100;
  }
   else if (voltage <= 4.5 && voltage > 4.1) {
      cpu_gauge = 90;
  }
    else if (voltage <= 4 && voltage > 3.6) {
      cpu_gauge = 80;
  }
     else if (voltage <= 3.5 && voltage > 3.1) {
      cpu_gauge = 70;
  }
    else if (voltage <= 3 && voltage > 2.6) {
      cpu_gauge = 60;
  }
     else if (voltage <= 2.5 && voltage > 2.1) {
      cpu_gauge = 50;
  }
    else if (voltage <= 2 && voltage > 1.6) {
      cpu_gauge = 40;
  }
     else if (voltage <= 1.5 && voltage > 1.1) {
      lcd.write(byte(0));                         // print warning character
      lcd.print("  ");

      cpu_gauge = 30;
  }
    else if (voltage <= 1 && voltage > 0.6 ) {
      lcd.write(byte(0));                         // print warning character
      lcd.print("  ");
      cpu_gauge = 20;
  }
    else if (voltage <= 0.5 && voltage > 0.1 ) {
      lcd.write(byte(0));                         // print warning character
      lcd.print("  ");
      cpu_gauge = 10;
  }

  if (Detection == HIGH){

    Serial.println("Motion detected !!");
    // Mirror Stuff
    static uint32_t lastTime = 0; // Memory (ms)
    static uint8_t  display = 0;  // Current display mode
    static bool flasher = false;  // Seconds passing flasher

    beginDS1307();
    getTemperature();

    P.displayAnimate();

    if (P.getZoneStatus(0))
    {
      switch (display)
      {
      case 0: // Temperature deg Celsius
        P.setPause(0, 1000);
        P.setTextEffect(0, PA_SCROLL_LEFT, PA_SCROLL_UP);
        display++;
        dtostrf(celsius, 3, 1, szMesg);
        strcat(szMesg, "$");

        break;
      case 1: // Temperature deg Fahrenheit
        P.setTextEffect(0, PA_SCROLL_UP, PA_SCROLL_DOWN);
        display++;
        dtostrf(fahrenheit, 3, 1, szMesg);
        strcat(szMesg, "&");

        break;

      case 2: // Clock
        P.setFont(0, numeric7Seg);
        P.setTextEffect(0, PA_PRINT, PA_NO_EFFECT);
        P.setPause(0, 0);

        if ((millis() - lastTime) >= 1000)
        {
          lastTime = millis();
          getTime(szMesg, flasher);
          flasher = !flasher;
        }

        if ((seconds == 0) && (seconds <= 5)) {
          display++;
          P.setTextEffect(0, PA_PRINT, PA_WIPE_CURSOR);
        }

        break;
      case 3: // Day of week
        P.setFont(0, nullptr);
        P.setTextEffect(0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
        display++;
        date2str(wday, szMesg, MAX_MESG);

        break;
      default: // Calendar
        P.setTextEffect(0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
        display = 0;
        getDate(szMesg);

        break;
      }

      P.displayReset(0); // Rest zone zero
    }

  }

  if(Detection == LOW){
    Serial.println("Clear");
      P.displayClear();
      P.displayReset(0);
  }

}

