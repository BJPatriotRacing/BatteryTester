/*

  Program name: Battery Tester
  Property: Bob Jones High School, Patriot Racing
  Copyright: 2018 (C) All rights reserved

  Purpose: Output on display Volts, Amps, Power, and Energy E-Time(Elapsed Time); Analyze data collected;

  Revision table
  ________________________________
  rev   author      date        desc
  Board 1 (jumper based)
  1.0   Jacob       1-9-15      initial creation
  2.0   kasprak     2-18-16     added PWM for setting loads
  3.0   kasprak     6-10-16     added auto-optimze for setting loads
  4.0   kasprak     6-18-16     added buttons to input filename
  6.0   kasprak     6-18-16     increased settle time for load optimizer
  7.0   kasprak     3-25-17     added support for break in battery time
  8.0   kasprak     4-18-17     added better filename input

  Board V3 (Garage PCB)
  20.0  kasprzak    12/14/2017  converted to Teensy, 2.8 display, touch screen
  21.0  kasprzak    2/24/2017   removed cycle routing (it didn't help recover batteries)
  22.0  kasprzak    3/28/2017   cleaned up variable names
  23.0  kasprzak    4/17/2018   added support for double digit runs
  26.0  kasprzak    6/7/2018    increased cuttoff to 11.0
  28.0  kasprzak    1/14/2019   changed filename, storing run and cycle data in EEPROM
  30.0  kasprzak    8/19/2019   RTC support
  31.0  kasprzak    8/28/2019   merged code paths for gen2 and gen3
  31.1  kasprzak    9/2/2019   added data/time to file create
  33.0  kasprzak    11/18/2020   updated slider and checkbox controls

  Board V4 (new PCB garage printed)
  01.0 kasprzak  11/11/2021  added code to create a summary table

  Board V5 (2-layer PCB)
  02.0 kasprzak  11/11/2022  added memory and chip to eliminate need for and SD card
  02.1 kasprzak  2/07/2023  reworked filename for BB_YYYY_MM_DD_TT where CC is the tester number

  Board v5 (2-layer PCB board with integrated SSD chip)
  03.0 kasprzak  4/22/2023  #ifdef for BJHS or DMS, added tester temp limit control


*/

 #define DISPLAY_HEADER_YELLOW
//
#define TESTER_FOR_PATRIOT_RACING

// #define TEST_SCREEN
////////////////////////////////////////////////////////////////////////////////
// #define debug
////////////////////////////////////////////////////////////////////////////////

#define CODE_VERSION "B5V03.1"


#include <SPI.h>                    // lib to talk to the SPI bus, not really needed
#include <ILI9341_t3.h>             // https://github.com/PaulStoffregen/ILI9341_t3
#include <SdFat.h>                  // lib to talk to the SD card
#include <EEPROM.h>                 // lib to talk to teensy EEPROM
#include <font_ArialBold.h>         // https://github.com/PaulStoffregen/ILI9341_fonts
#include <font_Arial.h>             // https://github.com/PaulStoffregen/ILI9341_fonts
#include <ILI9341_t3_Controls.h>    // https://github.com/KrisKasprzak/ILI9341_t3_controls
#include "PatriotRacing_Utilities.h"   // custom utilities definition
#include <ILI9341_t3_PrintScreen_SD.h> //https://github.com/KrisKasprzak/ILI9341_t3_PrintScreen
#include <TimeLib.h>                // https://github.com/PaulStoffregen/Time
#include <FlickerFreePrint.h>       // https://github.com/KrisKasprzak/FlickerFreePrint
#include <XPT2046_Touchscreen.h>    // https://github.com/PaulStoffregen/XPT2046_Touchscreen
#include <LittleFS.h>               // https://github.com/PaulStoffregen/LittleFS

#define TIME_HEADER  "T"


#define SSD_PIN  1     // ssd chip buzzer
#define BUZZ_PIN  4     // alert buzzer
#define CNT_PIN   6     // control pin for mosfet
#define TFT_RST   8     // rst
#define TFT_DC    9     // DC pin on LCD
#define TFT_CS    10    // chip select pin on LCD
#define AM_PIN    A0    // amp volt measurement pin
#define HTRTEMP_PIN  19    // temp volt measurement pin (not included in PCB
#define LCD_PIN   A7    // lcd pin to control brightness
#define SD_CS     A8    // chip select  pin on sd card
#define VM_PIN    A9    // volt measurement pin
#define T_CSP 5
#define T_IRQ 2
#define OVER_TEMP_LIMIT 120
#define TOTAL_TESTS 25 // maximum tests an 8 mb flash chip can hold

// variables for the locations of the keypad buttons
#define BUTTON_X 100
#define BUTTON_Y 80
#define BUTTON_W 60
#define BUTTON_H 30
#define BUTTON_SPACING_X 10
#define BUTTON_SPACING_Y 10
#define BUTTON_TEXTSIZE 2
#define C_RADIUS 4

#define MOSFET_ON true
#define MOSFET_OFF false

#define GRAPH_X 47
#define GRAPH_Y 205
#define GRAPH_W 213
#define GRAPH_H 170

#define FONT_HEADER     Arial_24_Bold  // font for the small data
#define FONT_HEADING    Arial_18  // font for all headings
#define FONT_SMALL      Arial_14       // font for menus
#define FONT_SMALL_BOLD Arial_14_Bold       // font for menus

#ifndef TESTER_FOR_PATRIOT_RACING
#define HANDLE_COLOR    C_DKCYAN     // handle color
#define TEAM_NAME    "Panther Racing" 
#endif

#ifdef TESTER_FOR_PATRIOT_RACING
#define HANDLE_COLOR    C_ORANGE     // handle color
#define TEAM_NAME    "Patriot Racing" 
#endif

////////////////////////////////////////////////////////////////////////

unsigned long SDUpdateTime = 5000;        // milliseconds between SD writes
unsigned long ComputeUpdate = 1000;       // milliseconds between compute upates
float BatteryCutOff = 11.0;               // cutoff volts, point where we terminate test to protect battery
byte TotalTests = 0;                      // variable to record the number to tests
/////////////////////////////////////////////////////////////////////////////////////////
// variables for the graphing function
bool KeepIn;
bool IsSD = false;
bool TestHasStarted = false;
byte BatTemp, BatTempID0, BatTempID1, BatTempID2;
byte Battery, Tester, b;  // holders for filename formatting
// strings for the buttons
char str[40];
char buf[40];
char KeyPadBtnText[12][5] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "Done", "0", "Can" };

char FileName[25] = "BB_YYYY-MM-DD_T_NN.csv";
char BMPName[25] = "BB_YYYY-MM-DD_T_NN.BMP";
char BatteryNumber[4] = "BB";
int i = 0;                                  // general counter variable
int h, m, s;                                // holders for time formatting
int row, col, BtnX, BtnY, top, wide, high, Left;  // holders for screen coordinate drawing
int Point;                                  // holder for current data point
int PowerID, EnergyID, bPowerID, bEnergyID;
int LastMonth, LastDay, LastYear, DaysSinceLastTest;
uint16_t LastEnergy = 0;
// color definitions for the buttons
uint16_t KeyPadBtnColor[12] = {C_BLUE, C_BLUE, C_BLUE,
                               C_BLUE, C_BLUE, C_BLUE,
                               C_BLUE, C_BLUE, C_BLUE,
                               C_DKGREEN, C_BLUE, C_DKRED
                              };
float R1 = 4650.0;          // resistor for the voltage divider
float R2 = 987.0;           // resistor for the voltage divider
float VVolts = 0.0;         // measured pin output (0-1023)
float Volts = 0.0;          // computed Volts
float AVolts = 0.0;         // amp meter Volts
float Amps = 0.0;           // computed Amps
float AmpOffset = 0.5 ;     // calibration adjustment for amperage offset
float mVPerAmp = 80.0 ;     // slope from data sheet ACS770-50U
float VoltOffset = 1.0 ;    // calibration adjustment for voltage offset
float VoltSlope = 5.7 ;     // calibration adjustment for voltage slope
float Power = 0.0;          // computed power
float Energy = 0.0;         // computed energy
float TempNum;              // some temp number
float thVolts = 0.0, TempF = 0.0, TempK = 0.0;    // computed temp values
float TesterTemp = 0.0;

unsigned long tr2 = 0;
unsigned long Counter = 0;          // counter for averaging
unsigned long RunTest = true;       // flag to run/stop battery profiling

