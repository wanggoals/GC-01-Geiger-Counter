#include <Arduino.h>

#include <EEPROM.h>
#include "SPI.h"
#include "Adafruit_GFX.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include "Adafruit_ILI9341.h"
#include <XPT2046_Touchscreen.h>

#define CS_PIN D2
XPT2046_Touchscreen ts(CS_PIN);

#define TS_MINX 250
#define TS_MINY 200                             // calibration points for touchscreen
#define TS_MAXX 3800
#define TS_MAXY 3750

#define TFT_DC D4
#define TFT_CS D8

#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF
#define DOSEBACKGROUND 0x0455

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

const int interruptPin = 5;

long count[61];
long fastCount[4];       // arrays to store running counts
int i = 0;                         
int j = 0;

int page = 0;

long currentMillis;
long previousMillis;
unsigned long currentMicros;
unsigned long previousMicros;

long averageCount;
unsigned long currentCount;         // incremented by interrupt
unsigned long previousCount;        // to activate buzzer and LED
unsigned long cumulativeCount;
float doseRate;
float totalDose;
char dose[5];            
float doseAdjusted;            

int doseLevel;            // determines home screen warning signs
int previousDoseLevel;

bool ledSwitch = 1;
bool buzzerSwitch = 1;
bool wasTouched;
bool integrationMode = 0;         // 0 = slow, 1 = fast

bool doseUnits = 0;               // 0 == uSv/hr, 1 = mR/hr  
int alarmThreshold = 5;
int conversionFactor = 175;

int x, y;                          // touch points

int batteryInput;
int batteryPercent;
int batteryMapped = 212;

const int saveUnits = 0;
const int saveAlertThreshold = 1;        // Addresses for storing settings data in the EEPROM
const int saveCalibration = 2;

void ICACHE_RAM_ATTR isr();

