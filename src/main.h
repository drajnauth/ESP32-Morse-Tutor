#ifndef _MAIN_H_
#define _MAIN_H_

#include <Arduino.h>

#include "network.h"

// Added by VE3OOI
#define MAX_CHAR_STRING 30  // Maximum string for various string definations
#define MAX_SSID_STRING 30  // Maximum string for SSID
#define INIT_FLAG 0xA1      // Flag for eeprom - last digit is revision level
#define DEFAULT_EEPROM_ADDRESS \
  128  // Starting address for EEPROM configuration strut

//===================================  Hardware Connections
//=============================
#define TFT_DC 21          // Display "DC" pin
#define TFT_CS 05          // Display "CS" pin
#define SD_CS 17           // SD card "CS" pin
#define ENCODER_A 16       // Rotary Encoder output A
#define ENCODER_B 4        // Rotary Encoder output B
#define ENCODER_BUTTON 15  // Rotary Encoder switch
#define PADDLE_A 32        // Morse Paddle "dit"
#define PADDLE_B 33        // Morse Paddle "dah"
#define AUDIO 13           // Audio output
#define LED 2              // onboard LED pin
#define SCREEN_ROTATION 3  // landscape mode: use '1' or '3'

//===================================  Morse Code Constants
//=============================
#define DEFAULTSPEED 13      // character speed in Words per Minute
#define MAXSPEED 50          // fastest morse speed in WPM
#define MINSPEED 3           // slowest morse speed in WPM
#define DEFAULTPITCH 1200    // default pitch in Hz of morse audio
#define MAXPITCH 2800        // highest allowed pitch
#define MINPITCH 300         // how low can you go
#define WORDSIZE 5           // number of chars per random word
#define MAXWORDSPACES 99     // maximum word delay, in spaces
#define FLASHCARDDELAY 2000  // wait in mS between cards
#define ENCODER_TICKS 3      // Ticks required to register movement
#define FNAMESIZE 15         // max size of a filename
#define MAXFILES 20          // max number of SD files recognized
#define IAMBIC_A 1           // Iambic Keyer Mode B
#define IAMBIC_B 2           // Iambic Keyer Mode A
#define LONGPRESS 1000       // hold-down time for long press, in mSec
#define SUPPRESSLED false    // if true, do not flash diagnostic LED

//===================================  Color Constants
//==================================
#define BLACK 0x0000
#define BLUE 0x001F
#define NAVY 0x000F
#define RED 0xF800
#define MAROON 0x7800
#define GREEN 0x07E0
#define LIME 0x0400
#define CYAN 0x07FF
#define TEAL 0x03EF
#define PURPLE 0x780F
#define PINK 0xF81F
#define YELLOW 0xFFE0
#define ORANGE 0xFD20
#define BROWN 0x79E0
#define WHITE 0xFFFF
#define OLIVE 0x7BE0
#define SILVER 0xC618
#define GRAY 0x7BEF
#define DARKGRAY 0x1208

// ==================================  Menu Constants
// ===================================
#define DISPLAYWIDTH 320   // Number of LCD pixels in long-axis
#define DISPLAYHEIGHT 240  // Number of LCD pixels in short-axis
#define TOPMARGIN 30       // All submenus appear below top line
#define MENUSPACING 100    // Width in pixels for each menu column
#define ROWSPACING 23      // Height in pixels for each text row
#define COLSPACING 12      // Width in pixels for each text character
#define MAXCOL DISPLAYWIDTH / COLSPACING  // Number of characters per row
#define MAXROW \
  (DISPLAYHEIGHT - TOPMARGIN) / ROWSPACING  // Number of text-rows per screen
#define FG GREEN                            // Menu foreground color
#define BG BLACK                            // App background color
#define SELECTFG BLUE                       // Selected Menu foreground color
#define SELECTBG WHITE                      // Selected Menu background color
#define TEXTCOLOR YELLOW                    // Default non-menu text color
#define TXCOLOR WHITE                       // color of outgoing (sending) text
#define RXCOLOR GREEN                       // color of incoming (received) text
#define ELEMENTS(x) \
  (sizeof(x) / sizeof(x[0]))  // Handy macro for determining array sizes