// https://javl.github.io/image2cpp/
// 'Small', 70x30px
const uint16_t LOGO [] PROGMEM = {
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdedb, 0x6b4d, 0x4208, 0x632c, 0xd6ba, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffdf, 0x9492,
  0x8c71, 0xef7d, 0x94b2, 0x8410, 0xf7be, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xef7d, 0x7bef, 0xc618, 0xffff, 0xdefb, 0x5aeb, 0xe73c, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xe73c, 0x738e, 0xd6ba, 0xffff, 0xffff, 0x738e,
  0xb5b6, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xdefb, 0x6b6d, 0xe73c, 0xffff, 0xffff, 0xad75, 0x7bcf, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xd6ba, 0x632c, 0xf79e, 0xffff, 0xffff, 0xe73c, 0x4a49, 0xf7be, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xbdf7, 0x738e, 0xffff, 0xffff,
  0xffff, 0xffff, 0x6b6d, 0xbdd7, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xad55, 0x8c51, 0xffff, 0xffff, 0xffff, 0xffff, 0xa534, 0x8410, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x9492, 0x9cf3, 0xffff, 0xffff, 0xffff, 0xffff, 0xd6ba, 0x632c,
  0xef7d, 0xffff, 0xffff, 0xef5d, 0xce59, 0xf79e, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x8430, 0xc618,
  0xffff, 0xffff, 0xffff, 0xffff, 0xef7d, 0x73ae, 0xc638, 0xffdf, 0xb5b6, 0x528a, 0x632c, 0x6b6d, 0xe71c, 0xffdf, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xff5a, 0xf632, 0xfdcf, 0xfe75, 0xffbd, 0xffff, 0xffff,
  0xffff, 0xffdf, 0xe6fc, 0xbdb9, 0x7bb2, 0x9495, 0xc5f9, 0xf77e, 0xffff, 0xffff, 0xffdf, 0x8c71, 0xa514, 0xef5d, 0x6b6d, 0xb5b6,
  0xce7b, 0x7c33, 0x8455, 0x8c96, 0x9d38, 0xbe1a, 0xef7e, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xff59, 0xfe50, 0xfe51, 0xff18, 0xfffe, 0xff9c,
  0xf6d7, 0xf4e9, 0xfbe3, 0xf5af, 0xffde, 0xffff, 0xef5d, 0x83f3, 0x398d, 0x290c, 0x18aa, 0x188a, 0x290c, 0x524f, 0xa4f7, 0xef5d,
  0xffff, 0xb5b6, 0x8c51, 0xe73c, 0x8c72, 0x73f4, 0x4a91, 0x3210, 0x29ef, 0x2a10, 0x3230, 0x42b2, 0x63b4, 0xbe1a, 0xf7bf, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffde, 0xff9b, 0xff7a, 0xff9b, 0xff7b,
  0xf738, 0xff7a, 0xf6d5, 0xfcc5, 0xf508, 0xfeb5, 0xff7b, 0xff39, 0xfc87, 0xf3e4, 0xf77c, 0xce3a, 0x6b31, 0x9495, 0xdebc, 0xef5d,
  0xdefc, 0xb598, 0x6311, 0x20cb, 0x106a, 0x20ec, 0x4a50, 0x7393, 0x7bd4, 0x5b12, 0x29ae, 0x298f, 0x29cf, 0x29f0, 0x2a10, 0x3230,
  0x3251, 0x3251, 0x3271, 0x3271, 0x5353, 0xadda, 0xef7e, 0xffdf, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xf737, 0xfeb1, 0xfe6e, 0xfe0d, 0xfe0d, 0xfe71, 0xfed5, 0xfe94, 0xfcc5, 0xfc00, 0xfc22, 0xfcc8, 0xf50a, 0xf487, 0xfdf1,
  0xef3c, 0xad37, 0xd6bb, 0xef5c, 0xa514, 0x6b6d, 0x52aa, 0x630c, 0x9492, 0x8c75, 0x314d, 0x18ab, 0x18ec, 0x190c, 0x212d, 0x216e,
  0x218f, 0x29cf, 0x29f0, 0x2a10, 0x3231, 0x3251, 0x3271, 0x3292, 0x3292, 0x3292, 0x3292, 0x3ab2, 0x7435, 0xb5fa, 0xef9e, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffdf, 0xef7d, 0xf7be, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffbc, 0xff9c, 0xffbc, 0xff16, 0xfd24, 0xfca1, 0xfcc4, 0xfd06, 0xfc82,
  0xfc00, 0xfbe0, 0xf4e7, 0xfe94, 0xff19, 0xffbe, 0xef7e, 0xef5d, 0xdebb, 0x528a, 0x4a49, 0x8430, 0x9cd3, 0x630c, 0x2104, 0x8430,
  0xa537, 0x318e, 0x18ec, 0x192d, 0x214d, 0x216e, 0x29af, 0x29cf, 0x2a10, 0x3230, 0x3271, 0x3291, 0x32b2, 0x3ab2, 0x3ad2, 0x3ad2,
  0x3ad2, 0x32b2, 0x3292, 0x42d2, 0x5b94, 0x8476, 0x94d7, 0x8c96, 0x73d4, 0x6353, 0x5b12, 0x6b73, 0x9495, 0xce7b, 0xffff, 0xef7d,
  0x9cd3, 0x4228, 0x39c7, 0x4228, 0xa514, 0xf7be, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xf758,
  0xf62e, 0xfd68, 0xfe50, 0xff7b, 0xffbc, 0xf5ce, 0xfca5, 0xfc21, 0xfbc0, 0xfce9, 0xfe33, 0xf634, 0xff9d, 0xbdd7, 0x4228, 0xa514,
  0xffdf, 0xffff, 0xffff, 0xffff, 0x9cf3, 0x2945, 0xd6ba, 0xce7b, 0x52b0, 0x214d, 0x214e, 0x218e, 0x29cf, 0x29f0, 0x3230, 0x3251,
  0x3292, 0x32b2, 0x3ad3, 0x3af3, 0x3b13, 0x3b13, 0x3af3, 0x3ad3, 0x32b2, 0x3292, 0x3271, 0x3230, 0x2a10, 0x29cf, 0x218e, 0x216d,
  0x192d, 0x190c, 0x18cc, 0x18ab, 0x4a2f, 0x8c54, 0x738e, 0xa514, 0xb596, 0x8430, 0x2945, 0x6b6d, 0xef7d, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xff7b, 0xff9b, 0xf738, 0xfe0f, 0xfded, 0xfdef, 0xf5ef, 0xfca6, 0xfbc0, 0xfba0,
  0xfb80, 0xf52b, 0xdeba, 0x5acb, 0x9cf3, 0xffdf, 0xffff, 0xffff, 0xffff, 0xffff, 0xef7d, 0x3186, 0xb596, 0xffff, 0xdefc, 0x8455,
  0x3a30, 0x21af, 0x29cf, 0x2a10, 0x3231, 0x3271, 0x32b2, 0x3ad2, 0x3b13, 0x3b34, 0x4334, 0x4334, 0x3b34, 0x3b13, 0x3ad3, 0x32b2,
  0x3272, 0x3251, 0x2a10, 0x29ef, 0x3a50, 0x4a91, 0x4a70, 0x31ce, 0x190c, 0x18ab, 0x106b, 0x20ab, 0x62f1, 0xdebb, 0xffff, 0xffff,
  0xad55, 0x3186, 0xc618, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xff9c, 0xfed5,
  0xf58b, 0xfc83, 0xfc62, 0xfc21, 0xfbc0, 0xfba0, 0xfc24, 0xfed7, 0x8c51, 0x6b6d, 0xf79e, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0x39e7, 0xad75, 0xffff, 0xffff, 0xf79e, 0xce7b, 0x73f4, 0x3210, 0x2a10, 0x3251, 0x3291, 0x3ab2, 0x3af3, 0x3b33, 0x4354,
  0x4374, 0x4375, 0x4354, 0x3b33, 0x3af3, 0x3ad2, 0x3a92, 0x5b73, 0x8cd7, 0xbe1a, 0xd6bc, 0xdedc, 0xdedc, 0xce9b, 0xad78, 0x5ad1,
  0x108b, 0x104a, 0x102a, 0x4a2f, 0xce7b, 0xffff, 0xef5d, 0x5acb, 0x8c71, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xff5a, 0xfd6c, 0xfc84, 0xfc22, 0xfc86, 0xfe95, 0xf7be, 0x5aeb, 0xce59,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xf79e, 0x5acb, 0xd6ba, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdf1c, 0x9518,
  0x63b4, 0x4b53, 0x4333, 0x4333, 0x4b74, 0x4b95, 0x4bd6, 0x53f6, 0x5c16, 0x6416, 0x7cb7, 0xadd9, 0xdf3d, 0xffff, 0xffff, 0xffdf,
  0xd6ba, 0xa514, 0x738e, 0x6b6d, 0x9cd3, 0xd6bb, 0xa517, 0x294d, 0x082a, 0x0809, 0x398d, 0xc63a, 0xffff, 0x7bef, 0x73ae, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffdf, 0xffde, 0xffdf, 0xffbf, 0xffbe, 0xffbe, 0xffbe, 0xfef8,
  0xf654, 0xf719, 0xffbe, 0xf79e, 0xdedb, 0xf7be, 0xffbf, 0xffdf, 0xffbf, 0xffbf, 0xffdf, 0xffdf, 0xf79e, 0xdebb, 0xffbe, 0xffdf,
  0xffdf, 0xffdf, 0xffdf, 0xffbf, 0xffdf, 0xffdf, 0xef5d, 0xce7b, 0xb5f9, 0xadd9, 0xadda, 0xadfa, 0xb61a, 0xbe5b, 0xce9c, 0xe73d,
  0xf7be, 0xffdf, 0xffff, 0xffff, 0xf79e, 0x9cd3, 0x4228, 0x4a69, 0x6b4d, 0x5aeb, 0x2945, 0x4228, 0xd6ba, 0x9cf6, 0x20cb, 0x0809,
  0x0809, 0x4a0e, 0xe71c, 0x6b6d, 0x9492, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffdf, 0xa32d, 0x924a,
  0x9a4a, 0x9a4a, 0xa26a, 0xa28b, 0xe618, 0xbb4e, 0xb28b, 0xb28b, 0xba8b, 0xc28b, 0xcbd0, 0xdcd3, 0xcacb, 0xd2cb, 0xd2cb, 0xdacc,
  0xdaec, 0xe431, 0xebaf, 0xe2ec, 0xe2ec, 0xdaec, 0xdacc, 0xe3cf, 0xe4d3, 0xd32d, 0xdcb3, 0xd3cf, 0xc2ab, 0xba8b, 0xba8b, 0xb28b,
  0xc3d0, 0xc472, 0xa28b, 0xa24a, 0x9a4a, 0x924a, 0x8a4a, 0xb492, 0xf7be, 0xf79e, 0x8430, 0x4208, 0xb5b6, 0xe71c, 0xf7be, 0xef7d,
  0xc618, 0x3186, 0x738e, 0xef7e, 0x5270, 0x0809, 0x0809, 0x1009, 0x7b92, 0x73ae, 0xc618, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffdf, 0xbcd3, 0x7083, 0xa34e, 0xd5b7, 0xccf4, 0x9167, 0xa22a, 0xaa29, 0xa166, 0xd4d3, 0xe5d7, 0xcbcf, 0xb105,
  0xdcf4, 0xf75d, 0xee59, 0xc9a7, 0xdaec, 0xf69a, 0xf6fb, 0xe410, 0xd946, 0xec92, 0xf5f8, 0xed14, 0xd987, 0xdacc, 0xd229, 0xca09,
  0xd2ec, 0xc187, 0xee38, 0xf71c, 0xd4b3, 0xa926, 0xcc51, 0xf73d, 0xe679, 0x91a8, 0x91e9, 0xe679, 0xef1c, 0xf79e, 0xffdf, 0x8c71,
  0x4a69, 0xdefb, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xad55, 0x3186, 0xdedb, 0xad37, 0x0809, 0x0809, 0x0809, 0x20cb, 0x9cd6,
  0xf7be, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xe6ba, 0x81c8, 0x8167, 0xa32d, 0xab8e, 0xab4e, 0xb32d, 0xbb8f,
  0x98e4, 0xb26a, 0xc38f, 0xc30d, 0xa926, 0xba6a, 0xf75d, 0xffbe, 0xdc51, 0xc987, 0xed96, 0xffdf, 0xf679, 0xd9e8, 0xe249, 0xe2ab,
  0xe229, 0xe2ec, 0xe38e, 0xdb6e, 0xc966, 0xd38e, 0xc166, 0xdc51, 0xffbe, 0xf71c, 0xbaab, 0xb1e9, 0xf71c, 0xffff, 0xccb3, 0x88e5,
  0xccf4, 0xffdf, 0xffff, 0xffff, 0xd6ba, 0x4208, 0xbdf7, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdedb, 0x2945, 0xce79,
  0xc63a, 0x0809, 0x0809, 0x0809, 0x0809, 0x524f, 0xe71d, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffbe, 0xb410, 0x6883,
  0xcd96, 0xef1c, 0xef1c, 0xef1c, 0xe659, 0xaa29, 0xaa09, 0xee9a, 0xf71c, 0xc3ae, 0xa925, 0xdd55, 0xffdf, 0xee59, 0xca08, 0xd32d,
  0xff9e, 0xffdf, 0xe38e, 0xd9e8, 0xedd7, 0xecf3, 0xd925, 0xec92, 0xee59, 0xd1e8, 0xd2cc, 0xca29, 0xc9a7, 0xdcb3, 0xe5f8, 0xcc10,
  0xa905, 0xdd14, 0xffff, 0xeefb, 0x99c8, 0xa28b, 0xef3c, 0xffff, 0xffff, 0xffff, 0x8430, 0x632c, 0xf7be, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xd6ba, 0x3186, 0xd6ba, 0xbdd9, 0x0809, 0x0809, 0x0809, 0x0809, 0x104a, 0xa4f7, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xef3c, 0xac10, 0xb472, 0xffdf, 0xffff, 0xffff, 0xffff, 0xddf8, 0xb38e, 0xde18, 0xffdf, 0xf77d, 0xcbd0,
  0xd4d4, 0xf77d, 0xffbe, 0xe596, 0xd3af, 0xf6db, 0xffdf, 0xf71c, 0xe410, 0xed55, 0xff9e, 0xf73c, 0xe410, 0xe4f4, 0xed76, 0xe3f0,
  0xedf8, 0xdc10, 0xd38e, 0xd38e, 0xcbaf, 0xcbaf, 0xdd35, 0xf79e, 0xffdf, 0xd5b7, 0xbb8f, 0xde38, 0xffdf, 0xffdf, 0xffdf, 0xf7be,
  0x39e7, 0xad75, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x94b2, 0x5aca, 0xef7e, 0x6b31, 0x0809, 0x0809, 0x0809,
  0x0809, 0x0809, 0x292c, 0xf79e, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xef1c,
  0xbc51, 0xb3f0, 0xbbf0, 0xbbf1, 0xc410, 0xccd4, 0xf73c, 0xdd35, 0xd431, 0xd431, 0xd431, 0xdc31, 0xe514, 0xf71c, 0xed35, 0xe451,
  0xe451, 0xec51, 0xec52, 0xec72, 0xf6da, 0xecd3, 0xed35, 0xee79, 0xdc31, 0xe514, 0xff9e, 0xf75d, 0xd514, 0xd4d4, 0xeedb, 0xccb3,
  0xbc11, 0xbc10, 0xb3f0, 0xb3f0, 0xbcd3, 0xde9a, 0x3186, 0xc638, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xe73c, 0x4a49,
  0xad75, 0xce5a, 0x398d, 0x0809, 0x0809, 0x0809, 0x0809, 0x0809, 0x0809, 0xb598, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xbc52, 0x78c5, 0xb411, 0xd555, 0xbbf0, 0x9105, 0xb2ec, 0xd4d3, 0xa926, 0xcc31, 0xdd76,
  0xd492, 0xb925, 0xd34d, 0xe4b3, 0xd1c8, 0xe3f0, 0xed96, 0xedb6, 0xedb6, 0xedd7, 0xed34, 0xd166, 0xe4b3, 0xdb2d, 0xc0e4, 0xc166,
  0xeeba, 0xe596, 0xa946, 0xcc10, 0xc3af, 0x9967, 0xccb3, 0xd576, 0xcd75, 0xcd55, 0xd618, 0xe71c, 0x2966, 0xad55, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xf7be, 0x8430, 0x52aa, 0xf79e, 0xef5d, 0xce5a, 0xad37, 0x7bb3, 0x4a0e, 0x186a, 0x290b, 0x7bb3, 0xe71d,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xe6ba, 0x70c4, 0x8146, 0x9209, 0x9a29, 0xa209,
  0xa229, 0xd576, 0xa9a7, 0xa966, 0xc30d, 0xc30c, 0xb9e8, 0xc1a7, 0xdcd3, 0xd229, 0xd2ec, 0xff9e, 0xffff, 0xffff, 0xffff, 0xff3c,
  0xda29, 0xe2ec, 0xe4b2, 0xd187, 0xcaab, 0xc166, 0xcb4d, 0xba29, 0xba6a, 0xd514, 0x98e5, 0xc431, 0xc4b3, 0xa2ab, 0x9209, 0xa32d,
  0xffbe, 0xffdf, 0x5acb, 0x4a69, 0xe71c, 0xffff, 0xffff, 0xffff, 0xe71c, 0x7bcf, 0x4a69, 0xdedb, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffdf, 0xffbf, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xf79e,
  0xa34d, 0x7926, 0xc514, 0xa30d, 0x88e4, 0xbc31, 0xeefc, 0xbb8e, 0xa167, 0xd4f4, 0xe618, 0xd431, 0xb925, 0xd3af, 0xdc10, 0xc925,
  0xe4d3, 0xf69a, 0xf6ba, 0xf6bb, 0xf75d, 0xec72, 0xd9a7, 0xe4b3, 0xdacb, 0xd24a, 0xee79, 0xcaec, 0xb905, 0xb146, 0xd4b3, 0xb26a,
  0xa187, 0xd535, 0xcd14, 0xa2cc, 0x7883, 0xc4f4, 0xffff, 0xffff, 0xc618, 0x2124, 0x528a, 0x9cd3, 0xa534, 0x8c51, 0x528a, 0x630c,
  0xce79, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xd618, 0x8209, 0xac10, 0xffbe, 0xde39, 0x91a7, 0xc452, 0xe69a, 0xa209, 0xc3f0, 0xf79e,
  0xf77e, 0xcbf0, 0xba4a, 0xee59, 0xd34d, 0xc9c8, 0xd2cb, 0xdaec, 0xdaec, 0xe38e, 0xedf8, 0xdb0d, 0xe38e, 0xe535, 0xd229, 0xed76,
  0xffdf, 0xdd14, 0xb9e8, 0xcb6e, 0xe5f8, 0xb229, 0xa1c8, 0xaa8a, 0xa28b, 0x91e8, 0xa30c, 0xef3c, 0xffff, 0xffff, 0xffdf, 0xb596,
  0x4a69, 0x18e3, 0x2124, 0x4a69, 0x9cf3, 0xe73c, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff
};