const unsigned char settingsBitmap[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xc0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xe0, 0x00, 0x00, 0x00,
    0x00, 0x01, 0xc0, 0x7f, 0xe0, 0x38, 0x00, 0x00, 0x00, 0x03, 0xf0, 0x7f, 0xe0, 0xfc, 0x00, 0x00,
    0x00, 0x07, 0xf9, 0xff, 0xf9, 0xfe, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
    0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
    0x00, 0x07, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00,
    0x00, 0x03, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00,
    0x00, 0x01, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x03, 0xff, 0xf0, 0x7f, 0xfc, 0x00, 0x00,
    0x00, 0x03, 0xff, 0xc0, 0x3f, 0xfc, 0x00, 0x00, 0x00, 0x1f, 0xff, 0x80, 0x1f, 0xff, 0x80, 0x00,
    0x00, 0xff, 0xff, 0x00, 0x0f, 0xff, 0xf0, 0x00, 0x01, 0xff, 0xff, 0x00, 0x07, 0xff, 0xf8, 0x00,
    0x01, 0xff, 0xfe, 0x00, 0x07, 0xff, 0xf8, 0x00, 0x01, 0xff, 0xfe, 0x00, 0x07, 0xff, 0xf8, 0x00,
    0x01, 0xff, 0xfe, 0x00, 0x07, 0xff, 0xf8, 0x00, 0x01, 0xff, 0xfe, 0x00, 0x07, 0xff, 0xf8, 0x00,
    0x01, 0xff, 0xfe, 0x00, 0x07, 0xff, 0xf8, 0x00, 0x00, 0xff, 0xff, 0x00, 0x0f, 0xff, 0xf0, 0x00,
    0x00, 0x1f, 0xff, 0x80, 0x1f, 0xff, 0x80, 0x00, 0x00, 0x03, 0xff, 0xc0, 0x3f, 0xfc, 0x00, 0x00,
    0x00, 0x03, 0xff, 0xe0, 0x7f, 0xfc, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00,
    0x00, 0x01, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00,
    0x00, 0x07, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00,
    0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
    0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x07, 0xf9, 0xff, 0xf9, 0xfe, 0x00, 0x00,
    0x00, 0x03, 0xf0, 0x7f, 0xe0, 0xfc, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x7f, 0xe0, 0x38, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xc0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xc0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const unsigned char buzzerOnBitmap[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0x0c, 0x00,
    0x00, 0x00, 0x07, 0x80, 0x0e, 0x00, 0x00, 0x00, 0x0f, 0x80, 0x0f, 0x00, 0x00, 0x00, 0x1f, 0x80,
    0x07, 0x00, 0x00, 0x00, 0x3f, 0x80, 0xc7, 0x80, 0x00, 0x00, 0xff, 0x80, 0xe3, 0x80, 0x00, 0x01,
    0xff, 0x80, 0xf3, 0xc0, 0x00, 0x03, 0xff, 0x80, 0x71, 0xc0, 0x00, 0x07, 0xff, 0x8c, 0x79, 0xc0,
    0x3f, 0xff, 0xff, 0x9e, 0x38, 0xe0, 0x3f, 0xff, 0xff, 0x8e, 0x38, 0xe0, 0x3f, 0xff, 0xff, 0x8e,
    0x3c, 0xe0, 0x3f, 0xff, 0xff, 0x87, 0x1c, 0xe0, 0x3f, 0xff, 0xff, 0x87, 0x1c, 0x60, 0x3f, 0xff,
    0xff, 0x87, 0x1c, 0x70, 0x3f, 0xff, 0xff, 0x87, 0x1c, 0x70, 0x3f, 0xff, 0xff, 0x87, 0x1c, 0x70,
    0x3f, 0xff, 0xff, 0x87, 0x1c, 0x70, 0x3f, 0xff, 0xff, 0x87, 0x1c, 0x70, 0x3f, 0xff, 0xff, 0x87,
    0x1c, 0xe0, 0x3f, 0xff, 0xff, 0x8e, 0x3c, 0xe0, 0x3f, 0xff, 0xff, 0x8e, 0x38, 0xe0, 0x3f, 0xff,
    0xff, 0x9e, 0x38, 0xe0, 0x00, 0x07, 0xff, 0x8c, 0x79, 0xc0, 0x00, 0x03, 0xff, 0x80, 0x71, 0xc0,
    0x00, 0x00, 0xff, 0x80, 0xf1, 0xc0, 0x00, 0x00, 0x7f, 0x80, 0xe3, 0x80, 0x00, 0x00, 0x3f, 0x80,
    0xc7, 0x80, 0x00, 0x00, 0x1f, 0x80, 0x07, 0x00, 0x00, 0x00, 0x0f, 0x80, 0x0f, 0x00, 0x00, 0x00,
    0x07, 0x80, 0x0e, 0x00, 0x00, 0x00, 0x03, 0x80, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const unsigned char buzzerOffBitmap[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x3f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x80, 0x00, 0x00, 0x00, 0x00, 0xff, 0x80, 0x00, 0x00,
    0x00, 0x03, 0xff, 0x80, 0x00, 0x00, 0x00, 0x07, 0xff, 0x80, 0x00, 0x00, 0x00, 0x0f, 0xff, 0x80,
    0x00, 0x00, 0x0f, 0xff, 0xff, 0x80, 0x00, 0x00, 0x1f, 0xff, 0xff, 0x80, 0x00, 0x00, 0x3f, 0xff,
    0xff, 0x8f, 0x00, 0x78, 0x7f, 0xff, 0xff, 0x8f, 0x80, 0xf8, 0x7f, 0xff, 0xff, 0x8f, 0xc1, 0xf8,
    0x7f, 0xff, 0xff, 0x87, 0xe3, 0xf0, 0x7f, 0xff, 0xff, 0x83, 0xf7, 0xe0, 0x7f, 0xff, 0xff, 0x81,
    0xff, 0xc0, 0x7f, 0xff, 0xff, 0x80, 0xff, 0x80, 0x7f, 0xff, 0xff, 0x80, 0x7f, 0x00, 0x7f, 0xff,
    0xff, 0x80, 0x7f, 0x00, 0x7f, 0xff, 0xff, 0x80, 0xff, 0x80, 0x7f, 0xff, 0xff, 0x81, 0xff, 0xc0,
    0x7f, 0xff, 0xff, 0x83, 0xf7, 0xe0, 0x7f, 0xff, 0xff, 0x87, 0xe3, 0xf0, 0x7f, 0xff, 0xff, 0x8f,
    0xc1, 0xf0, 0x7f, 0xff, 0xff, 0x8f, 0x80, 0xf8, 0x3f, 0xff, 0xff, 0x8f, 0x00, 0x70, 0x3f, 0xff,
    0xff, 0x84, 0x00, 0x20, 0x1f, 0xff, 0xff, 0x80, 0x00, 0x00, 0x0f, 0xff, 0xff, 0x80, 0x00, 0x00,
    0x00, 0x07, 0xff, 0x80, 0x00, 0x00, 0x00, 0x03, 0xff, 0x80, 0x00, 0x00, 0x00, 0x01, 0xff, 0x80,
    0x00, 0x00, 0x00, 0x00, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x1f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x07, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const unsigned char ledOnBitmap[] PROGMEM = {
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00,
    0x00, 0x00, 0x00, 0x18, 0x07, 0x00, 0xc0, 0x00, 0x00, 0x1c, 0x07, 0x01, 0xc0, 0x00, 0x00, 0x1e,
    0x07, 0x03, 0xc0, 0x00, 0x00, 0x0e, 0x07, 0x03, 0x80, 0x00, 0x00, 0x0f, 0x00, 0x07, 0x80, 0x00,
    0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1e, 0x00, 0x1f, 0xc0, 0x03, 0xc0, 0x0f, 0x80, 0x7f, 0xf0, 0x0f, 0x80, 0x07, 0xc1,
    0xff, 0xfc, 0x1f, 0x00, 0x03, 0xc3, 0xe0, 0x3e, 0x1e, 0x00, 0x00, 0x07, 0xc0, 0x0f, 0x00, 0x00,
    0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x07, 0x80, 0x00, 0x00, 0x0e, 0x00, 0x03,
    0x80, 0x00, 0x00, 0x0e, 0x00, 0x03, 0x80, 0x00, 0x00, 0x1c, 0x00, 0x01, 0xc0, 0x00, 0x7f, 0x1c,
    0x00, 0x01, 0xc3, 0xf0, 0x7f, 0x1c, 0x00, 0x01, 0xc7, 0xf0, 0x3c, 0x0e, 0x00, 0x03, 0x81, 0xe0,
    0x00, 0x0e, 0x00, 0x03, 0x80, 0x00, 0x00, 0x0e, 0x00, 0x03, 0x80, 0x00, 0x00, 0x0f, 0x00, 0x07,
    0x80, 0x00, 0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x07, 0x80, 0x0f, 0x00, 0x00, 0x01, 0xc3,
    0xc0, 0x1e, 0x1c, 0x00, 0x07, 0xc1, 0xc0, 0x1c, 0x1f, 0x00, 0x0f, 0x81, 0xe0, 0x3c, 0x0f, 0x80,
    0x1e, 0x00, 0xe0, 0x38, 0x03, 0xc0, 0x0c, 0x00, 0xe0, 0x38, 0x01, 0x80, 0x00, 0x00, 0xf0, 0x78,
    0x00, 0x00, 0x00, 0x00, 0x7f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xf0, 0x00, 0x00, 0x00, 0x00,
    0x3f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xe0, 0x00, 0x00,
    0x00, 0x00, 0x3f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xe0,
    0x00, 0x00, 0x00, 0x00, 0x1f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00};

const unsigned char ledOffBitmap[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x1f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xf0, 0x00, 0x00, 0x00, 0x01,
    0xff, 0xfc, 0x00, 0x00, 0x00, 0x03, 0xe0, 0x3e, 0x00, 0x00, 0x00, 0x07, 0xc0, 0x0f, 0x00, 0x00,
    0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x07, 0x80, 0x00, 0x00, 0x0e, 0x00, 0x03,
    0x80, 0x00, 0x00, 0x0e, 0x00, 0x03, 0x80, 0x00, 0x00, 0x1c, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x1c,
    0x00, 0x01, 0xc0, 0x00, 0x00, 0x1c, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x0e, 0x00, 0x03, 0x80, 0x00,
    0x00, 0x0e, 0x00, 0x03, 0x80, 0x00, 0x00, 0x0e, 0x00, 0x03, 0x80, 0x00, 0x00, 0x0f, 0x00, 0x07,
    0x80, 0x00, 0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x07, 0x80, 0x0f, 0x00, 0x00, 0x00, 0x03,
    0xc0, 0x1e, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x1c, 0x00, 0x00, 0x00, 0x01, 0xe0, 0x3c, 0x00, 0x00,
    0x00, 0x00, 0xe0, 0x38, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x38, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x78,
    0x00, 0x00, 0x00, 0x00, 0x7f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xf0, 0x00, 0x00, 0x00, 0x00,
    0x3f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xe0, 0x00, 0x00,
    0x00, 0x00, 0x3f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xe0,
    0x00, 0x00, 0x00, 0x00, 0x1f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00};

const unsigned char backBitmap [] PROGMEM = {
	0x00, 0x00, 0x00, 0x1f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xc0, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 
	0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x01, 0xff, 
	0x80, 0x00, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x07, 0xfc, 0x00, 0x00, 0x1f, 0xf0, 0x00, 0x00, 0x00, 
	0x0f, 0xf0, 0x00, 0x00, 0x07, 0xf8, 0x00, 0x00, 0x00, 0x1f, 0xc0, 0x00, 0x00, 0x01, 0xfc, 0x00, 
	0x00, 0x00, 0x3f, 0x80, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 
	0x3f, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x80, 0x00, 0x01, 0xf8, 0x00, 0x00, 
	0x00, 0x00, 0x0f, 0xc0, 0x00, 0x03, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x07, 0xe0, 0x00, 0x03, 0xe0, 
	0x00, 0x00, 0x00, 0x00, 0x03, 0xf0, 0x00, 0x07, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x01, 0xf0, 0x00, 
	0x0f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x01, 0xf8, 0x00, 0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0xf8, 0x00, 0x1f, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x1f, 0x00, 0x00, 0x1f, 0x00, 
	0x00, 0x00, 0x7c, 0x00, 0x3e, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x3e, 0x00, 0x00, 
	0x7e, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x3c, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x7c, 
	0x00, 0x01, 0xfc, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x7c, 0x00, 0x03, 0xf8, 0x00, 0x00, 0x00, 0x1f, 
	0x00, 0x78, 0x00, 0x07, 0xf0, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x78, 0x00, 0x0f, 0xe0, 0x00, 0x00, 
	0x00, 0x0f, 0x00, 0xf8, 0x00, 0x1f, 0xc0, 0x00, 0x00, 0x00, 0x0f, 0x80, 0xf8, 0x00, 0x3f, 0x80, 
	0x00, 0x00, 0x00, 0x0f, 0x80, 0xf8, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x80, 0xf8, 0x00, 
	0xff, 0xff, 0xff, 0xff, 0x00, 0x0f, 0x80, 0xf8, 0x01, 0xff, 0xff, 0xff, 0xff, 0x80, 0x0f, 0x80, 
	0xf8, 0x01, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x07, 0x80, 0xf8, 0x01, 0xff, 0xff, 0xff, 0xff, 0x80, 
	0x0f, 0x80, 0xf8, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x0f, 0x80, 0xf8, 0x00, 0x7e, 0x00, 0x00, 
	0x00, 0x00, 0x0f, 0x80, 0xf8, 0x00, 0x3f, 0x80, 0x00, 0x00, 0x00, 0x0f, 0x80, 0xf8, 0x00, 0x1f, 
	0x80, 0x00, 0x00, 0x00, 0x0f, 0x80, 0x78, 0x00, 0x0f, 0xe0, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x78, 
	0x00, 0x07, 0xe0, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x7c, 0x00, 0x03, 0xf8, 0x00, 0x00, 0x00, 0x1f, 
	0x00, 0x7c, 0x00, 0x01, 0xf8, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x3c, 0x00, 0x00, 0xfe, 0x00, 0x00, 
	0x00, 0x1e, 0x00, 0x3e, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x3e, 0x00, 0x00, 0x3f, 
	0x00, 0x00, 0x00, 0x3e, 0x00, 0x1f, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x1f, 0x00, 
	0x00, 0x0e, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, 
	0x0f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x07, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x01, 
	0xf0, 0x00, 0x03, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x03, 0xf0, 0x00, 0x03, 0xf0, 0x00, 0x00, 0x00, 
	0x00, 0x07, 0xe0, 0x00, 0x01, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xc0, 0x00, 0x00, 0xfc, 0x00, 
	0x00, 0x00, 0x00, 0x1f, 0x80, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 
	0x3f, 0x80, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x1f, 0xc0, 0x00, 0x00, 0x01, 0xfc, 0x00, 
	0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x07, 0xf8, 0x00, 0x00, 0x00, 0x07, 0xfc, 0x00, 0x00, 0x1f, 
	0xf0, 0x00, 0x00, 0x00, 0x03, 0xff, 0x80, 0x00, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 
	0x7f, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x0f, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x1f, 0xfc, 0x00, 0x00, 0x00, 0x00
};

void drawHomePage();
void drawSettingsPage();
void drawUnitsPage();
void drawAlertPage();
void drawCalibrationPage();

void setup()
{
  Serial.begin(38400);
  ts.begin();
  ts.setRotation(2);

  tft.begin();
  tft.setRotation(2);
  tft.fillScreen(ILI9341_BLACK);

  pinMode(D0, OUTPUT); // buzzer switch
  pinMode(D3, OUTPUT); // LED
  digitalWrite(D3, LOW);
  digitalWrite(D0, LOW);

  EEPROM.begin(512);

  doseUnits = EEPROM.read(saveUnits);
  alarmThreshold = EEPROM.read(saveAlertThreshold);
  conversionFactor = EEPROM.read(saveCalibration);

  Serial.println("Begin");
  attachInterrupt(interruptPin, isr, FALLING);

  drawHomePage();
}

void loop()
{
  if (page == 0)                 // homepage
  { 
    currentMillis = millis();
    if (currentMillis - previousMillis >= 1000) 
    {
      previousMillis = currentMillis;

      batteryInput = analogRead(A0);
      batteryInput = constrain(batteryInput, 670, 870);
      batteryPercent = map(batteryInput, 670, 870, 0, 100);
      batteryMapped = map(batteryPercent, 100, 0, 212, 233);  

      tft.fillRect(212, 6, 22, 10, ILI9341_BLACK);
      tft.fillRect(batteryMapped, 6, (234 - batteryMapped), 10, ILI9341_GREEN);  // draws battery icon every second

      count[i] = currentCount;
      i++;
      fastCount[j] = currentCount; // keep concurrent arrays of counts. Use only one depending on user choice
      j++;

      if (i == 61)                                                                                                                                                                                             
      {
        i = 0;
      }

      if (j == 4)
      {
        j = 0;
      }

      if (integrationMode == 1)
      {
        averageCount = (currentCount - fastCount[j]) * 20;  
      }

      else
      {
        averageCount = currentCount - count[i];  // count[i] stores the value from 60 seconds ago
      }

      averageCount = ((averageCount)/(1 - 0.00000333 * float(averageCount))); // accounts for dead time of the geiger tube. relevant at high count rates

      if (doseUnits == 0)
      {
        doseRate = averageCount / float(conversionFactor);
        totalDose = cumulativeCount / (60 * float(conversionFactor));
        doseAdjusted = doseRate;
      }
      else if (doseUnits == 1)
      {        
        doseRate = averageCount / float(conversionFactor * 10.0);
        totalDose = cumulativeCount / (60 * float(conversionFactor * 10.0));   // 1 mRem == 10 uSv
        doseAdjusted = doseRate * 10.0;
      }

      if (doseAdjusted < 1)
        doseLevel = 0;                         // determines alert level displayed on homescreen
      else if (doseAdjusted < alarmThreshold)
        doseLevel = 1;
      else
        doseLevel = 2;

      if (doseRate < 10)
      {
        dtostrf(doseRate, 4, 2, dose);       // display two digits after the decimal point if value is less than 10
      }
      if ((doseRate >= 10) && (doseRate < 100))  
      {
        dtostrf(doseRate, 4, 1, dose);       // display one digit after decimal point when dose is greater than 10
      }
      if ((doseRate >= 100) && (doseRate < 10000)) 
      {
        dtostrf(doseRate, 4, 0, dose);       // whole number only when dose is higher than 100
      }

      tft.setFont();
      tft.setCursor(75, 122);
      tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
      tft.setTextSize(3);
      tft.println(averageCount);         // Display CPM 
      if (averageCount < 10)
      {
        tft.fillRect(92, 120, 100, 25, ILI9341_BLACK);
      }
      else if (averageCount < 100)
      {
        tft.fillRect(109, 120, 90, 25, ILI9341_BLACK); 
      }
      else if (averageCount < 1000)
      {
        tft.fillRect(126, 120, 75, 25, ILI9341_BLACK);
      }
      else if (averageCount < 10000)
      {
        tft.fillRect(143, 120, 60, 25, ILI9341_BLACK);
      }
      else if (averageCount < 100000)
      {
        tft.fillRect(162, 120, 45, 25, ILI9341_BLACK);
      }
      tft.setCursor(80, 192);
      tft.setTextSize(2);
      tft.setTextColor(ILI9341_WHITE, 0x630C);
      tft.println(cumulativeCount);

      tft.setCursor(80, 222);
      tft.println(totalDose);

      tft.setCursor(44, 52);
      tft.setTextSize(5);
      tft.setTextColor(ILI9341_WHITE, DOSEBACKGROUND);
      tft.println(dose);
      tft.setTextSize(1);

      if (doseLevel != previousDoseLevel)   // only update alert level if it changed. This prevents flicker
      {
        if (doseLevel == 0)
        {
          tft.drawRect(0, 0, tft.width(), tft.height(), ILI9341_WHITE);
          tft.fillRoundRect(3, 94, 234, 21, 3, 0x2DC6);
          tft.setCursor(15, 104);
          tft.setFont(&FreeSans9pt7b);
          tft.setTextColor(ILI9341_WHITE);
          tft.setTextSize(1);
          tft.println("NORMAL BACKGROUND");

          previousDoseLevel = doseLevel;
        }
        else if (doseLevel == 1)
        {
          tft.drawRect(0, 0, tft.width(), tft.height(), ILI9341_WHITE);
          tft.fillRoundRect(3, 94, 234, 21, 3, 0xCE40);
          tft.setCursor(29, 104);
          tft.setFont(&FreeSans9pt7b);
          tft.setTextColor(ILI9341_WHITE);
          tft.setTextSize(1);
          tft.println("ELEVATED ACTIVITY");

          previousDoseLevel = doseLevel;
        }
        else if (doseLevel == 2)
        {
          tft.drawRect(0, 0, tft.width(), tft.height(), ILI9341_RED);
          tft.fillRoundRect(3, 94, 234, 21, 3, 0xB8A2);
          tft.setCursor(17, 104);
          tft.setFont(&FreeSans9pt7b);
          tft.setTextColor(ILI9341_WHITE);
          tft.setTextSize(1);
          tft.println("HIGH RADIATION LEVEL");

          previousDoseLevel = doseLevel;
        }
      }
    }       // end of millis()-controlled loop that runs once every second. The rest of the code on page 1 runs every loop
    if (currentCount > previousCount)
    {
      if (ledSwitch)
        digitalWrite(D3, HIGH);                 // trigger buzzer and led if they are activated
      if (buzzerSwitch)
        digitalWrite(D0, HIGH);
      previousCount = currentCount;
      previousMicros = micros();
    }
    currentMicros = micros();
    if (currentMicros - previousMicros >= 200)
    {
      digitalWrite(D3, LOW);            
      digitalWrite(D0, LOW);
      previousMicros = currentMicros;
    }

    if (!ts.touched())
      wasTouched = 0;
    if (ts.touched() && !wasTouched)     // A way of "debouncing" the touchscreen. Prevents multiple inputs from single touch
    {
      wasTouched = 1;
      TS_Point p = ts.getPoint();
      x = map(p.x, TS_MINX, TS_MAXX, 240, 0);    // get touch point and map to screen pixels
      y = map(p.y, TS_MINY, TS_MAXY, 320, 0);

      if ((x > 162 && x < 238) && (y > 259 && y < 318))  
      {
        integrationMode = !integrationMode;                  
        currentCount = 0;
        previousCount = 0;
        for (int a = 0; a < 60; a++)    // reset counts when integretation speed is changed
        {
          count[a] = 0;
        }
        for (int b = 0; b < 4; b++)
        {
          fastCount[b] = 0;
        }
        if (integrationMode == 0)  // change button based on touch and previous state
        {
          tft.fillRoundRect(162, 259, 75, 57, 3, 0x2A86);
          tft.setFont(&FreeSans12pt7b);
          tft.setTextSize(1);
          tft.setCursor(163, 284);
          tft.println("SLOW");
          tft.setCursor(164, 309);
          tft.println("MODE");
        }
        else
        {
          tft.fillRoundRect(162, 259, 75, 57, 3, 0x2A86);
          tft.setFont(&FreeSans12pt7b);
          tft.setTextSize(1);
          tft.setCursor(169, 284);
          tft.println("FAST");
          tft.setCursor(164, 309);
          tft.println("MODE");
        }
      }
      else if ((x > 64 && x < 159) && (y > 259 && y < 318))   // reset count
      {
        currentCount = 0;
        previousCount = 0;
        for (int a = 0; a < 60; a++)
        {
          count[a] = 0;
        }
        for (int b = 0; b < 4; b++)
        {
          fastCount[b] = 0;
        }
      }
      else if ((x > 190 && x < 238) && (y > 151 && y < 202))  // toggle LED
      {
        ledSwitch = !ledSwitch;
        if (ledSwitch)
        {
          tft.fillRoundRect(190, 151, 46, 51, 3, 0x6269);
          tft.drawBitmap(190, 153, ledOnBitmap, 45, 45, ILI9341_WHITE);
        }
        else
        {
          tft.fillRoundRect(190, 151, 46, 51, 3, 0x6269);
          tft.drawBitmap(190, 153, ledOffBitmap, 45, 45, ILI9341_WHITE);
        }
      }
      else if ((x > 190 && x < 238) && (y > 205 && y < 256))      // toggle buzzer
      {
        buzzerSwitch = !buzzerSwitch;
        if (buzzerSwitch)
        {
          tft.fillRoundRect(190, 205, 46, 51, 3, 0x6269);
          tft.drawBitmap(190, 208, buzzerOnBitmap, 45, 45, ILI9341_WHITE);
        }
        else
        {
          tft.fillRoundRect(190, 205, 46, 51, 3, 0x6269);
          tft.drawBitmap(190, 208, buzzerOffBitmap, 45, 45, ILI9341_WHITE);
        }
      }
      else if ((x > 3 && x < 61) && (y > 259 && y < 316))     // settings button pressed
      {
        page = 1;
        drawSettingsPage();
      }
    }
  }
  else if (page == 1)    // settings page. all display elements are drawn when drawSettingsPage() is called
  {
    if (!ts.touched())
      wasTouched = 0;
    if (ts.touched() && !wasTouched)
    {
      wasTouched = 1;
      TS_Point p = ts.getPoint();
      x = map(p.x, TS_MINX, TS_MAXX, 240, 0);
      y = map(p.y, TS_MINY, TS_MAXY, 320, 0);

      if ((x > 6 && x < 71) && (y > 250 && y < 315))        // back button. draw homepage, reset counts and go back
      {
        page = 0;
        drawHomePage();
        currentCount = 0;
        previousCount = 0;
        for (int a = 0; a < 60; a++)
        {
          count[a] = 0;                         // counts need to be reset to prevent errorenous readings
        }
        for (int b = 0; b < 4; b++)
        {
          fastCount[b] = 0;
        }
      }
      else if ((x > 4 && x < 234) && (y > 70 && y < 120))
      {
        page = 2;
        drawUnitsPage();
      }
      else if ((x > 4 && x < 234) && (y > 127 && y < 177))
      {
        page = 3;
        drawAlertPage();
      }
      else if ((x > 4 && x < 234) && (y > 184 && y < 234))
      {
        page = 4;
        drawCalibrationPage();
      }
    }
  }
  else if (page == 2)      // units page
  {
    if (!ts.touched())
      wasTouched = 0;
    if (ts.touched() && !wasTouched)
    {
      wasTouched = 1;
      TS_Point p = ts.getPoint();
      x = map(p.x, TS_MINX, TS_MAXX, 240, 0);
      y = map(p.y, TS_MINY, TS_MAXY, 320, 0);

      if ((x > 6 && x < 71) && (y > 250 && y < 315))     // back button
      {
        page = 1;
        EEPROM.write(saveUnits, doseUnits);             // save current units to EEPROM during exit. This will be retrieved at startup
        EEPROM.commit();
        drawSettingsPage();
      }
      else if ((x > 4 && x < 234) && (y > 70 && y < 120))
      {
        doseUnits = 0;
        tft.fillRoundRect(4, 71, 232, 48, 4, 0x2A86);
        tft.setCursor(30, 103);
        tft.println("Sieverts (uSv/hr)");

        tft.fillRoundRect(4, 128, 232, 48, 4, ILI9341_BLACK);
        tft.setCursor(47, 160);
        tft.println("Rem (mR/hr)");
      }
      else if ((x > 4 && x < 234) && (y > 127 && y < 177))
      {
        doseUnits = 1;
        tft.fillRoundRect(4, 71, 232, 48, 4, ILI9341_BLACK);
        tft.setCursor(30, 103);
        tft.println("Sieverts (uSv/hr)");

        tft.fillRoundRect(4, 128, 232, 48, 4, 0x2A86);
        tft.setCursor(47, 160);
        tft.println("Rem (mR/hr)");
      }
    }
  }
  else if (page == 3)
  {
    tft.setFont();
    tft.setTextSize(3);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.setCursor(151, 146);
    tft.println(alarmThreshold);
    if (alarmThreshold < 10)
      tft.fillRect(169, 146, 22, 22, ILI9341_BLACK);

    if (!ts.touched())
      wasTouched = 0;
    if (ts.touched() && !wasTouched)
    {
      wasTouched = 1;
      TS_Point p = ts.getPoint();
      x = map(p.x, TS_MINX, TS_MAXX, 240, 0);
      y = map(p.y, TS_MINY, TS_MAXY, 320, 0);

      if ((x > 6 && x < 71) && (y > 250 && y < 315))
      {
        page = 1;
        EEPROM.write(saveAlertThreshold, alarmThreshold); 
        EEPROM.commit();                    // save to EEPROM to be retrieved at startup
        drawSettingsPage();
      }
      else if ((x > 130 && x < 190) && (y > 70 && y < 120))
      {
        alarmThreshold++;
        if (alarmThreshold > 100)
          alarmThreshold = 100;
      }
      else if ((x > 130 && x < 190) && (y > 185 && y < 245))
      {
        alarmThreshold--;
        if (alarmThreshold <= 2)
          alarmThreshold = 2;
      }
    }
   
  }
  else if (page == 4)
  {
    tft.setFont();
    tft.setTextSize(3);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.setCursor(161, 146);
    tft.println(conversionFactor);
    if (conversionFactor < 100)
      tft.fillRect(197, 146, 22, 22, ILI9341_BLACK);

    if (!ts.touched())
      wasTouched = 0;
    if (ts.touched() && !wasTouched)
    {
      wasTouched = 1;
      TS_Point p = ts.getPoint();
      x = map(p.x, TS_MINX, TS_MAXX, 240, 0);
      y = map(p.y, TS_MINY, TS_MAXY, 320, 0);

      if ((x > 6 && x < 71) && (y > 250 && y < 315))
      {
        page = 1;
        EEPROM.write(saveCalibration, conversionFactor);
        EEPROM.commit();
        drawSettingsPage();
      }
      else if ((x > 160 && x < 220) && (y > 70 && y < 120))
      {
        conversionFactor++;
      }
      else if ((x > 160 && x < 220) && (y > 185 && y < 245))
      {
        conversionFactor--;
        if (conversionFactor <= 1)
          conversionFactor = 1;
      }
    }
  
  }
}

void drawHomePage()  
{
  tft.fillRect(2, 21, 236, 298 ,ILI9341_BLACK);
  tft.drawRect(0, 0, tft.width(), tft.height(), ILI9341_WHITE);

  tft.drawRoundRect(210, 4, 26, 14, 3, ILI9341_WHITE);
  tft.drawLine(209, 8, 209, 13, ILI9341_WHITE); // Battery symbol
  tft.drawLine(208, 8, 208, 13, ILI9341_WHITE);
  tft.fillRect(212, 6, 22, 10, ILI9341_BLACK);
  tft.fillRect(batteryMapped, 6, (234 - batteryMapped), 10, ILI9341_GREEN);

  tft.setTextColor(ILI9341_CYAN);
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(2, 16);
  tft.println("GC-01");
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(73, 16);
  tft.println("Beta/Gamma");

  tft.drawLine(1, 20, 238, 20, ILI9341_WHITE);
  tft.fillRoundRect(3, 23, 234, 69, 3, DOSEBACKGROUND);
  tft.setCursor(16, 40);
  tft.println("EFFECTIVE DOSE RATE:");
  tft.setCursor(165, 85);
  tft.setFont(&FreeSans12pt7b);
  if (doseUnits == 0)
  {
    tft.println("uSv/hr");
  }
  else if (doseUnits == 1)
  {
    tft.println("mR/hr");
  }

  tft.fillRoundRect(3, 94, 234, 21, 3, 0x2DC6);
  tft.setCursor(15, 110);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.println("NORMAL BACKGROUND");

  tft.setFont(&FreeSans12pt7b);
  tft.setCursor(7, 141);
  tft.println("CPM:");
  tft.drawRoundRect(3, 117, 234, 32, 3, DOSEBACKGROUND);

  tft.fillRoundRect(3, 151, 185, 105, 4, 0x630C);
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(9, 171);
  tft.println("CUMULATIVE DOSE");
  tft.setCursor(7, 205);
  tft.println("Counts:");
  if (doseUnits == 0)
  {
    tft.setCursor(34, 235);
    tft.println("uSv:");
  }
  else if (doseUnits == 1)
  {
    tft.setCursor(37, 235);
    tft.println("mR:");
  }

  tft.fillRoundRect(3, 259, 58, 57, 3, 0x3B8F);
  tft.drawBitmap(1, 257, settingsBitmap, 60, 60, ILI9341_WHITE);

  tft.fillRoundRect(64, 259, 95, 57, 3, 0x6269);
  tft.setFont(&FreeSans12pt7b);
  tft.setCursor(74, 284);
  tft.println("RESET");
  tft.setCursor(70, 309);
  tft.println("COUNT");

  if (integrationMode == 0)
  {
    tft.fillRoundRect(162, 259, 75, 57, 3, 0x2A86);
    tft.setCursor(163, 284);
    tft.println("SLOW");
    tft.setCursor(164, 309);
    tft.println("MODE");
  }
  else if (integrationMode == 1)
  {
    tft.fillRoundRect(162, 259, 75, 57, 3, 0x2A86);
    tft.setCursor(169, 284);
    tft.println("FAST");
    tft.setCursor(164, 309);
    tft.println("MODE"); 
  }
  if (ledSwitch)
  {
    tft.fillRoundRect(190, 151, 46, 51, 3, 0x6269);
    tft.drawBitmap(190, 153, ledOnBitmap, 45, 45, ILI9341_WHITE);
  }
  else if (!ledSwitch)
  {
    tft.fillRoundRect(190, 151, 46, 51, 3, 0x6269);
    tft.drawBitmap(190, 153, ledOffBitmap, 45, 45, ILI9341_WHITE);
  }
  if (buzzerSwitch)
  {
    tft.fillRoundRect(190, 205, 46, 51, 3, 0x6269);
    tft.drawBitmap(190, 208, buzzerOnBitmap, 45, 45, ILI9341_WHITE);
  }
  else if (!buzzerSwitch)
  {
    tft.fillRoundRect(190, 205, 46, 51, 3, 0x6269);
    tft.drawBitmap(190, 208, buzzerOffBitmap, 45, 45, ILI9341_WHITE);
  }
}

void drawSettingsPage()
{
  digitalWrite(D3, LOW);
  digitalWrite(D0, LOW);

  tft.fillRect(2, 21, 236, 298 ,ILI9341_BLACK);
  tft.drawRect(0, 0, tft.width(), tft.height(), ILI9341_WHITE);

  tft.drawRoundRect(210, 4, 26, 14, 3, ILI9341_WHITE);
  tft.drawLine(209, 8, 209, 13, ILI9341_WHITE);         // Battery symbol
  tft.drawLine(208, 8, 208, 13, ILI9341_WHITE);
  tft.fillRect(212, 6, 22, 10, ILI9341_BLACK);
  tft.fillRect(batteryMapped, 6, (234 - batteryMapped), 10, ILI9341_GREEN);

  tft.setTextColor(ILI9341_CYAN);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextSize(1);
  tft.setCursor(2, 16);
  tft.println("GC-01");
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(73, 16);
  tft.println("Beta/Gamma");

  tft.drawLine(1, 20, 238, 20, ILI9341_WHITE);

  tft.fillRoundRect(3, 23, 234, 40, 3, 0x3B8F);
  tft.setFont(&FreeSans12pt7b);
  tft.setCursor(57, 51);
  tft.println("SETTINGS");
  tft.drawFastHLine(59, 55, 117, WHITE);

  tft.drawRoundRect(3, 70, 234, 50, 4, WHITE);
  tft.fillRoundRect(4, 71, 232, 48, 4, 0x2A86);
  tft.setCursor(44, 103);
  tft.println("DOSE UNITS");

  tft.drawRoundRect(3, 127, 234, 50, 4, WHITE);
  tft.fillRoundRect(4, 128, 232, 48, 4, 0x2A86);
  tft.setCursor(5, 160);
  tft.println("ALERT THRESHOLD");

  tft.drawRoundRect(3, 184, 234, 50, 4, WHITE);
  tft.fillRoundRect(4, 185, 232, 48, 4, 0x2A86);
  tft.setCursor(37, 217);
  tft.println("CALIBRATION");

  tft.fillCircle(38, 282, 30, 0x3B8F);
  tft.drawBitmap(6, 250, backBitmap, 65, 65, ILI9341_WHITE);
} 

void drawUnitsPage()
{
  tft.fillRect(2, 21, 236, 298 ,ILI9341_BLACK);
  tft.drawRect(0, 0, tft.width(), tft.height(), ILI9341_WHITE);

  tft.drawRoundRect(210, 4, 26, 14, 3, ILI9341_WHITE);
  tft.drawLine(209, 8, 209, 13, ILI9341_WHITE);         // Battery symbol
  tft.drawLine(208, 8, 208, 13, ILI9341_WHITE);
  tft.fillRect(212, 6, 22, 10, ILI9341_BLACK);
  tft.fillRect(batteryMapped, 6, (234 - batteryMapped), 10, ILI9341_GREEN);

  tft.setCursor(2, 16);
  tft.setTextColor(ILI9341_CYAN);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextSize(1);
  tft.println("GC-01");
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(73, 16);
  tft.println("Beta/Gamma");

  tft.drawLine(1, 20, 238, 20, ILI9341_WHITE);

  tft.fillRoundRect(3, 23, 234, 40, 3, 0x3B8F);
  tft.setFont(&FreeSans12pt7b);
  tft.setCursor(84, 51);
  tft.println("UNITS");
  tft.drawFastHLine(86, 55, 71, WHITE);

  tft.fillCircle(38, 282, 30, 0x3B8F);
  tft.drawBitmap(6, 250, backBitmap, 65, 65, ILI9341_WHITE);

  tft.drawRoundRect(3, 70, 234, 50, 4, WHITE);
  if (doseUnits == 0) 
    tft.fillRoundRect(4, 71, 232, 48, 4, 0x2A86);
  tft.setCursor(30, 103);
  tft.println("Sieverts (uSv/hr)");

  tft.drawRoundRect(3, 127, 234, 50, 4, WHITE);
  if (doseUnits == 1)
    tft.fillRoundRect(4, 128, 232, 48, 4, 0x2A86);
  tft.setCursor(47, 160);
  tft.println("Rem (mR/hr)");

}

void drawAlertPage()
{
  tft.fillRect(2, 21, 236, 298 ,ILI9341_BLACK);
  tft.drawRect(0, 0, tft.width(), tft.height(), ILI9341_WHITE);

  tft.drawRoundRect(210, 4, 26, 14, 3, ILI9341_WHITE);
  tft.drawLine(209, 8, 209, 13, ILI9341_WHITE);         // Battery symbol
  tft.drawLine(208, 8, 208, 13, ILI9341_WHITE);
  tft.fillRect(212, 6, 22, 10, ILI9341_BLACK);
  tft.fillRect(batteryMapped, 6, (234 - batteryMapped), 10, ILI9341_GREEN);

  tft.setCursor(2, 16);
  tft.setTextColor(ILI9341_CYAN);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextSize(1);
  tft.println("GC-01");
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(73, 16);
  tft.println("Beta/Gamma");

  tft.drawLine(1, 20, 238, 20, ILI9341_WHITE);

  tft.fillRoundRect(3, 23, 234, 40, 3, 0x3B8F);
  tft.setFont(&FreeSans12pt7b);
  tft.setCursor(4, 51);
  tft.println("ALERT THRESHOLD");
  tft.drawFastHLine(5, 55, 229, WHITE);

  tft.fillCircle(38, 282, 30, 0x3B8F);
  tft.drawBitmap(6, 250, backBitmap, 65, 65, ILI9341_WHITE);

  tft.setCursor(30, 164);
  tft.println("uSv/hr:");

  tft.drawRoundRect(130, 70, 60, 60, 4, ILI9341_WHITE);
  tft.fillRoundRect(131, 71, 58, 58, 4, 0x2A86);
  tft.drawRoundRect(130, 185, 60, 60, 4, ILI9341_WHITE);
  tft.fillRoundRect(131, 186, 58, 58, 4, 0x2A86);

  tft.setCursor(140, 113);
  tft.setTextSize(3);
  tft.println("+");
  tft.setCursor(148, 232);
  tft.println("-");
  tft.setTextSize(1);
}

void drawCalibrationPage()
{
  tft.fillRect(2, 21, 236, 298 ,ILI9341_BLACK);
  tft.drawRect(0, 0, tft.width(), tft.height(), ILI9341_WHITE);

  tft.drawRoundRect(210, 4, 26, 14, 3, ILI9341_WHITE);
  tft.drawLine(209, 8, 209, 13, ILI9341_WHITE);         // Battery symbol
  tft.drawLine(208, 8, 208, 13, ILI9341_WHITE);
  tft.fillRect(212, 6, 22, 10, ILI9341_BLACK);
  tft.fillRect(batteryMapped, 6, (234 - batteryMapped), 10, ILI9341_GREEN);

  tft.setCursor(2, 16);
  tft.setTextColor(ILI9341_CYAN);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextSize(1);
  tft.println("GC-01");
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(73, 16);
  tft.println("Beta/Gamma");

  tft.drawLine(1, 20, 238, 20, ILI9341_WHITE);

  tft.fillRoundRect(3, 23, 234, 40, 3, 0x3B8F);
  tft.setFont(&FreeSans12pt7b);
  tft.setCursor(47, 51);
  tft.println("CALIBRATE");
  tft.drawFastHLine(48, 55, 133, WHITE);

  tft.fillCircle(38, 282, 30, 0x3B8F);
  tft.drawBitmap(6, 250, backBitmap, 65, 65, ILI9341_WHITE);

  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(8, 154);
  tft.println("Conversion Factor");
  tft.setCursor(8, 174);
  tft.println("(CPM per uSv/hr)");

  tft.drawRoundRect(160, 70, 60, 60, 4, ILI9341_WHITE);
  tft.fillRoundRect(161, 71, 58, 58, 4, 0x2A86);
  tft.drawRoundRect(160, 185, 60, 60, 4, ILI9341_WHITE);
  tft.fillRoundRect(161, 186, 58, 58, 4, 0x2A86);

  tft.setCursor(170, 113);
  tft.setFont(&FreeSans12pt7b);
  tft.setTextSize(3);
  tft.println("+");
  tft.setCursor(178, 232);
  tft.println("-");
  tft.setTextSize(1);

}

void isr()  // interrupt service routine 
{
  currentCount++;
  cumulativeCount++;
}