// Added by VE3OOI
// Main Strut containing configuration data
typedef struct {
  unsigned char flag;
  int charSpeed;    // speed at which characters are sent, in WPM
  int codeSpeed;    // overall code speed, in WPM
  int pitch;        // frequency of audio output, in Hz
  bool usePaddles;  // if true, using paddles; if false, straight key
  int ditPaddle;    // digital pin attached to dit paddle
  int dahPaddle;    // digital pin attached to dah paddle
  int kochLevel;    // current Koch lesson #
  int xWordSpaces;  // extra spaces between words

  int keyerMode;   // current keyer mode
  int startItem;   // startup activity.  0 = main menu
  int brightness;  // backlight level (range 0-100%)
  int score;       // copy challange score
  int hits;        // copy challange correct #
  int misses;      // copy channange incorrect #
  int textColor;   // foreground (text) color
  int bgColor;     // background (screen) color
  char myCall[10];
  char wifi_ssid[MAX_SSID_STRING];
  char wifi_password[MAX_CHAR_STRING];
  char mqtt_userid[MAX_CHAR_STRING];
  char mqtt_password[MAX_CHAR_STRING];
  char mqtt_server[MAX_CHAR_STRING];
  char room[MAX_CHAR_STRING];
} TUTOR_STRUT;

// Added by VE3OOI
// Function Prototypes

//////
void initializeMem(void);
void printMem(void);
void loadMem(void);
void clearMem(void);
void dumpMem(void);

//////
void buttonISR(void);
void rotaryISR(void);
boolean buttonDown(void);
void waitForButtonRelease(void);
void waitForButtonPress(void);
bool longPress(void);
int readEncoder(int numTicks);
void keyUp(void);
void keyDown(void);
bool ditPressed(void);
bool dahPressed(void);

//////
int extracharDit(void);
int intracharDit(void);
void characterSpace(void);
void wordSpace(void);
void dit(void);
void dah(void);
void sendElements(int x);
void roger(void);
void sendCharacter(char c);
void sendString(char *ptr);
void sendMorseWord(char *ptr);
void displayWPM(void);
void checkForSpeedChange(void);
void checkPause(void);
void sendKochLesson(int lesson);
void introLesson(int lesson);
int getLessonNumber(void);
void sendKoch(void);
void addChar(char *str, char ch);
char randomLetter(void);
char randomNumber(void);
void randomCallsign(char *call);
void randomRST(char *rst);
void sendNumbers(void);
void sendLetters(void);
void sendMixedChars(void);
void sendPunctuation(void);
void sendWords(void);
void sendCallsigns(void);
void sendQSO(void);

//////
int getFileList(char list[][FNAMESIZE]);
void displayFiles(char menu[][FNAMESIZE], int top, int itemCount);
int fileMenu(char menu[][FNAMESIZE], int itemCount);
void sendFile(char *filename);
void sendFromSD(void);

//////
void checkSpeed(void);
int decode(int code);
char paddleInput(void);
char straightKeyInput(void);
char morseInput(void);
void practice(void);
void copyCallsigns(void);
void copyOneChar(void);
void copyTwoChars(void);
void copyWords(void);
void encourageUser(void);
void displayNumber(int num, int color, int x, int y, int wd, int ht);
void showScore(void);
void mimic1(char *text);
void showHitsAndMisses(int hits, int misses);
void headCopy(void);
void hitTone(void);
void missTone(void);
void mimic2(char *text);
void mimic(char *text);
void flashcards(void);

void twoWay(void);
void printConfig(void);
void saveConfig(void);
void loadConfig(void);
void checkConfig(void);
void useDefaults(void);
char *ltrim(char *str);
void selectionString(int choice, char *str);
void showMenuChoice(int choice);
void changeStartup(void);
void changeBackground(void);
void changeTextColor(void);
void changeBrightness(void);
void setScreen(void);
void setCodeSpeed(void);
void setFarnsworth(void);
void setExtraWordDelay(void);
void setSpeed(void);
void setPitch(void);
void configKey(void);
void setCallsign(void);
void clearMenu(void);
void clearBody(void);
void clearScreen(void);
void newScreen(void);
void showCharacter(char c, int row, int col);
void addCharacter(char c);
int getMenuSelection(void);
void setTopMenu(char *str);
void showSelection(int choice);
void showMenuItem(char *item, int x, int y, int fgColor, int bgColor);
int topMenu(char *menu[], int itemCount);
void displayFrame(char *menu[], int top, int itemCount);
int subMenu(char *menu[], int itemCount);
void setBrightness(int level);
void initEncoder(void);
void initMorse(void);
void initSD(void);
void initScreen(void);
void splashScreen(void);

#endif  // _MAIN_H_