// create display and DS objects
//note modified lib to pass in the screen size, that way I can use the lib
// for 3.5" display
ILI9341_t3 Display = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST);

// create the touch screen object
XPT2046_Touchscreen Touch(T_CSP, T_IRQ);

TS_Point p;

LittleFS_SPIFlash SSD;


FlickerFreePrint<ILI9341_t3> fAmpOffset(&Display, C_WHITE, C_BLACK);
FlickerFreePrint<ILI9341_t3> fAmpSlope(&Display, C_WHITE, C_BLACK);
FlickerFreePrint<ILI9341_t3> fVoltOffset(&Display, C_WHITE, C_BLACK);
FlickerFreePrint<ILI9341_t3> fVoltSlope(&Display, C_WHITE, C_BLACK);
FlickerFreePrint<ILI9341_t3> fTime(&Display, C_WHITE, C_BLACK);
FlickerFreePrint<ILI9341_t3> fVolts(&Display, C_WHITE, C_BLACK);
FlickerFreePrint<ILI9341_t3> fAmps(&Display, C_WHITE, C_BLACK);
FlickerFreePrint<ILI9341_t3> fPower(&Display, C_WHITE, C_BLACK);
FlickerFreePrint<ILI9341_t3> fEnergy(&Display, C_WHITE, C_BLACK);
FlickerFreePrint<ILI9341_t3> hTemp(&Display, C_WHITE, C_BLACK);


// stuff for SD card
SdFat SDCard;
SdFile SDDataFile;

// stuff for SPI chip
File DataFile;
File BMPFile;

Button StartBtn(&Display);
Button CalibrateBtn(&Display);
Button SetTimeBtn(&Display);
Button DownloadBtn(&Display);

// create the button objects
Button KeyPadBtn[12](&Display);
Button ProfileBtn(&Display);
Button BatteryBtn(&Display);
Button DoneBtn(&Display);

Button OnBtn(&Display);
Button OffBtn(&Display);
Button StopBtn(&Display);
Button PostRaceBtn(&Display);

Button SSDEraseBtn(&Display);
Button SSDDownloadBtn(&Display);
Button SSDDoneBtn(&Display);
Button SetTesterBtn(&Display);

SliderH AmpO(&Display);
SliderH AmpS(&Display);
SliderH VoltO(&Display);
SliderH VoltS(&Display);

SliderH RTCM(&Display);
SliderH RTCD(&Display);
SliderH RTCY(&Display);
SliderH RTCH(&Display);
SliderH RTCI(&Display);

CheckBox TestTime(&Display);

OptionButton BatteryTemp(&Display);

CGraph ProfileGraph(&Display, GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, 0, 100, 10, 0, 350, 50);

time_t RTCTime;

elapsedMillis ElapsedComputeUpdateTime;
elapsedMillis ElapsedSDWriteTime;
elapsedMillis ElapsedCurrentTime;

/*

  main setup function

*/

void setup() {
  Serial.begin(9600);

  pinMode(LCD_PIN, OUTPUT);
  pinMode(CNT_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);

  digitalWrite(LCD_PIN , LOW);

  PowerUpMOSFET(MOSFET_OFF);
  delay(10);

  // create buttons
  ProfileBtn.init (  60, 140, 100, 180, C_GREY, C_BLACK, C_WHITE, C_WHITE,  "Profile", 0, 0, FONT_HEADING);
  DownloadBtn.init(210, 80, 180, 60, C_GREY, C_DKGREY, C_WHITE, C_WHITE,  "Download", 0, 0, FONT_HEADING);
  CalibrateBtn.init(210, 140, 180, 60, C_GREY, C_DKGREY, C_WHITE, C_WHITE,  "Calibrate", 0, 0, FONT_HEADING);
  SetTimeBtn.init(  210, 200, 180, 60, C_GREY, C_DKGREY, C_WHITE, C_WHITE,  "Set Time", 0, 0, FONT_HEADING);

  BatteryBtn.init(160, 60, 100, 36, C_DKCYAN, C_LTRED, C_BLACK, C_WHITE,  "Battery", 0, 0, FONT_HEADING);

  StartBtn.init( 260, 200, 80, 40, C_DKGREEN, C_GREEN, C_BLACK, C_WHITE,  "Start", 0, 0, FONT_HEADING);
  DoneBtn.init(  265, 215, 90, 33, C_GREY, C_DKGREY, C_WHITE, C_WHITE,  "Save", 0, 0, FONT_HEADING);
  OnBtn.init(    265, 30, 90, 33, C_DKGREEN, C_GREEN, C_BLACK, C_WHITE,  "ON", 0, 0, FONT_HEADING);
  OffBtn.init(  265, 30, 90, 33, C_DKRED, C_RED, C_DKGREY, C_BLACK,  "OFF", 0, 0, FONT_HEADING);
  StopBtn.init(   280, 15, 60, 29, C_DKRED, C_RED, C_WHITE, C_WHITE,  "STOP", 0, 0, Arial_12_Bold);

  SSDEraseBtn.init(   95,  70, 180, 40, C_GREY, C_DKGREY, C_WHITE, C_WHITE,  "Erase SSD", 0, 0, FONT_HEADING);
  SSDDownloadBtn.init(95, 110, 180, 40, C_GREY, C_DKGREY, C_WHITE, C_WHITE,  "Download", 0, 0, FONT_HEADING);
  SetTesterBtn.init(  95, 150, 180, 40, C_GREY, C_DKGREY, C_WHITE, C_WHITE,  "Tester #", 0, 0, FONT_HEADING);
  SSDDoneBtn.init(    265, 20, 90, 33, C_GREY, C_GREEN, C_BLACK, C_WHITE,  "Done", 0, 0, FONT_HEADING);

  // we may disable this button, so set the disable colors
  SSDDoneBtn.setColors(C_GREY, C_GREEN, C_BLACK, C_WHITE, C_BLACK, C_GREY);

  // create KeyPadBtn
  for (row = 0; row < 4; row++) {
    for (col = 0; col < 3; col++) {
      KeyPadBtn[col + row * 3].init(BUTTON_X + col * (BUTTON_W + BUTTON_SPACING_X),
                                    BUTTON_Y + row * (BUTTON_H + BUTTON_SPACING_Y), BUTTON_W, BUTTON_H,
                                    C_WHITE, KeyPadBtnColor[col + row * 3], C_WHITE, C_BLACK,
                                    KeyPadBtnText[col + row * 3], 0, 0, Arial_12);
    }
  }

  ProfileGraph.init("", "Time [min]", "Power[W], Energy[Wh]", C_BLACK, C_DKGREY, C_BLACK, C_WHITE, C_WHITE, Arial_16_Bold, Arial_10);

  PowerID = ProfileGraph.add("Power", C_BLUE);
  EnergyID = ProfileGraph.add("Energy", C_RED);
  bPowerID = ProfileGraph.add("Power", C_BLUE);
  bEnergyID = ProfileGraph.add("Energy", C_RED);

  ProfileBtn.setCornerRadius(C_RADIUS);
  DownloadBtn.setCornerRadius(C_RADIUS);
  CalibrateBtn.setCornerRadius(C_RADIUS);
  SetTimeBtn.setCornerRadius(C_RADIUS);

  BatteryBtn.setCornerRadius(C_RADIUS);

  StartBtn.setCornerRadius(C_RADIUS);
  DoneBtn.setCornerRadius(C_RADIUS);
  OnBtn.setCornerRadius(C_RADIUS);
  OffBtn.setCornerRadius(C_RADIUS);
  StopBtn.setCornerRadius(C_RADIUS);

  SSDEraseBtn.setCornerRadius(C_RADIUS);
  SSDDownloadBtn.setCornerRadius(C_RADIUS);
  SetTesterBtn.setCornerRadius(C_RADIUS);
  SSDDoneBtn.setCornerRadius(C_RADIUS);

  AmpO.init(170,  65, 120,  0.4, 0.6, 0.0, 0.0, C_BLACK, C_WHITE, HANDLE_COLOR    );
  AmpS.init(170, 100, 120, 70.0, 90.0, 0.0, 0.0, C_BLACK, C_WHITE, HANDLE_COLOR   );
  VoltO.init(170, 135, 120, .7, .9, 0.0, 0.0, C_BLACK, C_WHITE, HANDLE_COLOR    );
  VoltS.init(170, 170, 120, 5.0, 7.0, 0.0, 0.0, C_BLACK, C_WHITE, HANDLE_COLOR    );

  AmpO.setHandleSize(20);
  AmpS.setHandleSize(20);
  VoltO.setHandleSize(20);
  VoltS.setHandleSize(20);

  RTCM.init(95, 40, 205, 1, 12, 0, 1, C_BLACK, C_WHITE, HANDLE_COLOR);
  RTCD.init(95, 75, 205, 1, 31, 0, 1, C_BLACK, C_WHITE, HANDLE_COLOR);
  RTCY.init(95, 110, 205, 2021, 2030, 0, 1, C_BLACK, C_WHITE, HANDLE_COLOR);
  RTCH.init(95, 145, 205, 0, 23, 0, 1, C_BLACK, C_WHITE, HANDLE_COLOR);
  RTCI.init(95, 180, 205, 0, 59, 0, 1, C_BLACK, C_WHITE, HANDLE_COLOR);

  RTCY.setHandleSize(20);
  RTCM.setHandleSize(20);
  RTCD.setHandleSize(20);
  RTCH.setHandleSize(20);
  RTCI.setHandleSize(20);

  TestTime.init(10, 130, C_BLACK, HANDLE_COLOR, C_LTGREY, C_BLACK, C_WHITE, 30, 2, "Pre-race test", FONT_SMALL);
  TestTime.value = true;

  BatteryTemp.init(C_BLACK, HANDLE_COLOR , C_LTGREY , C_BLACK, C_WHITE, 20, -2, FONT_SMALL);

  // fire up the display
  Display.begin();
  delay(100);

  // fire up the touch display
  Touch.begin();

  Touch.setRotation(1);
  Display.setRotation(1);
  delay(100);
  Display.invertDisplay(false);

  Display.fillScreen(C_WHITE);
  // get the calibration parameters from the EEPROM
  GetParameters();

  sprintf(buf, "Tester: %d", Tester);
  SetTesterBtn.setText(buf);


  // set the Time library to use Teensy 3.0's RTC to keep time
  setSyncProvider(getTeensy3Time);
  RTCTime = processSyncMessage();

  if (RTCTime != 0) {
    Teensy3Clock.set(RTCTime); // set the RTC
    setTime(RTCTime);
  }

  if (TotalTests > TOTAL_TESTS) {

    digitalWrite(LCD_PIN , HIGH);
    // we are out of ssd memory
    // user needs to download data and erase chip
    DownloadData(true);
  }

  // setup analog read resolutions
  analogReadRes(12);
  analogReadAveraging(20);

  SplashScreen();

#ifdef TEST_SCREEN
  Display.fillScreen(C_BLUE);
  Display.setFont(FONT_HEADING);
  Display.setTextColor(C_YELLOW);
  Display.setCursor(100, 100);
  Display.print(F("Touch press test"));
  while (1) {
    if (Touch.touched()) {
      ProcessTouch();
    }
  }
#endif

  analogWriteFrequency(BUZZ_PIN, 500);

  for (i = 0; i < 2; i ++) {
    analogWrite(BUZZ_PIN, 100);
    delay(100);
    analogWrite(BUZZ_PIN, 0);
    delay(100);
  }



  Display.setFont(FONT_HEADING);
  Display.setTextColor(C_BLACK);
  Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
  Display.fillRoundRect(20, 200, 10, 30, 4, C_GREY);
  Display.setCursor(25, 207);
  Display.print(F("Memory: ")); Display.print(100.0 - ((float)TotalTests / TOTAL_TESTS) * 100.0, 0); Display.print(F("%"));

  delay(500);

  Display.setFont(FONT_HEADING);
  Display.setTextColor(C_BLACK);
  Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
  Display.fillRoundRect(20, 200, 60, 30, 4, C_GREY);
  Display.setCursor(25, 207);
  TesterTemp = GetTemp(HTRTEMP_PIN);
  sprintf(str, "Ambient (%0.0f", TesterTemp);
  strcat(str, " deg F)");

  Display.print(str);

  BatTempID0 = BatteryTemp.add(20, 165, str);
  BatTempID1 = BatteryTemp.add(20, 193, "77 deg F");
  BatTempID2 = BatteryTemp.add(20, 219, "85 deg F");

  BatteryTemp.select(BatTempID1);
  delay(1000);
  Display.setFont(FONT_HEADING);
  Display.setTextColor(C_BLACK);
  Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
  Display.fillRoundRect(20, 200, 80, 30, 4, C_GREY);
  Display.setCursor(25, 207);

  TesterTemp = GetTemp(HTRTEMP_PIN);
  memset(str, ' ', sizeof(str));
  sprintf(str, "Load (%0.0f", TesterTemp);
  strcat(str, " deg F)");

  Display.print(str);
  delay(100);
  ProfileGraph.setLineThickness(PowerID, 4);
  ProfileGraph.setLineThickness(EnergyID, 4);
  ProfileGraph.setLineThickness(bPowerID, 2);
  ProfileGraph.setLineThickness(bEnergyID, 2);
  ProfileGraph.setXTextOffset(5);
  ProfileGraph.setYTextOffset(28);
  ProfileGraph.showLegend(false);

  Display.setFont(FONT_HEADING);
  Display.setTextColor(C_BLACK);
  Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
  Display.fillRoundRect(20, 200, 80, 30, 4, C_GREY);
  Display.setCursor(25, 207);
  Display.print(F("Starting SSD"));
  IsSD = SSD.begin(SSD_PIN);
  delay(100);

  if (IsSD) {
    Display.setFont(FONT_HEADING);
    Display.setTextColor(C_BLACK);
    Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
    Display.fillRoundRect(20, 200, 100, 30, 4, C_GREY);
    Display.setCursor(25, 207);
    Display.print(F("SSD found"));
  }
  else {
    Display.setFont(FONT_HEADING);
    Display.setTextColor(C_BLACK);
    Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
    Display.fillRoundRect(20, 200, 100, 30, 4, C_GREY);
    Display.setCursor(25, 207);
    Display.print(F("Starting SSD "));

    while (!IsSD) {
      IsSD = SSD.begin(SSD_PIN);
      Display.print(F("."));
      delay(500);
    }
  }

  // let users see splash screen
  delay(1000);
  Display.setFont(FONT_HEADING);
  Display.setTextColor(C_BLACK);
  Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
  Display.fillRoundRect(20, 200, 280, 30, 4, C_GREY);
  Display.setCursor(25, 207);
  Display.print(F("Initilization complete"));

  delay(100);
  // menu to prompt for calibration or battery profile test
  GetTestType();

  // reset everything after a potential calibration, note calibration calls compute()
  VVolts = 0.0;
  AVolts = 0.0;
  Counter = 0;
  Power = 0.0;
  Energy = 0.0;
  Display.fillScreen(C_WHITE);

  GetFileName();

  Display.fillScreen(C_WHITE);
  // if you got this far profile test desired
  // write SD and get test ready
  WriteHeader();
  Display.fillScreen(C_WHITE);
  DrawBaseLineGraph();
  DrawDataHeader();
  ElapsedCurrentTime = 0;

  DrawData();

  // don't power up the MOSFET's yet, let the test run for 10 sec to let the unloaded voltage get captured
  // make sure they are off
  PowerUpMOSFET(MOSFET_OFF);

  ElapsedComputeUpdateTime = 0;
  ElapsedSDWriteTime = 0;
  ElapsedCurrentTime = 0;

}

/*

  main loop function

*/

void loop() {

  // Counter used for averaging
  Counter++;
  AVolts = AVolts + analogRead(AM_PIN);
  VVolts = VVolts + analogRead(VM_PIN);

  if (Touch.touched()) {

    ProcessTouch();
    /*
        if ((BtnX < 50) & (BtnY < 50)) {
          mVPerAmp = mVPerAmp + 0.1;
          Display.setFont(Arial_10);
          Display.setCursor(10, 50);
          Display.fillRect(10, 50, 50, 20, C_WHITE);
          Display.print(mVPerAmp, 3);
        }

        if ((BtnX < 50) & (BtnY > 150)) {
          mVPerAmp = mVPerAmp - 0.1;
          Display.setFont(Arial_10);
          Display.setCursor(10, 50);
          Display.fillRect(10, 50, 50, 20, C_WHITE);
          Display.print(mVPerAmp, 3);
        }
    */
    if (PressIt(StopBtn) == true) {

      PowerUpMOSFET(MOSFET_OFF);
      RunTest = false;
      TestHasStarted = false;

      SaveBMP24(&Display, SD_CS , BMPName);

      // save the total test to the SSD chip
      TotalTests++;
      EEPROM.put(40, TotalTests);

      StopBtn.hide();
      ShowTestDone();

    }
  }

  if (ElapsedComputeUpdateTime > ComputeUpdate) {
    ElapsedComputeUpdateTime = 0;
    ComputeData();
    TesterTemp = GetTemp(HTRTEMP_PIN);
    DrawData();

#ifdef debug
    Debug();
#endif
    // should we continue to run the test
    RunTheTest();
    // set the old variables as these are used for display when test is complete
    VVolts = 0.0;
    AVolts = 0.0;
    Counter = 0;

  }

  if (ElapsedSDWriteTime >= SDUpdateTime) {
    ElapsedSDWriteTime = 0;
    // increment point counter
    Point++;
    // plot the data
    // Graph(Point / 12, Power, Energy, 50, 220, 210, 180, 0, 100, 10 , 0, 350, 50, true, display1);
    ProfileGraph.setX(Point / 12);
    ProfileGraph.plot(PowerID, Power);
    ProfileGraph.plot(EnergyID, Energy);
    // write the data to the SD card
    WriteData();
  }

}


/*

  main compute function

*/
void ComputeData() {

  // get the measured volt averages
  VVolts = VVolts / Counter;
  VVolts =  (VVolts * 3.3f) / 4096.0f;

  // now compentate for electrical tolerances (user can tweak these values)
  // good old y = mx + b
  Volts = (VVolts * VoltSlope) + VoltOffset;

  // now get Amps
  AVolts = AVolts / Counter;
  AVolts = AVolts / (4096.0f / (3.315f));
  Amps = (AVolts  - AmpOffset) / (mVPerAmp / 1000.0);

  // compute the power
  Power = Volts * Amps;

  // compute the energy (dynamic integration)
  Energy = Energy + (Power * (ComputeUpdate / 3600000.0f));

}

/*

  determine if the test should be run

*/

void RunTheTest() {

  // check for the cutoff limit
  if (Volts <= BatteryCutOff) {
    // if Volts drop then shut the test off and keep it off
    // or if we are in recover mode and time exceeds duration
    PowerUpMOSFET(MOSFET_OFF);
    TestHasStarted = false;
    RunTest = false;
    StopBtn.hide();

#ifdef TESTER_FOR_PATRIOT_RACING
    draw565Bitmap(245, 3, LOGO, 70, 30);
#endif

    SaveBMP24(BMPName);

    // save the total test to the SSD chip
    TotalTests++;
    EEPROM.put(40, TotalTests);

    ShowTestDone();

  }

  // check for the tester temp is > OVER_TEMP_LIMIT (some limit i dreamed up)
  if (TesterTemp > OVER_TEMP_LIMIT) {

    PowerUpMOSFET(MOSFET_OFF);
    TestHasStarted = false;
    RunTest = false;
    StopBtn.hide();
    ShowOverTemp();

  }


  if (ElapsedCurrentTime > 1000) {
    TestHasStarted = true;
    PowerUpMOSFET(MOSFET_ON);
  }

}

/*

  pop flashing done text

*/
void ShowTestDone() {

  Display.fillRect(0, 0, 240 , 27, C_WHITE);
  Display.fillRect(47, 27, 193 , 5, C_WHITE);

  analogWriteFrequency(BUZZ_PIN, 1000);

  // flash text every second
  while (true) {

    // update the tester temp
    TesterTemp = GetTemp(HTRTEMP_PIN);

    Display.setCursor(260, 225);
    Display.setFont(Arial_10);
    hTemp.setTextColor(C_BLACK, C_WHITE);
    hTemp.print(TesterTemp, 1);

    analogWrite(BUZZ_PIN, 50);
    Display.setCursor(5 , 9 );
    Display.setFont(Arial_16_Bold);
    Display.setTextColor(C_RED, C_WHITE);
    Display.print(F("DONE"));
    delay(500);

    analogWrite(BUZZ_PIN, 0);
    Display.setCursor(5 , 9 );
    Display.setFont(Arial_16_Bold);
    Display.setTextColor(C_BLACK, C_WHITE);
    Display.print(F("DONE"));
    delay(500);

  }

}

void ShowOverTemp() {

  Display.fillScreen(C_WHITE);
  analogWriteFrequency(BUZZ_PIN, 1000);

  // flash text every second
  while (true) {

    // update the tester temp
    TesterTemp = GetTemp(HTRTEMP_PIN);

    Display.setTextColor(C_RED, C_WHITE);

    Display.setFont(FONT_HEADER);
    Display.setCursor(20 , 40);
    Display.print(F("Test FAILED"));
    Display.setFont(FONT_HEADING);
    Display.setCursor(20 , 90);
    Display.print(F("Excessive Temperature!"));
    Display.setCursor(20 , 130);
    Display.print(F("Check the cooling fan."));
    Display.setCursor(20 , 170);
    hTemp.setTextColor(C_RED, C_WHITE);
    hTemp.print(TesterTemp, 1);
    analogWrite(BUZZ_PIN, 50);
    delay(500);


    Display.setTextColor(C_WHITE, C_WHITE);
    Display.setFont(FONT_HEADER);
    Display.setCursor(20 , 40);
    Display.print(F("Test FAILED"));
    Display.setFont(FONT_HEADING);
    Display.setCursor(20 , 90);
    Display.print(F("Excessive Temperature!"));
    Display.setCursor(20 , 130);
    Display.print(F("Check the cooling fan."));
    Display.setCursor(20 , 170);
    hTemp.setTextColor(C_WHITE, C_WHITE);
    hTemp.print(TesterTemp, 1);
    analogWrite(BUZZ_PIN, 0);
    delay(500);

  }

}
/*

  write data to SD card

*/
void WriteData() {

  DataFile = SSD.open(FileName, FILE_WRITE);

  // write the data to the SD card
  if (DataFile) {
    DataFile.print(ElapsedCurrentTime / 1000);
    DataFile.print(F(", "));
    DataFile.print(ElapsedCurrentTime / 60000.0);
    DataFile.print(F(", "));
    DataFile.print(Volts);
    DataFile.print(F(", "));
    DataFile.print(Amps);
    DataFile.print(F(", "));
    DataFile.print(Power);
    DataFile.print(F(", "));
    DataFile.println(Energy);

    delay(10);
    DataFile.close();
    delay(40);

  }
  else {
    // we could warn the user, but just ignore the record
    // and get it next time
  }
}


/*

  calibrate or profile function

*/

void GetTestType() {

  // UI to draw screen and capture input
  KeepIn = true;

  DrawTestTypeScreen();

  while (KeepIn) {
    delay(50);


    // if touch screen pressed handle it
    if (Touch.touched()) {

      ProcessTouch();

      if (PressIt(ProfileBtn) == true) {
        KeepIn = false;
      }

      if (PressIt(DownloadBtn) == true) {
        DownloadData(false);
        DrawTestTypeScreen();
      }

      if (PressIt(CalibrateBtn) == true) {
        Calibrate();
        DrawTestTypeScreen();
      }

      if (PressIt(SetTimeBtn) == true) {
        SetTime();
        DrawTestTypeScreen();
      }

    }
  }
}

/*

  service function UI screen

*/
void DrawTestTypeScreen() {

  //nothing fancy, just a header and some buttons
  Display.fillScreen(C_WHITE);

  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setFont(Arial_16_Bold);
  Display.setCursor(10 , 10 );
  Display.fillRect(0 , 0, 320, 40, C_LTGREY );
  Display.print(TEAM_NAME);
  Display.print(F(" Battery Tester"));

  Display.setFont(Arial_16);
  Display.setTextColor(C_BLACK, C_WHITE);

  ProfileBtn.draw();
  DownloadBtn.draw();
  CalibrateBtn.draw();
  SetTimeBtn.draw();

}

/*

  write header to SD card

*/

void WriteHeader() {

  // write success to screen and write header
  char str[16];

  Display.fillScreen(C_WHITE);

  Display.fillRect(0 , 0, 320, 40, C_LTGREY );
  Display.setTextColor(C_BLACK, C_LTGREY);
  Display.setFont(Arial_16_Bold);
  Display.setCursor(10 , 10 );
  Display.print(F("Preparing Profile Test"));

  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setFont(FONT_SMALL_BOLD);

  Display.setCursor(10, 60);
  Display.print(F("File"));
  Display.setFont(FONT_SMALL);
  Display.setCursor(105, 60);
  for (i = 0; i < 18; i++) {
    Display.print(FileName[i]);
  }


  Display.setCursor(10, 85);
  Display.setFont(FONT_SMALL_BOLD);
  Display.print(F("SD Card"));
  Display.setFont(FONT_SMALL);
  if (IsSD) {
    Display.setCursor(105, 85);
    Display.print(F("FOUND"));
  }
  else {
    Display.setCursor(105, 85);
    Display.print(F("NOT FOUND"));
  }
  Display.setFont(FONT_SMALL_BOLD);
  Display.setCursor(10, 110);
  Display.print(F("Header"));


  // now that we have a battery and filename
  // write to the SD card a header
  sprintf(str, "%02d", Battery);
  // Serial.println(str);
  Display.setFont(FONT_SMALL);
  DataFile = SSD.open(FileName, FILE_WRITE);

  if (DataFile) {

    // write date info to file
    //DataFile.timestamp(T_ACCESS, year(), month(), day(), hour(), minute(), second());
    //DataFile.timestamp(T_CREATE, year(), month(), day(), hour(), minute(), second());
    //DataFile.timestamp(T_WRITE, year(), month(), day(), hour(), minute(), second());

    // write header data
    DataFile.print("Time [s], Time [m], ");
    DataFile.print(str);
    DataFile.print(" - Volts [V], ");
    DataFile.print(str);
    DataFile.print(" - Amps [A], ");
    DataFile.print(str);
    DataFile.print("- Power [W], ");
    DataFile.print(str);
    DataFile.print(" - Energy [Whr]");
    DataFile.println("");

    delay(10);
    DataFile.close();
    delay(40);

    Display.setCursor(105, 110);
    Display.print(F("WRITTEN"));
  }
  else {
    Display.setCursor(105, 110);
    Display.print(F("NOT WRITTEN"));
  }

  RunTest = true;
  Display.setFont(FONT_SMALL_BOLD);
  Display.setCursor(10, 135);
  Display.print(F("Sensors"));
  Display.setCursor(105, 135);
  Display.setFont(FONT_SMALL);
  Display.print(F("READING"));

  TesterTemp = GetTemp(HTRTEMP_PIN);

  Display.setFont(FONT_SMALL_BOLD);
  Display.setCursor(10, 160);
  Display.print(F("Load"));
  Display.setFont(FONT_SMALL);
  Display.setCursor(105, 160);
  Display.print(TesterTemp, 1);

  // reset averaging Counters
  VVolts = 0.0;
  AVolts = 0.0;
  Counter = 0;
  delay(1000);

}

/*

  service function to get the filename

*/
void GetFileName() {

  bool FNKeepIn = true;

  DrawFileNameScreen();

  while (FNKeepIn) {

    if (Touch.touched()) {

      ProcessTouch();

      TestTime.press(BtnX, BtnY);

#ifdef TESTER_FOR_PATRIOT_RACING

      BatTemp = BatteryTemp.press(BtnX, BtnY);
#endif

      if (PressIt(BatteryBtn) == true) {
        KeyPad(Battery, 1, 99);

        DrawFileNameScreen();
      }

      if (PressIt(StartBtn) == true) {
        FNKeepIn = false;
      }
    }
  }

  EEPROM.put(0, Battery);


}

/*

  service function to write the filename screen

*/

void DrawFileNameScreen() {

  byte FileCount = 0;

  Display.fillScreen(C_WHITE);
  Display.setTextColor(C_BLACK, C_WHITE);

  sprintf(FileName, "%02d_%04d_%02d_%02d_%1d_%02d.csv",
          Battery,
          year(),
          month(),
          day(),
          Tester,
          FileCount);

  while (SSD.exists(FileName)) {
    FileCount++;

    FileName[16] = (int) ((FileCount / 10) % 10) + '0';
    FileName[17] = (int) (FileCount  % 10) + '0';

    if (FileCount > 99) {
      break;
    }
  }

  sprintf(BMPName, "%02d_%04d_%02d_%02d_%1d_%02d.bmp",
          Battery,
          year(),
          month(),
          day(),
          Tester,
          FileCount);

  Display.fillRect(55, 5, 30, 30, C_LTRED);
  Display.drawRect(55, 5, 30, 30, C_BLACK);

  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setFont(Arial_16_Bold);
  Display.setCursor(10 , 10 );
  Display.println(F("File:"));

  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setFont(Arial_16);
  Display.setCursor(60 , 10);
  Display.print(FileName);

  // for battery
  Display.drawLine(70, 60, 70, 35, C_BLACK);
  Display.drawLine(70, 60, 120, 60, C_BLACK);

  Display.setFont(Arial_12_Bold);
  BatteryBtn.draw();
  StartBtn.draw();


  TestTime.draw(TestTime.value);
#ifdef TESTER_FOR_PATRIOT_RACING
  BatteryTemp.draw(BatteryTemp.value); // always test at 77 deg f
#endif


}

/*

  function to calibrate tester

*/

void Calibrate() {

  bool CalKeepIn = true;
  bool Test = false;


  AmpO.resetScale(AmpOffset - 0.1, AmpOffset + 0.1, 0.0, 0.01);
  AmpS.resetScale(mVPerAmp - 2.0, mVPerAmp + 2.0, 0.0, 0.1);
  VoltO.resetScale(VoltOffset - 0.01, VoltOffset + 0.01, 0.0, 0.001);
  VoltS.resetScale(VoltSlope - 0.1, VoltSlope + 0.1, 0.0, 0.01);

  Display.fillScreen(C_WHITE);

  DrawCalibrateScreen(false);

  AmpO.draw(AmpOffset);
  AmpS.draw(mVPerAmp);
  VoltO.draw(VoltOffset);
  VoltS.draw(VoltSlope);


  DoneBtn.draw();
  OnBtn.draw();

  while (CalKeepIn) {

    if (Touch.touched()) {

      ProcessTouch();

      AmpO.slide(BtnX, BtnY);
      AmpS.slide(BtnX, BtnY);
      VoltO.slide(BtnX, BtnY);
      VoltS.slide(BtnX, BtnY);

      AmpOffset =  AmpO.value;
      mVPerAmp = AmpS.value;
      VoltOffset = VoltO.value;
      VoltSlope = VoltS.value;

      if (PressIt(OnBtn)) {
        DoneBtn.disable();
        DoneBtn.draw();
        OnBtn.hide();
        OffBtn.show();
        OffBtn.draw();
        PowerUpMOSFET(MOSFET_ON);
        delay(1000);
      }
      else if (PressIt(OffBtn)) {
        DoneBtn.enable();
        DoneBtn.draw();
        PowerUpMOSFET(MOSFET_OFF);
        OffBtn.hide();
        OnBtn.show();
        OnBtn.draw();
        delay(1000);
      }

      else if (PressIt(DoneBtn) == true) {
        PowerUpMOSFET(MOSFET_OFF);
        CalKeepIn = false;

      }

      DrawCalibrateScreen(Test);

    }
    // get current and Volts and display

    AVolts = 0;
    VVolts = 0;
    Counter = 0;

    for (Counter = 0; Counter < 3000; Counter++) {
      AVolts = AVolts + analogRead(AM_PIN);
      VVolts = VVolts + analogRead(VM_PIN);
    }

    ComputeData();

    if (PressIt(DoneBtn) == true) {
      PowerUpMOSFET(MOSFET_OFF);
      CalKeepIn = false;
    }


    // Display.fillRect(110, 195, 120 , 44, C_BLACK);
    Display.fillRect(5, 195, 180 , 44, C_GREY);
    Display.setFont(Arial_12_Bold);
    Display.setTextColor(C_BLACK);


    Display.setCursor(10 , 200);
    Display.print(F("Amps: "));
    Display.setCursor(70 , 200);
    Display.print(Amps, 2);

    Display.setCursor(135 , 200);
    Display.print(AVolts, 2);

    Display.setCursor(10 , 220);
    Display.print(F("Volts"));
    Display.setCursor(70 , 220);
    Display.print(Volts, 2);

    Display.setCursor(135 , 220);
    Display.print(VVolts, 2);

    AVolts = 0;
    VVolts = 0;
    Counter = 0;

  }

  AmpOffset =  AmpO.value;
  mVPerAmp = AmpS.value;
  VoltOffset = VoltO.value;
  VoltSlope = VoltS.value;

  EEPROM.put(5, AmpOffset);
  EEPROM.put(10, mVPerAmp);
  EEPROM.put(15, VoltOffset);
  EEPROM.put(20, VoltSlope);

  delay(100);

}

/*

  service function to draw calibrate tester

*/

void DrawCalibrateScreen(bool TestStat) {

  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setFont(Arial_16_Bold);
  Display.setCursor(10 , 10 );
  Display.print(F("Calibrate Sensors"));

  Display.setFont(Arial_12_Bold);

  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setCursor(10 , 65);
  Display.print(F("Amp Offset"));
  Display.setCursor(110 , 65);
  fAmpOffset.setTextColor(C_BLACK, C_WHITE);
  fAmpOffset.print(AmpOffset, 2);

  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setCursor(10 , 100);
  Display.print(F("Amp Slope"));
  Display.setCursor(110 , 100);
  fAmpSlope.setTextColor(C_BLACK, C_WHITE);
  fAmpSlope.print(mVPerAmp, 1);

  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setCursor(10 , 135);
  Display.print(F("Volt Offset"));
  Display.setCursor(110 , 135);
  fVoltOffset.setTextColor(C_BLACK, C_WHITE);
  fVoltOffset.print(VoltOffset, 3);

  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setCursor(10 , 170);
  Display.print(F("Volt Slope"));
  Display.setCursor(110 , 170);
  fVoltSlope.setTextColor(C_BLACK, C_WHITE);
  fVoltSlope.print(VoltSlope, 2);

  Display.setTextColor(C_BLACK, C_WHITE);

  Display.setFont(Arial_16_Bold);


}

/*

  function to turn on / off power MOSFETS

*/

void PowerUpMOSFET(bool Power) {

  if (Power) {

    digitalWriteFast(CNT_PIN, HIGH);
  }
  else {

    digitalWriteFast(CNT_PIN, LOW);
  }

}

/*

  function to draw a cute keypad for user input

*/

void KeyPad(byte & TheNumber, byte TheMin, byte TheMax) {

  bool KPKeepIn = true;

  bool Redraw = true;

  Display.setFont(Arial_16);

  Left = BUTTON_X -  BUTTON_SPACING_X - (0.5 * BUTTON_W);
  top = BUTTON_Y - (2 * BUTTON_H);
  wide = (3 * BUTTON_W ) + (5 * BUTTON_SPACING_X);
  high = 5 * (BUTTON_H +  BUTTON_SPACING_Y);
  TempNum = TheNumber;
  KeepIn = true;

  Display.fillRect(Left, top, wide , high, C_DKGREY);
  Display.drawRect(Left, top, wide , high, C_LTGREY);

  Display.fillRect(Left + 10, top + 10, wide - 20 , 30, C_GREY);
  Display.drawRect(Left + 10, top + 10, wide - 20 , 30, C_WHITE);

  Display.setCursor(Left  + 20 , top + 20);
  Display.setTextColor(C_BLACK, C_GREY);

  Display.print(TheNumber);

  for (row = 0; row < 4; row++) {
    for (col = 0; col < 3; col++) {
      KeyPadBtn[col + row * 3].draw();
    }
  }

  while (KPKeepIn) {

    if (Touch.touched()) {

      ProcessTouch();

      // go thru all the KeyPadBtn, checking if they were pressed
      for (b = 0; b < 12; b++) {

        if (PressIt(KeyPadBtn[b]) == true) {

          if ((b < 9) | (b == 10)) {

            if (Redraw) {
              TempNum = 0;
              Redraw = false;
            }
            TempNum *= 10.0;

            if (TempNum == 0) {
              //Display.fillRect(Left + 10, top + 10, wide - 20 , 30, C_GREY);
            }

            if (b != 10) {
              TempNum += (b + 1);
            }

            if (TempNum < TheMin) {
              //Display.fillRect(Left + 10, top + 10, wide - 20 , 30, C_GREY);
              TempNum = TheMin;
              TempNum += (b + 1);
            }

            if (TempNum > TheMax) {
              //Display.fillRect(Left + 10, top + 10, wide - 20 , 30, C_GREY);
              TempNum = 0;
              TempNum += (b + 1);
            }
          }
          // cancel button
          if (b == 11) {
            TempNum = TheNumber;
            KPKeepIn = false;
            BtnX = 0;
            BtnY = 0;
            delay(100);
          }
          if (b == 9) {
            // ok button
            KPKeepIn = false;
            BtnX = 0;
            BtnY = 0;
            delay(100);
          }

          Display.setFont(Arial_16);
          Display.fillRect(Left + 10, top + 10, wide - 20 , 30, C_GREY);
          Display.drawRect(Left + 10, top + 10, wide - 20 , 30, C_WHITE);

          Display.setCursor(Left  + 20 , top + 20);
          Display.setTextColor(C_BLACK, C_GREY);
          Display.print(TempNum, 0);
        }
      }
    }
  }

  TheNumber = TempNum;

}

/*

  service function to handle keypress from keypad

*/

bool PressIt(Button TheButton) {

  if (TheButton.press(BtnX, BtnY)) {
    TheButton.draw(B_PRESSED);
    Click();
    while (Touch.touched()) {
      if (TheButton.press(BtnX, BtnY)) {
        TheButton.draw(B_PRESSED);
      }
      else {
        TheButton.draw(B_RELEASED);
        BtnX = -100;
        BtnY = -100;
        //should be false but tester 2 is jacked
        return true;
      }
      ProcessTouch();
    }

    TheButton.draw(B_RELEASED);
    BtnX = -100;
    BtnY = -100;

    return true;
  }

  return false;
}


void ProcessTouch() {

  p = Touch.getPoint();

  BtnX = p.x;
  BtnY = p.y;

#ifdef TEST_SCREEN
  Serial.print("real coordinates: ");
  Serial.print(BtnX);
  Serial.print(",");
  Serial.print (BtnY);
#endif


#ifdef DISPLAY_HEADER_YELLOW
  BtnX  = map(BtnX, 200, 3600, 0, 320);
  BtnY  = map(BtnY, 270, 3800,  0, 240);
#endif

#ifndef DISPLAY_HEADER_YELLOW
  BtnX  = map(BtnX, 3900, 300, 0, 320);
  BtnY  = map(BtnY, 3900, 200, 0, 240);
#endif

#ifdef TEST_SCREEN
  Serial.print(", Mapped coordinates: ");
  Serial.print(BtnX);
  Serial.print(",");
  Serial.println(BtnY);
  Display.fillCircle(BtnX, BtnY, 3, C_RED);
#endif

  delay(10);
}

/*

  dump data to serial monitor for debugging

*/

void Click() {
  analogWriteFrequency(BUZZ_PIN, 200);
  analogWrite(BUZZ_PIN, 127);
  delay(5);
  analogWrite(BUZZ_PIN, 0);

}

void Debug() {

#ifdef debug

  Serial.println("" );

  Serial.print(F("ElapsedCurrentTime [s]: " ));
  Serial.println(ElapsedCurrentTime / 1000);
  Serial.print("point: " );
  Serial.println(Point);

  m = (ElapsedCurrentTime / 1000 ) / 60 ;
  s = (ElapsedCurrentTime / 1000 ) % 60;
  sprintf(str, "%03d:%02d", m, s);

  Serial.print(F("duration [mm:ss]: " ));
  Serial.println(str);

  Serial.print(F("Counter: " ));
  Serial.println(Counter);

  Serial.print(F("VVolts: " ));
  Serial.println(VVolts);

  Serial.print(F("Volts: " ));
  Serial.println(Volts);

  Serial.print(F("AVolts: "));
  Serial.println(AVolts, 4);

  Serial.print(F("Amps: "));
  Serial.println(Amps);

  Serial.print(F("Power: "));
  Serial.println(Power);

  Serial.print(F("Energy: "));
  Serial.println(Energy);
#endif

}


/*

  function to draw basline graph, data read from SD card, no error checking so file must exist.

*/
void DrawBaseLineGraph() {
  int i;

  int BLEnergy[92] = { 1, 5, 8, 12, 16, 19, 23, 26, 30, 33, 37, 40, 44, 47, 51, 54, 58, 62, 65, 69, 72, 75, 79, 82, 86, 89, 93, 96, 100, 103, 107, 110, 113,
                       117, 120, 124, 127, 130, 134, 137, 141, 144, 147, 151, 154, 157, 161, 164, 167, 170, 174, 177, 180, 184, 187, 190, 193, 197, 200, 203, 206, 209, 213, 216,
                       219, 222, 225, 228, 232, 235, 238, 241, 244, 247, 250, 253, 256, 259, 262, 265, 268, 271, 274, 277, 280, 283, 285, 288, 291, 294, 296, 300
                     };

  int BLPower[92] = {217, 216, 215, 215, 214, 213, 213, 213, 212, 211, 212, 212, 211, 211, 211, 211, 210, 210,
                     210, 209, 209, 209, 208, 208, 208, 207, 207, 207, 206, 206, 206, 205, 205, 204, 204, 204, 203, 203, 202, 202,
                     202, 201, 201, 200, 200, 199, 199, 199, 198, 198, 197, 197, 196, 196, 195, 195, 194, 194, 193,
                     192, 192, 191, 191, 190, 190, 189, 188, 188, 187, 186, 186, 185, 184, 183, 182, 181, 181, 180, 179, 178, 177, 176, 174, 173, 172, 170, 168, 167, 164, 162, 160
                    };

  ProfileGraph.drawGraph();
  StopBtn.draw();
  for (i = 1; i < 91; i++) {
    ProfileGraph.setX(i);
    ProfileGraph.plot(bPowerID, BLPower[i]);
    ProfileGraph.plot(bEnergyID, BLEnergy[i]);
  }

  // draw filename
  Display.setFont(Arial_16_Bold);
  Display.setTextColor(C_BLACK);
  Display.setCursor(15, 7);
  for (i = 0; i < 18; i++) {
    Display.print(FileName[i]);
  }

}

/*

  function to read the EEPROM for saved settings

*/

void GetParameters() {

  EEPROM.get(0, Battery);

  /*
    AmpOffset = 0.50;
    EEPROM.put(5, AmpOffset);
    mVPerAmp = 80.0;
    EEPROM.put(10, mVPerAmp);
    VoltOffset = 1.0;
    EEPROM.put(15, VoltOffset);
    VoltSlope = 5.7;
    EEPROM.put(20, VoltSlope);
  */
  if (Battery == 255) {
    // new unit zero everything out
    for (i = 0; i < 1000; i++) {
      EEPROM.put(i, 0);
    }
    // now set defaults
    Battery = 1;
    EEPROM.put(0, Battery);
    AmpOffset = 0.50;
    EEPROM.put(5, AmpOffset);
    mVPerAmp = 80.0;
    EEPROM.put(10, mVPerAmp);
    VoltOffset = .833;
    EEPROM.put(15, VoltOffset);
    VoltSlope = 5.7112;
    EEPROM.put(20, VoltSlope);
    Tester = 0;
    EEPROM.put(30, Tester);
    TotalTests = 0;
    EEPROM.put(40, TotalTests);

  }
  else {
    EEPROM.get(5, AmpOffset);
    EEPROM.get(10, mVPerAmp);
    EEPROM.get(15, VoltOffset);
    EEPROM.get(20, VoltSlope);
    EEPROM.get(30, Tester);
    EEPROM.get(40, TotalTests);

  }

#ifdef debug

  Serial.print(F("Battery: "));
  Serial.println(Battery);

  Serial.print(F("AmpOffset: "));
  Serial.println(AmpOffset, 3);

  Serial.print(F("mVPerAmp: "));
  Serial.println(mVPerAmp, 1);

  Serial.print(F("VoltOffset: "));
  Serial.println(VoltOffset, 1);

  Serial.print(F("VoltSlope: "));
  Serial.println(VoltSlope, 3);

  Serial.print(F("Tester: "));
  Serial.println(Tester);

  Serial.print(F("TotalTests: "));
  Serial.println(TotalTests);

#endif

}



/*

  function to get the time based on internal teensy chip

*/

time_t getTeensy3Time()
{
  return Teensy3Clock.get();
}


unsigned long processSyncMessage() {
  unsigned long pctime = 0L;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013

  if (Serial.find(TIME_HEADER)) {
    pctime = Serial.parseInt();
    return pctime;
    if ( pctime < DEFAULT_TIME) { // check the value is a valid time (greater than Jan 1 2013)
      pctime = 0L; // return 0 to indicate that the time is not valid
    }
  }
  return pctime;
}

/*

  function to set the time

*/

void SetTime() {

  bool CalKeepIn = true;

  float rYear = year(), rMonth = month(), rDay = day(), rHour = hour(), rMinute = minute();

  Display.fillScreen(C_WHITE);

#ifdef debug
  Serial.println("printing current time");
  Serial.println(rMonth);
  Serial.println(rDay);
  Serial.println(rYear);
  Serial.println(rHour);
  Serial.println(rMinute);
#endif

  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setFont(Arial_16_Bold);
  Display.setCursor(10 , 10 );
  Display.print(F("Set Date and Time"));

  RTCY.draw(rYear);
  RTCM.draw(rMonth);
  RTCD.draw(rDay);
  RTCH.draw(rHour);
  RTCI.draw(rMinute);

  DoneBtn.draw();

  Display.setFont(Arial_12_Bold);
  Display.setTextColor(C_BLACK);

  while (CalKeepIn) {

    if (Touch.touched()) {

      ProcessTouch();

      RTCM.slide(BtnX, BtnY);
      RTCD.slide(BtnX, BtnY);
      RTCY.slide(BtnX, BtnY);
      RTCH.slide(BtnX, BtnY);
      RTCI.slide(BtnX, BtnY);

      if (PressIt(DoneBtn) == true) {
        CalKeepIn = false;
      }

      Display.fillRect(5, 30, 80 , 190, C_WHITE);
      rMonth = RTCM.value;
      rDay = RTCD.value;
      rYear = RTCY.value;
      rHour = RTCH.value;
      //byte shit = RTCH.value;
      //Serial.print("dat "); Serial.println(shit);
      rMinute = RTCI.value;
    }

    Display.setCursor(10 , 40);
    Display.print(F("Mo"));
    Display.setCursor(45 , 40);
    Display.print(rMonth, 0);

    Display.setCursor(10 , 75);
    Display.print(F("Da"));
    Display.setCursor(45 , 75);
    Display.print(rDay, 0);

    Display.setCursor(10 , 110);
    Display.print(F("Yr"));
    Display.setCursor(45 , 110);
    Display.print(rYear, 0);

    Display.setCursor(10 , 145);
    Display.print(F("Hr"));
    Display.setCursor(45 , 145);
    Display.print(rHour, 0);

    Display.setCursor(10 , 180);
    Display.print(F("Mn"));
    Display.setCursor(45 , 180);
    Display.print(rMinute, 0);

  }

#ifdef debug
  Serial.println("Saving time");
  Serial.println(rMonth);
  Serial.println(rDay);
  Serial.println(rYear);
  Serial.println(rHour);
  Serial.println(rMinute);
#endif

  setTime(rHour, rMinute, 1, rDay, rMonth, rYear);
  Teensy3Clock.set(now());
  RTCTime = now();

}

void DrawDataHeader() {

  int t = GRAPH_Y - GRAPH_H;
  Display.setFont(Arial_10);
  Display.setTextColor(C_BLACK, C_WHITE);


  // time
  Display.setCursor(GRAPH_X + GRAPH_W + 6, t);
  Display.setFont(Arial_10_Bold);
  Display.print(F("Time: "));
  t = t + 27;
  // Volts
  Display.setCursor(GRAPH_X + GRAPH_W + 6 , t);
  Display.setFont(Arial_10_Bold);
  Display.print(F("Volts: "));
  t = t + 27;
  // Amps
  Display.setCursor(GRAPH_X + GRAPH_W + 6 , t);
  Display.setFont(Arial_10_Bold);
  Display.print(F("Amps: "));
  t = t + 27;
  // Power
  Display.setCursor(GRAPH_X + GRAPH_W + 6 , t);
  Display.setFont(Arial_10_Bold);
  Display.print(F("Power: "));
  t = t + 27;

  // Energy
  Display.setCursor(GRAPH_X + GRAPH_W + 6 , t);
  Display.setFont(Arial_10_Bold);
  Display.print(F("Energy: "));
  t = t + 27;


#ifdef TESTER_FOR_PATRIOT_RACING
  // temp
  Display.setCursor(GRAPH_X + GRAPH_W + 6 , t);
  Display.setFont(Arial_10_Bold);
  Display.print(F("Temp: "));

  Display.setCursor(GRAPH_X + GRAPH_W + 6 , t + 12);
  Display.setFont(Arial_10);

  if (BatteryTemp.value == 0) {
    Display.print(TesterTemp, 1);
  }
  else if (BatteryTemp.value == 1) {
    Display.print(F("77 deg"));
  }
  else if (BatteryTemp.value == 2) {
    Display.print(F("85 deg"));
  }

#endif
  t = t + 27;
  Display.setCursor(GRAPH_X + GRAPH_W + 12 , t);
  Display.setFont(Arial_10_Bold);
  if (!TestTime.value) { // post race
    Display.print(F("POST"));
  }
  else { // post race
    Display.print(F("PRE"));
  }

  // tester temperature
  Display.setFont(Arial_10_Bold);
  Display.setCursor(160, 225);
  Display.setTextColor(C_BLACK, C_WHITE);
  Display.print(F("Tester temp: "));

}

void DrawData() {

  int t = GRAPH_Y - GRAPH_H;
  // now draw the values
  Display.setFont(Arial_10);
  Display.setTextColor(C_BLACK, C_WHITE);

  // time
  if (RunTest) {
    m = (ElapsedCurrentTime / 1000 ) / 60 ;
    s = (ElapsedCurrentTime / 1000 ) % 60;
  }
  memset(str, ' ', sizeof(str));
  sprintf(str, "%03d:%02d", m, s);

  Display.setFont(Arial_10);
  Display.setCursor(GRAPH_X + GRAPH_W + 6  , t + 12);
  fTime.setTextColor(C_BLACK, C_WHITE);
  fTime.print(str);
  t = t + 27;
  // Volts
  Display.setFont(Arial_10);
  Display.setCursor(GRAPH_X + GRAPH_W + 6 , t + 12);
  fVolts.setTextColor(C_BLACK, C_WHITE);
  fVolts.print(Volts, 2);
  t = t + 27;
  // Amps
  Display.setFont(Arial_10);
  Display.setCursor(GRAPH_X + GRAPH_W + 6 , t + 12);
  fAmps.setTextColor(C_BLACK, C_WHITE);
  fAmps.print(Amps, 2);
  t = t + 27;
  // Power
  Display.setFont(Arial_10);
  Display.setCursor(GRAPH_X + GRAPH_W + 6 , t + 12);
  fPower.setTextColor(C_BLACK, C_WHITE);
  fPower.print(Power, 0);
  t = t + 27;

  // Energy
  Display.setFont(Arial_10);
  Display.setCursor(GRAPH_X + GRAPH_W + 6 , t + 12 );
  fEnergy.setTextColor(C_BLACK, C_WHITE);
  fEnergy.print(Energy, 0);

  t = t + 27;

  Display.setCursor(260, 225);
  Display.setFont(Arial_10);
  hTemp.setTextColor(C_BLACK, C_WHITE);
  hTemp.print(TesterTemp, 1);

}


float GetTemp(int pin) {

  thVolts = 0;
  TempK = 0.0;
  TempF = 0.0;

  for (i = 0; i < 100; i++) {
    thVolts =   thVolts + analogRead(pin);
  }
  // compute motor temperature
  thVolts = thVolts / 100;
  thVolts = thVolts / (4096.0f / 3.3f);

  // voltage divider calculation
  // vo = 5 * r2 /(r1+r2)
  // solve for r2
  // get the exact value for voltage divider r1
  tr2 = ( thVolts * 9800.0f) / (3.3f - thVolts);

  //equation from data sheet
  TempK = 1.0f / (NTC_A + (NTC_B * (log(tr2 / 10000.0f))) + (NTC_C * pow(log(tr2 / 10000.0f), 2)) + (NTC_D * pow(log(tr2 / 10000.0f), 3)));
  TempF = (TempK * 1.8f) - 459.67f;

  return TempF;

}

void SplashScreen() {

  Display.fillScreen(C_BLACK);

#ifdef TESTER_FOR_PATRIOT_RACING
  Display.fillRect(10, 20, 300, 20, C_RED);
  Display.fillRect(10, 160, 300, 20, C_BLUE);
  Display.setTextColor(C_WHITE);
  Display.setFont(FONT_HEADER);
  Display.setCursor(10, 60);
  Display.print(TEAM_NAME);
  Display.setFont(FONT_HEADING);
  Display.setCursor(10, 100);
  Display.print(F(CODE_VERSION));
  Display.print(F(" Test unit: "));
  Display.print(Tester);
#endif

#ifndef TESTER_FOR_PATRIOT_RACING
  Display.fillRect(10, 20, 300, 20, C_DKGREY);
  Display.fillRect(10, 160, 300, 20, C_DKCYAN);
  Display.setTextColor(C_WHITE);
  Display.setFont(FONT_HEADER);
  Display.setCursor(10, 60);
  Display.print(TEAM_NAME);
  Display.setFont(FONT_HEADING);
  Display.setCursor(10, 100);
  Display.print(F(CODE_VERSION));
  Display.print(F(" Test unit: "));
  Display.print(Tester);
#endif

  Display.setCursor(10, 130);
  Display.print(month());
  Display.print(F("-"));
  Display.print(day());
  Display.print(F("-"));
  Display.print(year());
  Display.print(F("  "));
  Display.print(hour());
  Display.print(F(":"));
  if (minute() < 10) {
    Display.print("0");
  }
  Display.print(minute());

  digitalWrite(LCD_PIN , HIGH);

}

void DownloadData(bool SoundWarning) {

  bool KeepIn = true;
  bool ISSDCard = false;
  float rw = 0;
  uint16_t FileCount = 0;
  int fc = 0;
  char FileName[25];
  int next = 0;
  char c;

  File dir;
  File fil;

  if (SoundWarning) {

    Display.setFont(FONT_HEADER);
    Display.fillScreen(C_WHITE);
    Display.setTextColor(C_RED, C_WHITE);
    Display.setCursor(10, 70); Display.print(F("SSD memory FULL"));
    Display.setFont(FONT_HEADING);
    Display.setCursor(10, 130); Display.print(F("1. Download to an SD card"));
    Display.setCursor(10, 180); Display.print(F("2. Erase SSD chip"));
    SSDDoneBtn.draw();
    analogWriteFrequency(BUZZ_PIN, 200);
    for (i = 0; i < 3; i ++) {
      analogWrite(BUZZ_PIN, 100);
      delay(500);
      analogWrite(BUZZ_PIN, 0);
      delay(500);
    }

    while (true) {
      if (Touch.touched()) {
        ProcessTouch();
        if (PressIt(SSDDoneBtn) == true) {
          break;
        }
      }
    }
  }

  if (SoundWarning) {
    SSDDoneBtn.disable();
  }
  DrawDownloadData();

  Display.setFont(Arial_16);
  Display.setTextColor(C_BLACK, C_WHITE);

  FileCount = 0;

  dir = SSD.open("/");

  while (true) {
    File fil = dir.openNextFile();

    if (! fil) {
      break;
    }
    FileCount++;
    fil.close();
  }
  dir.close();
  dir = SSD.open("/");

  Display.fillRect(15, 170, 280, 20, C_WHITE);
  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setCursor(15, 180);
  Display.setFont(Arial_12_Bold);
  Display.print(F("File Count: ")); Display.print(FileCount);

  while (KeepIn) {

    if (Touch.touched()) {
      ProcessTouch();

      if (PressIt(SSDDoneBtn) == true) {
        KeepIn = false;
      }
      if (PressIt(SetTesterBtn) == true) {

        KeyPad(Tester, 1, 5);
        sprintf(buf, "Tester: %d", Tester);

        SetTesterBtn.setText(buf);
        EEPROM.put(30, Tester);
        DrawDownloadData();

      }
      if (PressIt(SSDEraseBtn) == true) {

        Display.fillRect(15, 180, 280, 20, C_WHITE);
        Display.setTextColor(C_RED, C_WHITE);
        Display.setCursor(15, 180);
        Display.setFont(Arial_12_Bold);
        Display.print(F("Erasing: "));


        SSD.quickFormat();

        // reset the total test to the SSD chip
        TotalTests = 0;
        EEPROM.put(40, TotalTests);

        Display.print(F("Complete"));
        delay(2000);
        Display.fillRect(15, 180, 280, 20, C_WHITE);

        SSDDoneBtn.enable();
        SSDDoneBtn.draw();

        FileCount = 0;

        Display.fillRect(15, 170, 280, 20, C_WHITE);
        Display.setTextColor(C_BLACK, C_WHITE);
        Display.setCursor(15, 180);
        Display.setFont(Arial_12_Bold);
        Display.print(F("File Count: ")); Display.print(FileCount);
      }

      if (PressIt(SSDDownloadBtn) == true) {

        if (FileCount > 0) {

          ISSDCard = SDCard.begin(SD_CS, SD_SCK_MHZ(20));  //SD

          if (!ISSDCard) {

#ifdef DO_DEBUG
            Serial.println("no card");
#endif
            Display.setFont(Arial_12_Bold);
            Display.fillRect(15, 180, 280, 20, C_WHITE);
            Display.setCursor(15, 180);
            Display.setTextColor(C_RED, C_WHITE);

            Display.print(F("No SD Card, or defective SD card."));
            delay(5000);
            Display.fillRect(15, 180, 280, 20, C_WHITE);
            return;

          }

          while (true) {

            fil = dir.openNextFile();

            if (! fil) {
              break;
            }

            strcpy(FileName, fil.name());

            DataFile = SSD.open(FileName);

            if (!DataFile) {
              Display.fillRect(15, 180, 280, 20, C_WHITE);
              Display.setTextColor(C_RED, C_WHITE);
              Display.setCursor(15, 180);
              Display.print(F("No data file"));
              delay(5000);
              return;
            }

            next = 0;

            while (SDCard.exists(FileName)) {
              next++;

              FileName[16] = (int) ((next / 10) % 10) + '0';
              FileName[17] = (int) (next  % 10) + '0';

              if (next > 99) {
                break;
              }
            }

            delay(100);

            Display.fillRect(15, 180, 280, 20, C_WHITE);
            Display.setTextColor(C_BLACK, C_WHITE);
            Display.setCursor(15, 180);
            Display.setFont(Arial_12_Bold);
            Display.print(F("Copying: ")); Display.print(FileName);

            ISSDCard = SDDataFile.open(FileName, O_WRITE | O_CREAT);

            if (!ISSDCard) {
#ifdef DO_DEBUG
              Serial.println("no card");
#endif
              Display.setTextColor(C_RED, C_WHITE);
              Display.setCursor(15, 180);
              Display.print(F("SD write error"));
              delay(5000);
              return;
            }

            while (DataFile.available()) {
              c = DataFile.read();
              SDDataFile.write(c);
            }

            // make status bar shows something less that 100 for all files then 100% complete
            fc++;
            rw = (fc * 297) / (FileCount);
            Display.fillRoundRect(12, 202, rw, 31, 1, C_GREEN);

            SDDataFile.timestamp(T_CREATE, year(), month(), day(), hour(), minute(), second());
            SDDataFile.timestamp(T_WRITE, year(), month(), day(), hour(), minute(), second());
            SDDataFile.timestamp(T_ACCESS, year(), month(), day(), hour(), minute(), second());

            SDDataFile.close();
            DataFile.close();
            delay(200);
          }
        }
      }
      Display.fillRect(15, 180, 280, 20, C_WHITE);
      Display.setTextColor(C_BLACK, C_WHITE);
      Display.setCursor(15, 180);
      Display.setFont(Arial_12_Bold);
      Display.print(F("File Count: ")); Display.print(FileCount);
      Display.fillRoundRect(12, 202, 296, 31, 1, C_WHITE);
      Display.drawRoundRect(10, 200, 300, 34, 3, C_GREY);
      Display.drawRoundRect(11, 201, 298, 33, 2, C_DKGREY);
    }
  }
}

void DrawDownloadData() {
  //nothing fancy, just a header and some buttons
  Display.fillScreen(C_WHITE);

  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setFont(Arial_16_Bold);
  Display.setCursor(10 , 10 );
  Display.fillRect(0 , 0, 320, 40, C_LTGREY );
  Display.print(F("SSD Options"));

  SSDEraseBtn.draw();
  SSDDownloadBtn.draw();
  SetTesterBtn.draw();
  SSDDoneBtn.draw();

  Display.drawRoundRect(10, 200, 300, 34, 3, C_GREY);
  Display.drawRoundRect(11, 201, 298, 33, 2, C_DKGREY);

}

bool SaveBMP24(const char *FileName) {
  File dataFile;
  int hh, ww;
  int i, j = 0;
  uint8_t r, g, b;
  uint16_t rgb;
  hh = Display.height();
  ww = Display.width();

  dataFile = SSD.open(FileName, FILE_WRITE);

  if (!dataFile) {
    return false;
  }

  unsigned char bmFlHdr[14] = {
    'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0
  };

  // set color depth to 24 as we're storing 8 bits for r-g-b
  unsigned char bmInHdr[40] = {
    40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 24, 0
  };

  unsigned long fileSize = 3ul * hh * ww + 54;

  bmFlHdr[ 2] = (unsigned char)(fileSize      );
  bmFlHdr[ 3] = (unsigned char)(fileSize >>  8);
  bmFlHdr[ 4] = (unsigned char)(fileSize >> 16);
  bmFlHdr[ 5] = (unsigned char)(fileSize >> 24);

  bmInHdr[ 4] = (unsigned char)(       ww      );
  bmInHdr[ 5] = (unsigned char)(       ww >>  8);
  bmInHdr[ 6] = (unsigned char)(       ww >> 16);
  bmInHdr[ 7] = (unsigned char)(       ww >> 24);
  bmInHdr[ 8] = (unsigned char)(       hh      );
  bmInHdr[ 9] = (unsigned char)(       hh >>  8);
  bmInHdr[10] = (unsigned char)(       hh >> 16);
  bmInHdr[11] = (unsigned char)(       hh >> 24);

  dataFile.write(bmFlHdr, sizeof(bmFlHdr));
  dataFile.write(bmInHdr, sizeof(bmInHdr));

  for (i = hh - 1; i >= 0; i--) {
    for (j = 0; j < ww; j++) {

      // if you are attempting to convert this library to use another display library, this
      // is where you may run into issues
      // the libries must have a readPixel function
      rgb = Display.readPixel(j, i);

      // convert the 16 bit color to full 24
      // that way we have a legit bmp that can be read into the
      // bmp reader below
      Display.color565toRGB(rgb, r, g, b);

      // write the data in BMP reverse order
      dataFile.write(b);
      dataFile.write(g);
      dataFile.write(r);
    }
  }

  dataFile.close();
  delay(10);
  return true;

}

void draw565Bitmap(uint16_t x, uint16_t y, const uint16_t *bitmap, uint16_t w, uint16_t h) {

  uint32_t offset = 0;
  uint16_t j = 0, i = 0;

  for (i = 0; i < h; i++) {
    for (j = 0; j < w; j++) {
      Display.drawPixel(j + x, i + y, bitmap[offset]);
      offset++;
    }
  }

}

////////////////////////////////////
//
// End of code
//
///////////////////////////////////
