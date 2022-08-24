/**************************************************************************
       Title:   Morse Tutor ESP32
      Author:   Original Bruce E. Hall, w8bh.net
                Updated Dave Rajnauth, VE3OOI (see change log below)
        Date:   07 Jan 2022
    Hardware:   ESP32 DevBoard "HiLetGo", ILI9341 TFT display
    Software:   Arduino IDE 1.8.19
                ESP32 by Expressif Systems 1.0.6
                Adafruit GFX Library 1.5.3
                Adafruit ILI9341 1.5.6
       Legal:   Copyright (c) 2020  Bruce E. Hall: Open Source under the terms
 of the MIT License. 2022 Update by VE3OOI: Licensed under Creative in Commons
 (Attribution-NonCommercial-ShareAlike)

 Description:   Practice sending & receiving morse code
                Inspired by Jack Purdum's "Morse Code Tutor"

                >> THIS VERSION UNDER DEVELOPMENT <<<
 **************************************************************************/

/*

***** Release v0.1 Change Log *****
Original W8BH code/firmware updated by Dave Rajnauth, VE3OOI June 2022. Updated
code available at https://github.com/drajnauth/ESP32-Morse-Tutor

Change log:

1. Moved code to PlatformIO instead of arduino (Arduino IDE is a joke IMO).
Should be able to work with Arduino IDE provided libraries are installed.

2. Removed point to point WiFi connection (ESP_NOW) and replaced with MQTT based
messages from AWS based cloud server (ve3ooi.ddns.net)

3. New Libraries used are:
PubSubClient
**Adafruit BusIO was needed to make Adafruit_GFX & Adafruit_ILI9341 to work.
**Same needed for Arduino IDE.  This may be need for later revision of Adafruit
lib or Arduino IDE

4. There is "room" which is a "topic" for MQTT message.  All units that use the
same room will participate in the online twoway call.  Same as over the air
practice. There is also a username and password.
//****TODO Need to add TLS encrption to protect password

5. Each message to MQTT server is of the form "<UniqueID>:<c>".
  <UniqueID> is a generated 3 character code.
  <c> is the character being transmitted
//****Note if 2 units use the same UniqueID, the communicaton will not work
between them


6. Moved config to a structure which has MQTT parameters (DNS name, SSID/passwd,
MQTT user/passwd, room)
//****TODO Need to add TLS key to config structure to protect transport to MQTT
Server

7. Brokeup the code so that all network related code is located in "network.cpp"
and all #defines are located in "main.h" and network.h

8. Fixed error on startup related to using PWM frequency and duty cycle
resolution that is not supported

9. Fixed error in SD file processing.  The file list was missing the first
character of each file name.

10. There is a part of send SD file across wireless which will will probably
cause a buffer overflow and break the code since receive buffer is only 100
characters. There were problems with sending MQTT messages that are too long.
Wireless transmission craps out if there is a buffer overflow (its silient
- no errors generated - means no checks in place for MQTT library)

11. Added CLI to config menu option to allow use of CLI to define parameters.
Currently it only addresses updates I made, but can be easily updated to allow
changing of ANY configuration parameter. Currently, I kept the local defined
config parameter but used a structure that is written to EEPROM The structure is
used directly by my updates.  W8BGH config parameters are stores and retrieved
from the config structure.

The CLI allows for changing of any of the config parameters that used by my
code.   Specifically:
 char wifi_ssid[MAX_SSID_STRING];  // SSID to connect WiFi.  Note maximum length
 char wifi_password[MAX_SSID_STRING]; // password to connect WiFi.  Note maximum
length char mqtt_userid[MAX_CHAR_STRING]; // userid to connect MQTT Server. Will
be provided to you char mqtt_password[MAX_CHAR_STRING]; // password to connect
MQTT Server.  Will be provided to you char mqtt_server[MAX_CHAR_STRING]; // MQTT
server DNS name.  Will be provided to you char room[MAX_CHAR_STRING];// MQTT
Topic which I call room.  Will be provided to you

Defaults for the above can be hardcoded via several defines (see network.h):
  #define DEFAULT_SSID "Enter your SSID here";
  #define DEFAULT_WIFI_PASSWORD "Enter WIFI password here";
  #define DEFAULT_MQTT_USERNAME "Enter provided MQTT username here";
  #define DEFAULT_MQTT_PASSWORD "Enter provided MQTT password here";
  #define DEFAULT_SERVER_ADDRESS "Enter provided MQTT server DNS name here";

Enter "H" at CLI for help - listing of commands.

CLI is currently configured for com port running 115200.

If you have double charaters when you type at CLI, comment the local echo define
(see mail.h) #define TERMINAL_ECHO 1  // Enable local echo

If you don't want a CLI (e.g. to save RAM and Flash memory usage), uncomment the
following define (see mail.h)
//#define REMOVE_CLI // uncomment this line to remove CLI routines

*/

//===================================  INCLUDES
//=========================================

#include <Adafruit_GFX.h>      // Version 1.5.3
#include <Adafruit_ILI9341.h>  // Version 1.5.6
#include <EEPROM.h>
#include <SD.h>
#include <WiFi.h>
#include <arduino.h>
//#include <esp_now.h>

// added by VE3OOI
#include <PubSubClient.h>

#include "UART.h"
#include "main.h"
#include "network.h"

const word colors[] = {BLACK, BLUE,  NAVY,   RED,  MAROON,  GREEN,  LIME,
                       CYAN,  TEAL,  PURPLE, PINK, YELLOW,  ORANGE, BROWN,
                       WHITE, OLIVE, SILVER, GRAY, DARKGRAY};

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

//===================================  Rotary Encoder Variables
//=========================
volatile int rotaryCounter = 0;  // "position" of rotary encoder (increments CW)
volatile boolean button_pressed = false;  // true if the button has been pushed
volatile boolean button_released =
    false;  // true if the button has been released (sets button_downtime)
volatile long button_downtime = 0L;  // ms the button was pushed before released

//===================================  Morse Code Variables
//=============================

// The following is a list of the 100 most-common English words, in frequency
// order. See: https://www.dictionary.com/e/common-words/ from The Brown Corpus
// Standard Sample of Present-Day American English (Providence, RI: Brown
// University Press, 1979)

// Modified by VE3OOI to address converstion from *char *const
char *words[] = {
    (char *)"THE",   (char *)"OF",    (char *)"AND",     (char *)"TO",
    (char *)"A",     (char *)"IN",    (char *)"THAT",    (char *)"IS",
    (char *)"WAS",   (char *)"HE",    (char *)"FOR",     (char *)"IT",
    (char *)"WITH",  (char *)"AS",    (char *)"HIS",     (char *)"ON",
    (char *)"BE",    (char *)"AT",    (char *)"BY",      (char *)"I",
    (char *)"THIS",  (char *)"HAD",   (char *)"NOT",     (char *)"ARE",
    (char *)"BUT",   (char *)"FROM",  (char *)"OR",      (char *)"HAVE",
    (char *)"AN",    (char *)"THEY",  (char *)"WHICH",   (char *)"ONE",
    (char *)"YOU",   (char *)"WERE",  (char *)"ALL",     (char *)"HER",
    (char *)"SHE",   (char *)"THERE", (char *)"WOULD",   (char *)"THEIR",
    (char *)"WE",    (char *)"HIM",   (char *)"BEEN",    (char *)"HAS",
    (char *)"WHEN",  (char *)"WHO",   (char *)"WILL",    (char *)"NO",
    (char *)"MORE",  (char *)"IF",    (char *)"OUT",     (char *)"SO",
    (char *)"UP",    (char *)"SAID",  (char *)"WHAT",    (char *)"ITS",
    (char *)"ABOUT", (char *)"THAN",  (char *)"INTO",    (char *)"THEM",
    (char *)"CAN",   (char *)"ONLY",  (char *)"OTHER",   (char *)"TIME",
    (char *)"NEW",   (char *)"SOME",  (char *)"COULD",   (char *)"THESE",
    (char *)"TWO",   (char *)"MAY",   (char *)"FIRST",   (char *)"THEN",
    (char *)"DO",    (char *)"ANY",   (char *)"LIKE",    (char *)"MY",
    (char *)"NOW",   (char *)"OVER",  (char *)"SUCH",    (char *)"OUR",
    (char *)"MAN",   (char *)"ME",    (char *)"EVEN",    (char *)"MOST",
    (char *)"MADE",  (char *)"AFTER", (char *)"ALSO",    (char *)"DID",
    (char *)"MANY",  (char *)"OFF",   (char *)"BEFORE",  (char *)"MUST",
    (char *)"WELL",  (char *)"BACK",  (char *)"THROUGH", (char *)"YEARS",
    (char *)"MUCH",  (char *)"WHERE", (char *)"YOUR",    (char *)"WAY"};
char *antenna[] = {(char *)"YAGI", (char *)"DIPOLE", (char *)"VERTICAL",
                   (char *)"HEXBEAM", (char *)"MAGLOOP"};
char *weather[] = {(char *)"HOT",    (char *)"SUNNY",  (char *)"WARM",
                   (char *)"CLOUDY", (char *)"RAINY",  (char *)"COLD",
                   (char *)"SNOWY",  (char *)"CHILLY", (char *)"WINDY",
                   (char *)"FOGGY"};
char *names[] = {
    (char *)"WAYNE", (char *)"TYE",  (char *)"DARREN",   (char *)"MICHAEL",
    (char *)"SARAH", (char *)"DOUG", (char *)"FERNANDO", (char *)"CHARLIE",
    (char *)"HOLLY", (char *)"KEN",  (char *)"SCOTT",    (char *)"DAN",
    (char *)"ERVIN", (char *)"GENE", (char *)"PAUL",     (char *)"VINCENT"};
char *cities[] = {(char *)"DAYTON, OH",      (char *)"HADDONFIELD, NJ",
                  (char *)"MURRYSVILLE, PA", (char *)"BALTIMORE, MD",
                  (char *)"ANN ARBOR, MI",   (char *)"BOULDER, CO",
                  (char *)"BILLINGS, MT",    (char *)"SANIBEL, FL",
                  (char *)"CIMMARON, NM",    (char *)"TYLER, TX",
                  (char *)"OLYMPIA, WA"};
char *rigs[] = {(char *)"YAESU FT101", (char *)"KENWOOD 780",
                (char *)"ELECRAFT K3", (char *)"HOMEBREW",
                (char *)"QRPLABS QCX", (char *)"ICOM 7410",
                (char *)"FLEX 6400"};
char punctuation[] = "!@$&()-+=,.:;'/";
char prefix[] = {'A', 'W', 'K', 'N'};
char koch[] = "KMRSUAPTLOWI.NJEF0Y,VG5/Q9ZH38B?427C1D6X";
byte morse[] = {
    // Each character is encoded into an 8-bit byte:
    0b01001010,  // ! exclamation
    0b01101101,  // " quotation
    0b01010111,  // # pound                   // No Morse, mapped to SK
    0b10110111,  // $ dollar or ~SX
    0b00000000,  // % percent
    0b00111101,  // & ampersand or ~AS
    0b01100001,  // ' apostrophe
    0b00110010,  // ( open paren
    0b01010010,  // ) close paren
    0b0,         // * asterisk                // No Morse
    0b00110101,  // + plus or ~AR
    0b01001100,  // , comma
    0b01011110,  // - hypen
    0b01010101,  // . period
    0b00110110,  // / slant
    0b00100000,  // 0                         // Read the bits from RIGHT to
                 // left,
    0b00100001,  // 1                         // with a "1"=dit and "0"=dah
    0b00100011,  // 2                         // example: 2 = 11000 or
                 // dit-dit-dah-dah-dah
    0b00100111,  // 3                         // the final bit is always 1 =
                 // stop bit.
    0b00101111,  // 4                         // see "sendElements" routine for
                 // more info.
    0b00111111,  // 5
    0b00111110,  // 6
    0b00111100,  // 7
    0b00111000,  // 8
    0b00110000,  // 9
    0b01111000,  // : colon
    0b01101010,  // ; semicolon
    0b0,         // <                         // No Morse
    0b00101110,  // = equals or ~BT
    0b0,         // >                         // No Morse
    0b01110011,  // ? question
    0b01101001,  // @ at or ~AC
    0b00000101,  // A
    0b00011110,  // B
    0b00011010,  // C
    0b00001110,  // D
    0b00000011,  // E
    0b00011011,  // F
    0b00001100,  // G
    0b00011111,  // H
    0b00000111,  // I
    0b00010001,  // J
    0b00001010,  // K
    0b00011101,  // L
    0b00000100,  // M
    0b00000110,  // N
    0b00001000,  // O
    0b00011001,  // P
    0b00010100,  // Q
    0b00001101,  // R
    0b00001111,  // S
    0b00000010,  // T
    0b00001011,  // U
    0b00010111,  // V
    0b00001001,  // W
    0b00010110,  // X
    0b00010010,  // Y
    0b00011100   // Z
};

int charSpeed = DEFAULTSPEED;  // speed at which characters are sent, in WPM
int codeSpeed = DEFAULTSPEED;  // overall code speed, in WPM
int ditPeriod = 100;           // length of a dit, in milliseconds
int ditPaddle = PADDLE_A;      // digital pin attached to dit paddle
int dahPaddle = PADDLE_B;      // digital pin attached to dah paddle
int pitch = DEFAULTPITCH;      // frequency of audio output, in Hz
int kochLevel = 1;             // current Koch lesson #
int score = 0;                 // copy challange score
int hits = 0;                  // copy challange correct #
int misses = 0;                // copy channange incorrect #
int xWordSpaces = 0;           // extra spaces between words
int keyerMode = IAMBIC_B;      // current keyer mode
bool usePaddles = false;       // if true, using paddles; if false, straight key
bool paused = false;           // if true, morse output is paused
bool ditRequest = false;       // dit memory for iambic sending
bool dahRequest = false;       // dah memory for iambic sending
bool inStartup = true;         // startup flag
char myCall[MAX_CALLSIGN_STRING] = DEFAULT_CALL;
int textColor = TEXTCOLOR;  // foreground (text) color
int bgColor = BG;           // background (screen) color
int brightness = 100;       // backlight level (range 0-100%)
int startItem = 0;          // startup activity.  0 = main menu

//===================================  Menu Variables
//===================================
int menuCol = 0, textRow = 0, textCol = 0;
char *mainMenu[] = {(char *)" Receive ", (char *)"  Send  ", (char *)"Config "};
char *menu0[] = {(char *)" Koch    ", (char *)" Letters ", (char *)" Words   ",
                 (char *)" Numbers ", (char *)" Mixed   ", (char *)" SD Card ",
                 (char *)" QSO     ", (char *)" Callsign", (char *)" Exit    "};
char *menu1[] = {(char *)" Practice", (char *)" Copy One", (char *)" Copy Two",
                 (char *)" Cpy Word", (char *)" Cpy Call", (char *)" Flashcrd",
                 (char *)" Head Cpy", (char *)" Two-Way ", (char *)" Exit    "};
char *menu2[] = {(char *)" Speed   ", (char *)" Chk Spd ", (char *)" Tone    ",
                 (char *)" Key     ", (char *)" Callsign", (char *)" Screen  ",
                 (char *)" Defaults", (char *)" CLI    ",  (char *)" Exit    "};

// Added by VE3OOI
unsigned char addr = DEFAULT_EEPROM_ADDRESS;
TUTOR_STRUT cfg;
char eebuf[DEFAULT_EEPROM_ADDRESS + sizeof(cfg)];
#ifndef REMOVE_CLI
extern char rbuff[RBUFF];
extern char commands[MAX_COMMAND_ENTRIES];
extern unsigned char command_entries;
extern unsigned long numbers[MAX_COMMAND_ENTRIES];
extern unsigned char ctr;
extern char prompt[6];
extern char ovflmsg[9];
extern char errmsg[4];

#define MAXIMUM_STRING_LENGTH 80
char line[MAXIMUM_STRING_LENGTH];

#endif

//===================================  Wireless Code
//===================================

void setStatusLED(int color) {
  const int size = 20;  // size of square LED indicator
  const int xPos = DISPLAYWIDTH - size,
            yPos = 0;                           // location = screen top-right
  tft.fillRect(xPos, yPos, size, size, color);  // fill it in with desired color
}

//===================================  Rotary Encoder Code
//=============================

/*
   Rotary Encoder Button Interrupt Service Routine ----------
   Process encoder button presses and releases, including debouncing (extra
   "presses" from noisy switch contacts). If button is pressed, the
   button_pressed flag is set to true. If button is released, the
   button_released flag is set to true, and button_downtime will contain the
   duration of the button press in ms.  Manually reset flags after handling
   event.
*/

void buttonISR() {
  static boolean button_state = false;
  static unsigned long start, end;
  boolean pinState = digitalRead(ENCODER_BUTTON);

  if ((pinState == LOW) &&
      (button_state == false)) {  // Button was up, but is now down
    start = millis();             // mark time of button down
    if (start > (end + 10))       // was button up for 10mS?
    {
      button_state = true;  // yes, so change state
      button_pressed = true;
    }
  } else if ((pinState == HIGH) &&
             (button_state == true)) {  // Button was down, but now up
    end = millis();                     // mark time of release
    if (end > (start + 10))             // was button down for 10mS?
    {
      button_state = false;  // yes, so change state
      button_released = true;
      button_downtime = end - start;  // and record how long button was down
    }
  }
}

/*
   Rotary Encoder Interrupt Service Routine ---------------
   This is an alternative to rotaryISR() above.
   It gives twice the resolution at a cost of using an additional interrupt line
   This function will runs when either encoder pin A or B changes state.
   The states array maps each transition 0000..1111 into CW/CCW rotation (or
   invalid). The rotary "position" is held in rotary_counter, increasing for CW
   rotation, decreasing for CCW rotation. If the position changes, rotary_change
   will be set true. You should set this to false after handling the change. To
   implement, attachInterrupts to encoder pin A *and* pin B
*/

void rotaryISR() {
  const int states[] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};
  static byte transition = 0;  // holds current and previous encoder states
  transition <<= 2;            // shift previous state up 2 bits
  transition |= (digitalRead(ENCODER_A));       // put encoder_A on bit 0
  transition |= (digitalRead(ENCODER_B) << 1);  // put encoder_B on bit 1
  transition &= 0x0F;                           // zero upper 4 bits
  rotaryCounter +=
      states[transition];  // update counter +/- 1 based on rotation
}

boolean buttonDown()  // check CURRENT state of button
{
  return (digitalRead(ENCODER_BUTTON) == LOW);
}

void waitForButtonRelease() {
  if (buttonDown())  // make sure button is currently pressed.
  {
    button_released = false;  // reset flag
    while (!button_released)
      ;  // and wait for release
  }
}

void waitForButtonPress() {
  if (!buttonDown())  // make sure button is not pressed.
  {
    while (!button_pressed)
      ;                      // wait for press
    button_pressed = false;  // and reset flag
  }
}

bool longPress() {
  waitForButtonRelease();                // wait until button is released
  return (button_downtime > LONGPRESS);  // then check time is was held down
}

/*
   readEncoder() returns 0 if no significant encoder movement since last call,
   +1 if clockwise rotation, and -1 for counter-clockwise rotation
*/

int readEncoder(int numTicks = ENCODER_TICKS) {
  static int prevCounter = 0;                // holds last encoder position
  int change = rotaryCounter - prevCounter;  // how many ticks since last call?
  if (abs(change) <= numTicks)               // not enough ticks?
    return 0;                                // so exit with a 0.
  prevCounter = rotaryCounter;  // enough clicks, so save current counter values
  return (change > 0) ? 1 : -1;  // return +1 for CW rotation, -1 for CCW
}

//===================================  Morse Routines
//===================================

void keyUp()             // device-dependent actions
{                        // when key is up:
  digitalWrite(LED, 0);  // turn off LED
  ledcWrite(0, 0);       // and turn off sound
}

void keyDown()                             // device-dependent actions
{                                          // when key is down:
  if (!SUPPRESSLED) digitalWrite(LED, 1);  // turn on LED
  ledcWriteTone(0, pitch);                 // and turn on sound
}

bool ditPressed() {
  return (digitalRead(ditPaddle) == 0);  // pin is active low
}

bool dahPressed() {
  return (digitalRead(dahPaddle) == 0);  // pin is active low
}

int extracharDit() { return (3158 / codeSpeed) - (1958 / charSpeed); }

int intracharDit() { return (1200 / charSpeed); }

void characterSpace() {
  int fudge =
      (charSpeed / codeSpeed) - 1;  // number of fast dits needed to convert
  delay(fudge * ditPeriod);   // single intrachar dit to slower extrachar dit
  delay(2 * extracharDit());  // 3 total (last element includes 1)
}

void wordSpace() {
  delay(4 * extracharDit());  // 7 total (last char includes 3)
  if (xWordSpaces)            // any user-specified delays?
    delay(7 * extracharDit() *
          xWordSpaces);  // yes, so additional wait between words
}

void dit() {                            // send a dit
  ditRequest = false;                   // clear any pending request
  keyDown();                            // turN on sound & led
  int finished = millis() + ditPeriod;  // wait for duration of dit
  while (millis() < finished) {         // check dah paddle while dit sounding
    if ((keyerMode == IAMBIC_B) &&
        dahPressed())     // if iambic B & dah was pressed,
      dahRequest = true;  // request it for next element
  }
  keyUp();                          // turn off sound & led
  finished = millis() + ditPeriod;  // wait before next element sent
  while (millis() < finished) {     // check dah paddle while waiting
    if (keyerMode && dahPressed())  // if iambic A or B & dah was pressed,
      dahRequest = true;            // request it for next element
  }
}

void dah() {                                // send a dah
  dahRequest = false;                       // clear any pending request
  keyDown();                                // turn on sound & led
  int finished = millis() + ditPeriod * 3;  // wait for duration of dah
  while (millis() < finished) {  // check dit paddle while dah sounding
    if ((keyerMode == IAMBIC_B) &&
        ditPressed())     // if iambic B & dit was pressed
      ditRequest = true;  // request it for next element
  }
  keyUp();                          // turn off sound & led
  finished = millis() + ditPeriod;  // wait before next element sent
  while (millis() < finished) {     // check dit paddle while waiting
    if (keyerMode && ditPressed())  // if iambic A or B & dit was pressed,
      ditRequest = true;            // request it for next element
  }
}

void sendElements(int x) {  // send string of bits as Morse
  while (x > 1) {           // stop when value is 1 (or less)
    if (x & 1)
      dit();  // right-most bit is 1, so dit
    else
      dah();  // right-most bit is 0, so dah
    x >>= 1;  // rotate bits right
  }
  characterSpace();  // add inter-character spacing
}

void roger() {
  dit();
  dah();
  dit();  // audible (only) confirmation
}

/*
   The following routine accepts a numberic input, where each bit represents a
   code element: 1=dit and 0=dah.   The elements are read right-to-left.  The
   left-most bit is a stop bit. For example:  5 = binary 0b0101.  The right-most
   bit is 1 so the first element is a dit. The next element to the left is a 0,
   so a dah follows.  Only a 1 remains after that, so the character is complete:
   dit-dah = morse character 'A'.
*/

void sendCharacter(char c) {   // send a single ASCII character in Morse
  if (button_pressed) return;  // user wants to quit, so vamoose
  if (c < 32) return;          // ignore control characters
  if (c > 96) c -= 32;         // convert lower case to upper case
  if (c > 90) return;          // not a character
  addCharacter(c);             // display character on LCD
  if (c == 32)
    wordSpace();  // space between words
  else
    sendElements(morse[c - 33]);  // send the character
  checkForSpeedChange();          // allow change in speed while sending
  do {
    checkPause();
  } while (paused);  // allow user to pause morse output
}

void sendString(char *ptr) {
  while (*ptr)              // send the entire string
    sendCharacter(*ptr++);  // one character at a time
}

void sendMorseWord(char *ptr) {
  while (*ptr)                         // send the entire word
    sendElements(morse[*ptr++ - 33]);  // each char in Morse - no display
}

void displayWPM() {
  const int x = 290, y = 200;
  tft.fillRect(x, y, 24, 20, BLACK);
  tft.setCursor(x, y);
  tft.print(codeSpeed);
}

void checkForSpeedChange() {
  int dir = readEncoder();
  if (dir != 0) {
    bool farnsworth = (codeSpeed != charSpeed);
    codeSpeed += dir;
    if (codeSpeed < MINSPEED) codeSpeed = MINSPEED;
    if (farnsworth) {
      if (codeSpeed >= charSpeed) codeSpeed = charSpeed - 1;
    } else {
      if (codeSpeed > MAXSPEED) codeSpeed = MAXSPEED;
      charSpeed = codeSpeed;
    }
    displayWPM();
    ditPeriod = intracharDit();
  }
}

void checkPause() {
  bool userKeyed =
      (ditPressed() ^ dahPressed());  // did user press a key (not both)
  if (!paused && userKeyed)           // was it during sending?
  {
    paused = true;                  // yes, so pause output
    delay(ditPeriod * 10);          // for at least for 10 dits
  } else if ((paused && userKeyed)  // did user key during pause
             || button_pressed)     // or cancel?
    paused = false;                 // yes, so resume output
}

//===================================  Koch Method
//=====================================

void sendKochLesson(int lesson)  // send letter/number groups...
{
  const int maxCount = 175;  // full screen = 20 x 9
  int charCount = 0;
  newScreen();                                       // start with empty screen
  while (!button_pressed && (charCount < maxCount))  // full screen = 1 lesson
  {
    for (int i = 0; i < WORDSIZE; i++)  // break them up into "words"
    {
      int c = koch[random(lesson + 1)];  // pick a random character
      sendCharacter(c);                  // and send it
      charCount++;                       // keep track of #chars sent
    }
    sendCharacter(' ');  // send a space between words
  }
}

void introLesson(int lesson)  // helper fn for getLessonNumber()
{
  newScreen();  // start with clean screen
  tft.print((char *)"You are in lesson ");
  tft.println(lesson);  // show lesson number
  tft.println((char *)"\nCharacters: ");
  tft.setTextColor(CYAN);
  for (int i = 0; i <= lesson; i++)  // show characters in this lession
  {
    tft.print(koch[i]);
    tft.print(" ");
  }
  tft.setTextColor(textColor);
  tft.println((char *)"\n\nPress <dit> to begin");
}

int getLessonNumber() {
  int lesson = kochLevel;  // start at current level
  introLesson(lesson);     // display lesson number
  while (!button_pressed && !ditPressed()) {
    int dir = readEncoder();
    if (dir != 0)  // user rotated encoder knob:
    {
      lesson += dir;                               // ...so change speed up/down
      if (lesson < 1) lesson = 1;                  // dont go below 1
      if (lesson > kochLevel) lesson = kochLevel;  // dont go above maximum
      introLesson(lesson);                         // show new lesson number
    }
  }
  return lesson;
}

void sendKoch() {
  while (!button_pressed) {
    setTopMenu((char *)"Koch lesson");
    int lesson = getLessonNumber();  // allow user to select lesson
    if (button_pressed) return;      // user quit, so sad
    sendKochLesson(lesson);          // do the lesson
    setTopMenu((char *)"Get 90%? Dit=YES, Dah=NO");  // ask user to score lesson
    while (!button_pressed) {                        // wait for user response
      if (ditPressed())  // dit = user advances to next level
      {
        roger();                                      // acknowledge success
        if (kochLevel < ELEMENTS(koch)) kochLevel++;  // advance to next level
        saveConfig();                                 // save it in EEPROM
        delay(1000);  // give time for user to release dit
        break;        // go to next lesson
      }
      if (dahPressed()) break;  // dah = repeat same lesson
    }
  }
}

//===================================  Receive Menu
//====================================

void addChar(char *str, char ch)  // adds 1 character to end of string
{
  char c[2] = " ";  // happy hacking: char into string
  c[0] = ch;        // change char 'A' to string "A"
  strcat(str, c);   // and add it to end of string
}

char randomLetter()  // returns a random uppercase letter
{
  return 'A' + random(0, 26);
}

char randomNumber()  // returns a random single-digit # 0-9
{
  return '0' + random(0, 10);
}

void randomCallsign(char *call)  // returns with random US callsign in "call"
{
  strcpy(call, "");            // start with empty callsign
  int i = random(0, 4);        // 4 possible start letters for US
  char c = prefix[i];          // Get first letter of prefix
  addChar(call, c);            // and add it to callsign
  i = random(0, 3);            // single or double-letter prefix?
  if ((i == 2) or (c == 'A'))  // do a double-letter prefix
  {                            // allowed combinations are:
    if (c == 'A')
      i = random(0, 12);  // AA-AL, or
    else
      i = random(0, 26);     // KA-KZ, NA-NZ, WA-WZ
    addChar(call, 'A' + i);  // add second char to prefix
  }
  addChar(call, randomNumber());          // add zone number to callsign
  for (int i = 0; i < random(1, 4); i++)  // Suffix contains 1-3 letters
    addChar(call, randomLetter());        // add suffix letter(s) to call
}

void randomRST(char *rst) {
  strcpy(rst, "");                    // start with empty string
  addChar(rst, '0' + random(3, 6));   // readability 3-5
  addChar(rst, '0' + random(5, 10));  // strength: 6-9
  addChar(rst, '9');                  // tone usually 9
}

void sendNumbers() {
  while (!button_pressed) {
    for (int i = 0; i < WORDSIZE; i++)  // break them up into "words"
      sendCharacter(randomNumber());    // send a number
    sendCharacter(' ');                 // send a space between number groups
  }
}

void sendLetters() {
  while (!button_pressed) {
    for (int i = 0; i < WORDSIZE; i++)  // group letters into "words"
      sendCharacter(randomLetter());    // send the letter
    sendCharacter(' ');                 // send a space between words
  }
}

void sendMixedChars()  // send letter/number groups...
{
  while (!button_pressed) {
    for (int i = 0; i < WORDSIZE; i++)  // break them up into "words"
    {
      int c = '0' + random(43);  // pick a random character
      sendCharacter(c);          // and send it
    }
    sendCharacter(' ');  // send a space between words
  }
}

void sendPunctuation() {
  while (!button_pressed) {
    for (int i = 0; i < WORDSIZE; i++)  // break them up into "words"
    {
      int j = random(0, ELEMENTS(punctuation));  // select a punctuation char
      sendCharacter(punctuation[j]);             // and send it.
    }
    sendCharacter(' ');  // send a space between words
  }
}

void sendWords() {
  while (!button_pressed) {
    int index = random(0, ELEMENTS(words));  // eeny, meany, miney, moe
    sendString(words[index]);                // send the word
    sendCharacter(' ');                      // and a space between words
  }
}

void sendCallsigns()  // send random US callsigns
{
  char call[8];  // need string to stuff callsign into
  while (!button_pressed) {
    randomCallsign(call);  // make it
    sendString(call);      // and send it
    sendCharacter(' ');    // with a space between callsigns
  }
}

void sendQSO() {
  char otherCall[8];
  char temp[20];
  char qso[300];  // string to hold entire QSO
  randomCallsign(otherCall);
  strcpy(qso, myCall);  // start of QSO
  strcat(qso, " de ");
  strcat(qso, otherCall);  // another ham is calling you
  strcat(qso, " K  TNX FER CALL= UR RST ");
  randomRST(temp);  // add RST
  strcat(qso, temp);
  strcat(qso, " ");
  strcat(qso, temp);
  strcat(qso, "= NAME HERE IS ");
  strcpy(temp, names[random(0, ELEMENTS(names))]);
  strcat(qso, temp);  // add name
  strcat(qso, " ? ");
  strcat(qso, temp);  // add name again
  strcat(qso, "= QTH IS ");
  strcpy(temp, cities[random(0, ELEMENTS(cities))]);
  strcat(qso, temp);  // add QTH
  strcat(qso, "= RIG HR IS ");
  strcpy(temp, rigs[random(0, ELEMENTS(rigs))]);  // add rig
  strcat(qso, temp);
  strcat(qso, " ES ANT IS ");  // add antenna
  strcat(qso, antenna[random(0, ELEMENTS(antenna))]);
  strcat(qso, "== WX HERE IS ");  // add weather
  strcat(qso, weather[random(0, ELEMENTS(weather))]);
  strcat(qso, "= SO HW CPY? ");  // back to other ham
  strcat(qso, myCall);
  strcat(qso, " de ");
  strcat(qso, otherCall);
  strcat(qso, " KN");
  sendString(qso);  // send entire QSO
}

int getFileList(char list[][FNAMESIZE])  // gets list of files on SD card
{
  File root = SD.open("/");  // open root directory on the SD card
  int count = 0;             // count the number of files
  while (count < MAXFILES)   // only room for so many!
  {
    File entry = root.openNextFile();  // get next file in the SD root directory
    if (!entry) break;                 // leave if there aren't any more
    if (!entry.isDirectory() &&        // ignore directory names
        (entry.name()[0] != '_'))      // ignore hidden "_name" Mac files

      // Modified by VE3OOI.  Removed entry.name()[1] so that full file name can
      // be retrieved
      strcpy(list[count++],
             entry.name());  // add SD file to the list (ESP32: remove '/')
    entry.close();           // close the file
  }
  root.close();
  return count;
}

void displayFiles(char menu[][FNAMESIZE], int top, int itemCount) {
  int x = 30;                       // x-coordinate of this menu
  newScreen();                      // clear screen below menu
  for (int i = 0; i < MAXROW; i++)  // for all items in the frame
  {
    int y = TOPMARGIN + i * ROWSPACING;  // calculate y coordinate
    int item = top + i;
    if (item < itemCount)                      // make sure item exists
      showMenuItem(menu[item], x, y, FG, BG);  // and show the item.
  }
}

int fileMenu(char menu[][FNAMESIZE],
             int itemCount)  // Display list of files & get user selection
{
  int index = 0, top = 0, pos = 0, x = 30, y;
  button_pressed = false;              // reset button flag
  displayFiles(menu, 0, itemCount);    // display as many files as possible
  showMenuItem(menu[0], x, TOPMARGIN,  // highlight first item
               SELECTFG, SELECTBG);
  while (!button_pressed)  // exit on button press
  {
    int dir = readEncoder();  // check for encoder movement
    if (dir)                  // it moved!
    {
      if ((dir > 0) &&
          (index == (itemCount - 1)))  // dont try to go below last item
        continue;
      if ((dir < 0) && (index == 0))  // dont try to go above first item
        continue;
      if ((dir > 0) &&
          (pos == (MAXROW - 1)))  // does the frame need to move down?
      {
        top++;                               // yes: move frame down,
        displayFiles(menu, top, itemCount);  // display it,
        index++;                             // and select next item
      } else if ((dir < 0) && (pos == 0))    // does the frame need to move up?
      {
        top--;                               // yes: move frame up,
        displayFiles(menu, top, itemCount);  // display it,
        index--;                             // and select previous item
      } else  // we must be moving within the frame
      {
        y = TOPMARGIN + pos * ROWSPACING;  // calc y-coord of current item
        showMenuItem(menu[index], x, y, FG, BG);  // deselect current item
        index += dir;                             // go to next/prev item
      }
      pos = index - top;  // posn of selected item in visible list
      y = TOPMARGIN + pos * ROWSPACING;  // calc y-coord of new item
      showMenuItem(menu[index], x, y, SELECTFG, SELECTBG);  // select new item
    }
  }
  return index;
}

void sendFile(char *filename)  // output a file to screen & morse
{
  char s[FNAMESIZE] = "/";   // ESP32: need space for whole filename
  strcat(s, filename);       // ESP32: prepend filename with slash
  const int pageSkip = 250;  // number of characters to skip, if asked to
  newScreen();               // clear screen below menu

  // Notes by VE3OOI.  Sending file contends via network is a bad idea.  Can
  // result in a buffer overflow
  bool wireless = longPress();   // if long button press, send file wirelessly
  if (wireless) initWireless();  // start wireless
  if (!(cfg.conflag & SRV_CONNECTED)) {
    tft.print((char *)"Wifi Err");
    closeWireless();
    wireless = false;
  }
  //    transmission
  button_pressed = false;  // reset flag for new presses
  File book = SD.open(s);  // look for book on sd card
  if (book) {              // find it?
    while (!button_pressed &&
           book.available())  // do for all characters in book:
    {
      char ch = book.read();     // get next character
      if (ch == '\n') ch = ' ';  // convert LN to a space
      sendCharacter(ch);         // and send it

      // Notes by VE3OOI.  Sending file contends via network is a bad idea.  Can
      // result in a buffer overflow
      if (wireless) sendWireless(ch);
      if (ditPressed() && dahPressed())  // user wants to 'skip' ahead:
      {
        sendString((char *)"= ");  // acknowledge the skip with ~BT
        for (int i = 0; i < pageSkip; i++)
          book.read();  // skip a bunch of text!
      }
    }
    book.close();  // close the file
  }
  // Notes by VE3OOI.  Sending file contends via network is a bad idea.  Can
  // result in a buffer overflow
  if (wireless) closeWireless();  // close wireless transmission
}

void sendFromSD()  // show files on SD card, get user selection & send it.
{
  char list[MAXFILES][FNAMESIZE];      // hold list of SD filenames (DOS 8.3
                                       // format, 13 char)
  int count = getFileList(list);       // get list of files on the SD card
  int choice = fileMenu(list, count);  // display list & let user choose one
  sendFile(list[choice]);              // output text & morse until user quits
}

//===================================  Send Menu
//=======================================

void checkSpeed() {
  long start = millis();
  sendString((char *)"PARIS ");     // send "PARIS"
  long elapsed = millis() - start;  // see how long it took
  dit();                            // sound out the end.
  float wpm = 60000.0 / elapsed;    // convert time to WPM
  tft.setTextSize(3);
  tft.setCursor(100, 80);
  tft.print(wpm);  // display result
  tft.print((char *)" WPM");
  while (!button_pressed)
    ;  // wait for user
}

int decode(int code)  // convert code to morse table index
{
  int i = 0;
  while (i < ELEMENTS(morse) &&
         (morse[i] != code))  // search table for the code
    i++;                      // ...from beginning to end.
  if (i < ELEMENTS(morse))
    return i;  // found the code, so return index
  else
    return -1;  // didn't find the code, return -1
}

char paddleInput()  // monitor paddles & return a decoded char
{
  int bit = 0, code = 0;
  unsigned long start = millis();
  while (!button_pressed) {
    if (ditRequest ||                   // dit was requested
        (ditPressed() && !dahRequest))  // or user now pressing it
    {
      dit();                 // so sound it out
      code += (1 << bit++);  // add a '1' element to code
      start = millis();      // and reset timeout.
    }
    if (dahRequest ||                   // dah was requested
        (dahPressed() && !ditRequest))  // or user now pressing it
    {
      dah();             // so sound it tout
      bit++;             // add '0' element to code
      start = millis();  // and reset the timeout.
    }
    int wait = millis() - start;
    if (bit && (wait > ditPeriod))  // waited for more than a dit
    {                               // so that must be end of character:
      code += (1 << bit);           // add stop bit
      int result = decode(code);    // look up code in morse array
      if (result < 0)
        return ' ';  // oops, didn't find it
      else
        return '!' + result;  // found it! return result
    }
    if (wait > 5 * ditPeriod) return ' ';  // long pause = word space
  }
  return ' ';  // return on button press
}

char straightKeyInput()  // decode straight key input
{
  int bit = 0, code = 0;
  bool keying = false;
  unsigned long start, end, timeUp, timeDown, timer;
  timer = millis();  // start character timer
  while (!button_pressed) {
    if (ditPressed()) timer = millis();  // dont count keydown time
    if (ditPressed() && (!keying))       // new key_down event
    {
      start = millis();  // mark time of key down
      timeUp = start - end;
      if (timeUp > 10)  // was key up for 10mS?
      {
        keying = true;  // mark key as down
        keyDown();      // turn on sound & led
      }
    } else if (!ditPressed() && keying)  // new key_up event
    {
      end = millis();          // get time of release
      timeDown = end - start;  // how long was key down?
      if (timeDown > 10)       // was key down for 10mS?
      {
        keying = false;                // not just noise: mark key as up
        keyUp();                       // turn off sound & led
        if (timeDown > 2 * ditPeriod)  // if longer than 2 dits, call it dah
          bit++;                       // dah: add '0' element to code
        else
          code += (1 << bit++);  // dit: add '1' element to code
      }
    }
    int wait = millis() - timer;        // time since last element was sent
    if (bit && (wait > ditPeriod * 2))  // waited for more than 2 dits
    {                                   // so that must be end of character:
      code += (1 << bit);               // add stop bit
      int result = decode(code);        // look up code in morse array
      if (result < 0)
        return ' ';  // oops, didn't find it
      else
        return '!' + result;  // found it! return result
    }
    if (wait > ditPeriod * 5) return ' ';  // long pause = word space
  }
  return ' ';  // return on button press
}

char morseInput()  // get & decode user input from key
{
  if (usePaddles)
    return paddleInput();  // it can be either paddle input
  else
    return straightKeyInput();  // or straight key, depending on setting
}

void practice()  // get Morse from user & display it
{
  char oldCh = ' ';
  while (!button_pressed) {
    char ch = morseInput();                // get a morse character from user
    if (!((ch == ' ') && (oldCh == ' ')))  // only 1 word space at a time.
      addCharacter(ch);                    // show character on display
    oldCh = ch;                            // and remember it
  }
}

void copyCallsigns()  // show a callsign & see if user can copy it
{
  char call[8];
  while (!button_pressed) {
    randomCallsign(call);  // make a random callsign
    mimic(call);
  }
}

void copyOneChar() {
  char text[8];
  while (!button_pressed) {
    strcpy(text, "");               // start with empty string
    addChar(text, randomLetter());  // add a random letter
    mimic(text);                    // compare it to user input
  }
}

void copyTwoChars() {
  char text[8];
  while (!button_pressed) {
    strcpy(text, "");               // start with empty string
    addChar(text, randomLetter());  // add a random letter
    addChar(text, randomLetter());  // make that two random letters
    mimic(text);                    // compare it to user input
  }
}

void copyWords()  // show a callsign & see if user can copy it
{
  char text[10];
  while (!button_pressed) {
    int index = random(0, ELEMENTS(words));  // eeny, meany, miney, moe
    strcpy(text, words[index]);              // pick a random word
    mimic(text);                             // and ask user to copy it
  }
}

void encourageUser()  // helper fn for showScore()
{
  char *phrases[] = {
      (char *)"Good Job", (char *)"Keep Going",  // list of phrases to display
      (char *)"Amazing", (char *)"Dont Stop", (char *)"Impressive"};
  if (score == 0) return;  // 0 is not an encouraging score
  if (score % 25) return;  // set interval at every 25 points
  textCol = 0;
  textRow = 6;                                // place display below challange
  int choice = random(0, ELEMENTS(phrases));  // pick a random message
  sendString(phrases[choice]);                // show it and send it
}

void displayNumber(int num, int color,  // show large number on screen
                   int x, int y, int wd,
                   int ht)  // specify x,y and width,height of box
{
  int textSize = (num > 99) ? 5 : 6;   // use smaller text for 3 digit scores
  tft.setCursor(x + 15, y + 20);       // position text within colored box
  tft.setTextSize(textSize);           // use big text,
  tft.setTextColor(BLACK, color);      // in inverted font,
  tft.fillRect(x, y, wd, ht, color);   // on selected background
  tft.print(num);                      // show the number
  tft.setTextSize(2);                  // resume usual size
  tft.setTextColor(textColor, BLACK);  // resume usual colors
}

void showScore()  // helper fn for mimic()
{
  const int x = 200, y = 50, wd = 105, ht = 80;  // posn & size of scorecard
  int bkColor = (score > 0) ? GREEN : RED;  // back-color green unless score 0
  displayNumber(score, bkColor, x, y, wd, ht);  // show it!
  encourageUser();  // show encouraging message periodically
}

void mimic1(char *text) {
  char ch, response[20];
  textRow = 1;
  textCol = 6;           // set position of text
  sendString(text);      // display text & morse it
  strcpy(response, "");  // start with empty response
  textRow = 2;
  textCol = 6;                             // set position of response
  while (!button_pressed && !ditPressed()  // wait until user is ready
         && !dahPressed())
    ;
  do {                                     // user has started keying...
    ch = morseInput();                     // get a character
    if (ch != ' ') addChar(response, ch);  // add it to the response
    addCharacter(ch);                      // and put it on screen
  } while (ch != ' ');                     // space = word timeout
  if (button_pressed) return;              // leave without scoring
  if (!strcmp(text, response))             // did user match the text?
    score++;                               // yes, so increment score
  else
    score = 0;            // no, so reset score to 0
  showScore();            // display score for user
  delay(FLASHCARDDELAY);  // wait between attempts
  newScreen();            // erase screen for next attempt
}

void showHitsAndMisses(int hits, int misses)  // helper fn for mimic2()
{
  const int x = 200, y1 = 50, y2 = 140, wd = 105,
            ht = 80;                          // posn & size of scorecard
  displayNumber(hits, GREEN, x, y1, wd, ht);  // show the hits in green
  displayNumber(misses, RED, x, y2, wd, ht);  // show misses in red
}

void headCopy()  // show a callsign & see if user can copy it
{
  char text[10];
  while (!button_pressed) {
    int index = random(0, ELEMENTS(words));  // eeny, meany, miney, moe
    strcpy(text, words[index]);              // pick a random word
    mimic2(text);                            // and ask user to copy it
  }
}

void hitTone() {
  ledcWriteTone(0, 440);
  delay(150);  // first tone
  ledcWriteTone(0, 600);
  delay(200);       // second tone
  ledcWrite(0, 0);  // audio off
}

void missTone() {
  ledcWriteTone(0, 200);
  delay(200);       // single tone
  ledcWrite(0, 0);  // audio off
}

void mimic2(char *text)  // used by head-copy feature
{
  char ch, response[20];
  bool correct, leave;
  do {                     // repeat same word until correct
    sendMorseWord(text);   // morse the text, NO DISPLAY
    strcpy(response, "");  // start with empty response
    textRow = 2;
    textCol = 6;                             // set position of response
    while (!button_pressed && !ditPressed()  // wait until user is ready
           && !dahPressed())
      ;
    do {                                     // user has started keying...
      ch = morseInput();                     // get a character
      if (ch != ' ') addChar(response, ch);  // add it to the response
      addCharacter(ch);                      // and put it on screen
    } while (ch != ' ');                     // space = word timeout
    if (button_pressed) return;              // leave without scoring
    correct = !strcmp(text, response);       // did user match the text?
    leave = !strcmp(response, "=");          // user entered BT/break to skip
    if (correct) {                           // did user match the text?
      hits++;                                // got it right!
      hitTone();
    } else {
      misses++;  // user muffed it
      missTone();
    }
    showHitsAndMisses(hits, misses);       // display scores for user
    delay(1000);                           // wait a sec, then
    tft.fillRect(50, 50, 120, 80, BLACK);  // erase answer
  } while (!correct && !leave);  // repeat until correct or user breaks
}

void mimic(char *text) {
  if (button_downtime > LONGPRESS)  // did user hold button down > 1 sec?
    mimic2(text);                   // yes, so use alternate scoring mode
  else
    mimic1(text);  // no, so use original scoring mode
}

void flashcards() {
  tft.setTextSize(7);
  while (!button_pressed) {
    int index = random(0, ELEMENTS(morse));  // get a random character
    int code = morse[index];                 // convert to morse code
    if (!code) continue;                     // only do valid morse chars
    sendElements(code);                      // sound it out
    delay(1000);                             // wait for user to guess
    tft.setCursor(120, 70);
    tft.print(char('!' + index));  // show the answer
    delay(FLASHCARDDELAY);         // wait a little
    newScreen();                   // and start over.
  }
}

void twoWay()  // wireless QSO between units
{
  initWireless();  // look for another unit & connect
  if (!(cfg.conflag & SRV_CONNECTED)) {
    tft.print((char *)"Network Err");
    closeWireless();
    delay(5000);
    return;
  }
  char oldCh = ' ';
  while (!button_pressed) {
    processMQTT();
    char ch = morseInput();                // get a morse character from user
    if (!((ch == ' ') && (oldCh == ' ')))  // only 1 word space at a time.
    {
      tft.setTextColor(TXCOLOR);
      addCharacter(ch);  // show character on display
      sendWireless(ch);  // send it to other device
    }
    oldCh = ch;  // and remember it

    while (ch = deQueue())  // any characters to receive?
    {
      processMQTT();  // Address potential buffer overflow - clear any
                      // received characters
      tft.setTextColor(RXCOLOR);  // change text color
      sendCharacter(ch);          // sound it out and show it.
    }
  }
  tft.setTextColor(TEXTCOLOR);
  closeWireless();
}

//===================================  Config Menu
//=====================================

// Added by VE3OOI
void clearMem(void) {
  memset(eebuf, 0, sizeof(eebuf));
  EEPROM.writeBytes(0, eebuf, sizeof(eebuf));
  EEPROM.commit();
}

void dumpMem(void) {
  memset(eebuf, 0, sizeof(eebuf));
  EEPROM.readBytes(0, eebuf, sizeof(eebuf));
  Serial.print("EEPROM Dump for ");
  Serial.print(sizeof(eebuf));
  Serial.println(" bytes");

  Serial.println("Addr\tHex\tDec\tASCII");
  for (int i = 0; i < sizeof(eebuf); i++) {
    Serial.print(i);
    Serial.print("\t");
    Serial.print(eebuf[i], HEX);
    Serial.print("\t");
    Serial.print((unsigned char)eebuf[i]);
    Serial.print("\t");
    Serial.print((char)eebuf[i]);
    Serial.println();
  }
}

void initializeMem(void) {
  Serial.println("Initializing cfg and mem");
  memset((char *)&cfg, 0, sizeof(cfg));
  cfg.flag = INIT_FLAG;
  cfg.charSpeed = DEFAULTSPEED;
  cfg.codeSpeed = DEFAULTSPEED;
  cfg.pitch = DEFAULTPITCH;
  cfg.usePaddles = false;
  cfg.ditPaddle = PADDLE_A;
  cfg.dahPaddle = PADDLE_B;
  cfg.kochLevel = 1;
  cfg.xWordSpaces = 0;

  cfg.keyerMode = IAMBIC_B;
  cfg.startItem = 0;
  cfg.brightness = 100;
  cfg.textColor = TEXTCOLOR;
  cfg.bgColor = BG;

  // Make sure that strings are less than MAX_CHAR_STRING or MAX_SSID_STRING
  // else bad things happen
  // can hardcode default here instead of running CLI to define
  cfg.conflag = 0;

  strcpy(cfg.myCall, DEFAULT_CALL);
  strcpy(cfg.wifi_ssid, DEFAULT_SSID);
  strcpy(cfg.wifi_password, DEFAULT_WIFI_PASSWORD);
  strcpy(cfg.mqtt_userid, DEFAULT_MQTT_USERNAME);
  strcpy(cfg.mqtt_password, DEFAULT_MQTT_PASSWORD);
  strcpy(cfg.mqtt_server, DEFAULT_SERVER_ADDRESS);
  strcpy(cfg.room, DEFAULT_MQTT_ROOM);
  setRunningConfig();
  saveConfig();
}

// Modified by VE3OOI
void printConfig(unsigned char ee)  // debugging only; not called
{
  TUTOR_STRUT cfgbck;

  if (ee) {
    memset((char *)&cfgbck, 0, sizeof(cfgbck));
    memcpy((char *)&cfgbck, (char *)&cfg,
           sizeof(cfgbck));  // backup running config
    loadConfig();
    if (cfg.flag != INIT_FLAG) {
      return;
    }
  }
  Serial.print("\r\nEEPROM Config at Addr: ");
  Serial.print(addr);
  Serial.print(" size: ");
  Serial.println(sizeof(cfg));
  Serial.print("  flag: ");
  Serial.println(cfg.flag);
  Serial.print("  charSpeed: ");
  Serial.println(cfg.charSpeed);
  Serial.print("  codeSpeed: ");
  Serial.println(cfg.codeSpeed);
  Serial.print("  pitch: ");
  Serial.println(cfg.pitch);
  Serial.print("  ditPaddle: ");
  Serial.println(cfg.ditPaddle);
  Serial.print("  kochLevel: ");
  Serial.println(cfg.kochLevel);
  Serial.print("  usePaddles");
  Serial.println(cfg.usePaddles);
  Serial.print("  xWordSpaces: ");
  Serial.println(cfg.xWordSpaces);
  Serial.print("  myCall: ");
  Serial.println(cfg.myCall);
  Serial.print("  keyerMode: ");
  Serial.println(cfg.keyerMode);
  Serial.print("  startItem: ");
  Serial.println(cfg.startItem);
  Serial.print("  brightness: ");
  Serial.println(cfg.brightness);
  Serial.print("  textColor: ");
  Serial.println(cfg.textColor);
  Serial.print("  bgColor: ");
  Serial.println(cfg.bgColor);
  Serial.print("  Connection Flag: ");
  Serial.println(cfg.conflag);
  Serial.print("  wifi_ssid: ");
  Serial.println(cfg.wifi_ssid);
  Serial.print("  wifi_password: ");
  Serial.println(cfg.wifi_password);
  Serial.print("  mqtt_userid: ");
  Serial.println(cfg.mqtt_userid);
  Serial.print("  mqtt_password: ");
  Serial.println(cfg.mqtt_password);
  Serial.print("  mqtt_server: ");
  Serial.println(cfg.mqtt_server);
  Serial.print("  room: ");
  Serial.println(cfg.room);

  if (ee) {  // loadConfig sets running config, need to restore back to original
    memcpy((char *)&cfg, (char *)&cfgbck,
           sizeof(cfg));  // restore running config
    setRunningConfig();
  }
}

// Modified by VE3OOI
void saveConfig(void) {
  cfg.charSpeed = charSpeed;
  cfg.codeSpeed = codeSpeed;
  cfg.pitch = pitch;  // Don't need to store this as a byte.
  cfg.ditPaddle = ditPaddle;
  cfg.kochLevel = kochLevel;
  cfg.usePaddles = usePaddles;
  cfg.xWordSpaces = xWordSpaces;
  strncpy(cfg.myCall, myCall, sizeof(cfg.myCall));
  cfg.keyerMode = cfg.keyerMode;
  cfg.startItem = startItem;
  cfg.brightness = brightness;
  cfg.textColor = textColor;  // and color low byte
  cfg.bgColor = bgColor;

  EEPROM.writeBytes(addr, (char *)&cfg, sizeof(cfg));
  EEPROM.commit();
}

void setRunningConfig(void) {
  charSpeed = cfg.charSpeed;
  codeSpeed = cfg.codeSpeed;
  pitch = cfg.pitch;
  ditPaddle = cfg.ditPaddle;
  kochLevel = cfg.kochLevel;
  usePaddles = cfg.usePaddles;
  xWordSpaces = cfg.xWordSpaces;
  strncpy(myCall, cfg.myCall, sizeof(myCall));
  keyerMode = cfg.keyerMode;
  startItem = cfg.startItem;
  brightness = cfg.brightness;
  textColor = cfg.textColor;  // and color low byte
  bgColor = cfg.bgColor;

  checkConfig();  // ensure loaded settings are valid
}

// Modified by VE3OOI
void loadConfig(void) {
  memset((char *)&cfg, 0, sizeof(cfg));
  EEPROM.readBytes(addr, (char *)&cfg, sizeof(cfg));
  if (cfg.flag != INIT_FLAG) {
    Serial.println("EEPROM config not defined. Running config not defined!!");
    Serial.println("Restart or use CLI to initialize EEPROM");
  } else {
    setRunningConfig();
  }
}

// Modified by VE3OOI
void checkConfig(void)  // ensure config settings are valid
{
  unsigned char changed = false;

  if ((charSpeed < MINSPEED) ||
      (charSpeed > MAXSPEED)) {  // validate character speed
    Serial.println("Invalid charSpeed. Resetting");
    charSpeed = DEFAULTSPEED;
    changed = true;
  }

  if ((codeSpeed < MINSPEED) ||
      (codeSpeed > MAXSPEED)) {  // validate code speed
    Serial.println("Invalid codeSpeed. Resetting");
    codeSpeed = DEFAULTSPEED;
    changed = true;
  }

  if ((pitch < MINPITCH) || (pitch > MAXPITCH)) {  // validate pitch
    Serial.println("Invalid pitch. Resetting");
    pitch = DEFAULTPITCH;
    changed = true;
  }

  if ((kochLevel < 0) ||
      (kochLevel > ELEMENTS(koch))) {  // validate koch lesson number
    Serial.println("Invalid kochLevel. Resetting");
    kochLevel = 0;
    changed = true;
  }

  if (xWordSpaces > MAXWORDSPACES) {  // validate word spaces
    Serial.println("Invalid xWordSpaces. Resetting");
    xWordSpaces = 0;
    changed = true;
  }

  if (!isAlphaNumeric(myCall[0])) {  // validate callsign
    Serial.println("Invalid myCall. Resetting");
    strcpy(myCall, DEFAULT_CALL);
    changed = true;
  }

  if ((keyerMode < 0) || (keyerMode > 2)) {  // validate keyer mode
    Serial.println("Invalid keyerMode. Resetting");
    keyerMode = IAMBIC_B;
    changed = true;
  }

  if ((brightness < 10) || (brightness > 100)) {  // validate screen brightness}
    Serial.println("Invalid brightness. Resetting");
    brightness = 100;
    changed = true;
  }

  if (textColor == bgColor) {  // text&bg colors must be different
    Serial.println("Invalid textColor/bgColor. Resetting");
    textColor = CYAN;
    bgColor = BLACK;
    changed = true;
  }

  if ((startItem < -1) || (startItem > 27)) {
    Serial.println("Invalid startItem. Resetting");
    startItem = -1;  // validate startup screen
    changed = true;
  }

  if (changed) {
    Serial.println("Saving changes to EEPROM");
    saveConfig();
  }
}

void useDefaults()  // if things get messed up...
{
  charSpeed = DEFAULTSPEED;
  codeSpeed = DEFAULTSPEED;
  pitch = DEFAULTPITCH;
  ditPaddle = PADDLE_A;
  dahPaddle = PADDLE_B;
  kochLevel = 1;
  usePaddles = true;
  xWordSpaces = 0;
  strcpy(myCall, DEFAULT_CALL);
  keyerMode = IAMBIC_B;
  brightness = 100;
  textColor = TEXTCOLOR;
  bgColor = BG;
  startItem = -1;
  saveConfig();
  roger();
}

char *ltrim(char *str) {      // quick n' dirty trim routine
  while (*str == ' ') str++;  // dangerous but efficient
  return str;
}

void selectionString(int choice, char *str)  // return menu item in str
{
  strcpy(str, "");
  if (choice < 0) return;  // validate input value
  int i = choice / 10;     // get menu from selection
  int j = choice % 10;     // get menu item from selection
  switch (i) {
    case 0:
      strcpy(str, ltrim(menu0[j]));
      break;  // show choice from menu0
    case 1:
      strcpy(str, ltrim(menu1[j]));
      break;  // show choice from menu1
    case 2:
      strcpy(str, ltrim(menu2[j]));
      break;   // show choice from menu2
    default:;  // oopsie
  }
}

void showMenuChoice(int choice) {
  char str[10];
  const int x = 180, y = 40;             // screen posn for startup display
  tft.fillRect(x, y, 130, 32, bgColor);  // erase any prior entry
  tft.setCursor(x, y);
  if (choice < 0)
    tft.print((char *)"Main Menu");
  else {
    tft.println(
        ltrim(mainMenu[choice / 10]));  // show top menu choice on line 1
    tft.setCursor(x, y + 16);           // go to next line
    selectionString(choice, str);       // get menu item
    tft.print(str);                     // and show it
  }
}

void changeStartup()  // choose startup activity
{
  const int LASTITEM = 27;  // currenly 27 choices
  tft.setTextSize(2);
  tft.println((char *)"\nStartup:");
  int i = startItem;
  if ((i < 0) || (i > LASTITEM)) i = -1;  // dont wig out on bad value
  showMenuChoice(i);                      // show current startup choice
  button_pressed = false;
  while (!button_pressed) {
    int dir = readEncoder();
    if (dir != 0)  // user rotated encoder knob:
    {
      i += dir;                        // change choice up/down
      i = constrain(i, -1, LASTITEM);  // keep within bounds
      showMenuChoice(i);               // display new startup choice
    }
  }
  startItem = i;  // update startup choice
}

void changeBackground() {
  const int x = 180, y = 150;  // screen posn for text display
  tft.setTextSize(2);
  tft.println((char *)"\n\n\nBackground:");
  tft.drawRect(x - 6, y - 6, 134, 49, WHITE);  // draw box around background
  button_pressed = false;
  int i = 0;
  tft.fillRect(x - 5, y - 5, 131, 46,
               colors[i]);  // show current background color
  while (!button_pressed) {
    int dir = readEncoder();
    if (dir != 0)  // user rotated encoder knob:
    {
      i += dir;  // change color up/down
      i = constrain(i, 0,
                    ELEMENTS(colors) - 1);  // keep within given colors
      tft.fillRect(x - 5, y - 5, 131, 46,
                   colors[i]);  // show new background color
    }
  }
  bgColor = colors[i];  // save background color
}

void changeTextColor() {
  const char sample[] = "ABCDE";
  const int x = 180, y = 150;  // screen posn for text display
  tft.setTextSize(2);
  tft.println((char *)"Text Color:");
  tft.setTextSize(4);
  tft.setCursor(x, y);
  int i = 7;  // start with cyan for fun
  tft.setTextColor(colors[i]);
  tft.print(sample);  // display text in cyan
  button_pressed = false;
  while (!button_pressed) {
    int dir = readEncoder();
    if (dir != 0)  // user rotated encoder knob:
    {
      i += dir;  // change color up/down
      i = constrain(i, 0,
                    ELEMENTS(colors) - 1);  // keep within given colors
      tft.setCursor(x, y);
      tft.setTextColor(colors[i], bgColor);  // show text in new color
      tft.print(sample);
    }
  }
  textColor = colors[i];  // update text color
}

void changeBrightness() {
  const int x = 180, y = 100;  // screen position
  tft.println((char *)"\n\n\nBrightness:");
  tft.setTextSize(4);
  tft.setCursor(x, y);
  tft.print(brightness);  // show current brightness
  button_pressed = true;  // on ESP32, inhibit user input
  while (!button_pressed) {
    int dir = readEncoder(2);
    if (dir != 0)  // user rotated encoder knob
    {
      brightness += dir * 5;  // so change level up/down, 5% increments
      brightness = constrain(brightness, 10, 100);  // stay in range
      tft.fillRect(x, y, 100, 40, bgColor);         // erase old value
      tft.setCursor(x, y);
      tft.print(brightness);      // and display new value
      setBrightness(brightness);  // update screen brightness
    }
  }
}

void setScreen() {
  changeStartup();     // set startup screen
  changeBrightness();  // set display brightness
  changeBackground();  // set background color
  changeTextColor();   // set text color
  saveConfig();        // save settings
  roger();             // and acknowledge
  clearScreen();
}

void setCodeSpeed() {
  const int x = 240, y = 50;  // screen posn for speed display
  tft.println("\nEnter");
  tft.print((char *)"Code Speed (WPM):");
  tft.setTextSize(4);
  tft.setCursor(x, y);
  tft.print(charSpeed);  // display current speed
  button_pressed = false;
  while (!button_pressed) {
    int dir = readEncoder();
    if (dir != 0)  // user rotated encoder knob:
    {
      charSpeed += dir;  // ...so change speed up/down
      charSpeed = constrain(charSpeed, MINSPEED, MAXSPEED);
      tft.fillRect(x, y, 50, 50, bgColor);  // erase old speed
      tft.setCursor(x, y);
      tft.print(charSpeed);  // and show new speed
    }
  }
  ditPeriod = intracharDit();  // adjust dit length
}

void setFarnsworth() {
  const int x = 240, y = 100;  // screen posn for speed display
  if (codeSpeed > charSpeed) codeSpeed = charSpeed;  // dont go above charSpeed
  tft.setTextSize(2);
  tft.println((char *)"\n\n\nFarnsworth");
  tft.print((char *)"Speed (WPM):");
  tft.setTextSize(4);
  tft.setCursor(x, y);
  tft.print(codeSpeed);  // display current speed
  button_pressed = false;
  while (!button_pressed) {
    int dir = readEncoder();
    if (dir != 0)  // user rotated encoder knob:
    {
      codeSpeed += dir;                            // ...so change speed up/down
      codeSpeed = constrain(codeSpeed,             // dont go below minimum
                            MINSPEED, charSpeed);  // and dont exceed charSpeed
      tft.fillRect(x, y, 50, 50, bgColor);         // erase old speed
      tft.setCursor(x, y);
      tft.print(codeSpeed);  // and show new speed
    }
  }
  ditPeriod = intracharDit();  // adjust dit length
}

void setExtraWordDelay()  // add extra word spacing
{
  const int x = 240, y = 150;  // screen posn for speed display
  tft.setTextSize(2);
  tft.println((char *)"\n\n\nExtra Word Delay");
  tft.print((char *)"(Spaces):");
  tft.setTextSize(4);
  tft.setCursor(x, y);
  tft.print(xWordSpaces);  // display current space
  button_pressed = false;
  while (!button_pressed) {
    int dir = readEncoder();
    if (dir != 0)  // user rotated encoder knob:
    {
      xWordSpaces += dir;                   // ...so change value up/down
      xWordSpaces = constrain(xWordSpaces,  // stay in range 0..MAX
                              0, MAXWORDSPACES);
      tft.fillRect(x, y, 50, 50, bgColor);  // erase old value
      tft.setCursor(x, y);
      tft.print(xWordSpaces);  // and show new value
    }
  }
}

void setSpeed() {
  setCodeSpeed();       // get code speed
  setFarnsworth();      // get farnsworth delay
  setExtraWordDelay();  // Thank you Mark AJ6CU!
  saveConfig();         // save the new speed values
  roger();              // and acknowledge
}

void setPitch() {
  const int x = 120, y = 80;  // screen posn for pitch display
  tft.print((char *)"Tone Frequency (Hz)");
  tft.setTextSize(4);
  tft.setCursor(x, y);
  tft.print(pitch);  // show current pitch
  while (!button_pressed) {
    int dir = readEncoder(2);
    if (dir != 0)  // user rotated encoder knob
    {
      pitch += dir * 50;  // so change pitch up/down, 50Hz increments
      pitch = constrain(pitch, MINPITCH, MAXPITCH);  // stay in range
      tft.fillRect(x, y, 100, 40, bgColor);          // erase old value
      tft.setCursor(x, y);
      tft.print(pitch);  // and display new value
      dit();             // let user hear new pitch
    }
  }
  saveConfig();  // save the new pitch
  roger();       // and acknowledge
}

void configKey() {
  tft.print((char *)"Currently: ");     // show current settings:
  if (!usePaddles)                      // using paddles?
    tft.print((char *)"Straight Key");  // not on your life!
  else {
    tft.print(
        (char *)"Iambic Mode ");  // of course paddles, but which keyer mode?
    if (keyerMode == IAMBIC_B)
      tft.print((char *)"B");  // show iambic B or iambic A
    else
      tft.print((char *)"A");
  }
  tft.setCursor(0, 60);
  tft.println((char *)"Send a dit NOW");  // first, determine which input is dit
  while (!button_pressed && !ditPressed() && !dahPressed())
    ;                          // wait for user input
  if (button_pressed) return;  // user wants to exit
  ditPaddle = !digitalRead(PADDLE_A)
                  ? PADDLE_A
                  : PADDLE_B;  // dit = whatever pin went low.
  dahPaddle =
      !digitalRead(PADDLE_A) ? PADDLE_B : PADDLE_A;  // dah = opposite of dit.
  tft.println((char *)"OK.\n");
  saveConfig();  // save it
  roger();       // and acknowledge.

  tft.println(
      (char *)"Send Dit again for Key");  // now, select key or paddle input
  tft.println((char *)"Send Dah for Paddles");
  while (!button_pressed && !ditPressed() && !dahPressed())
    ;                          // wait for user input
  if (button_pressed) return;  // user wants to exit
  usePaddles = dahPressed();   // dah = paddle input
  tft.println("OK.\n");
  saveConfig();  // save it
  roger();

  if (!usePaddles) return;
  tft.println((char *)"Send Dit for Iambic A");  // now, select keyer mode
  tft.println((char *)"Send Dah for Iambic B");
  while (!button_pressed && !ditPressed() && !dahPressed())
    ;                          // wait for user input
  if (button_pressed) return;  // user wants to exit
  if (ditPressed())
    keyerMode = IAMBIC_A;
  else
    keyerMode = IAMBIC_B;
  tft.println((char *)"OK.\n");
  saveConfig();  // save it
  if (keyerMode == IAMBIC_A)
    sendCharacter('A');
  else
    sendCharacter('B');
}

void setCallsign() {
  char ch, response[20];
  tft.print((char *)"\n Enter Callsign:");
  tft.setTextColor(CYAN);
  strcpy(response, "");  // start with empty response
  textRow = 2;
  textCol = 8;                             // set position of response
  while (!button_pressed && !ditPressed()  // wait until user is ready
         && !dahPressed())
    ;
  do {                                     // user has started keying...
    ch = morseInput();                     // get a character
    if (ch != ' ') addChar(response, ch);  // add it to the response
    addCharacter(ch);                      // and put it on screen
  } while (ch != ' ');                     // space = word timeout
  if (button_pressed) return;              // leave without saving
  strcpy(myCall, response);                // copy user input to callsign
  delay(1000);                             // allow user to see result
  saveConfig();                            // save it
  roger();
}

//===================================  Screen Routines
//====================================

//  The screen is divided into 3 areas:  Menu, Icons, and Body
//
//  [------------------------------------------------------]
//  [     Menu                                      Icons  ]
//  [------------------------------------------------------]
//  [                                                      ]
//  [     Body                                             ]
//  [                                                      ]
//  [------------------------------------------------------]
//
// "Menu" is where the main menu is displayed
// "Icons" is for battery icon and other special flags (30px)
// "Body" is the writable area of the screen

void clearMenu() { tft.fillRect(0, 0, DISPLAYWIDTH - 30, ROWSPACING, bgColor); }

void clearBody() {
  tft.fillRect(0, TOPMARGIN, DISPLAYWIDTH, DISPLAYHEIGHT, bgColor);
}

void clearScreen() {
  tft.fillScreen(bgColor);  // fill screen with background color
  tft.drawLine(0, TOPMARGIN - 6, DISPLAYWIDTH, TOPMARGIN - 6,
               YELLOW);  // draw horizontal menu line
}

void newScreen()  // prepare display for new text.  Menu not distrubed
{
  clearBody();  // clear screen below menu
  tft.setTextColor(textColor,
                   bgColor);    // set text foreground & background color
  tft.setCursor(0, TOPMARGIN);  // position graphics cursor
  textRow = 0;
  textCol = 0;  // position text cursor below the top menu
}

//===================================  Menu Routines
//====================================

void showCharacter(char c, int row,
                   int col)  // display a character at given row & column
{
  int x = col * COLSPACING;                // convert column to x coordinate
  int y = TOPMARGIN + (row * ROWSPACING);  // convert row to y coordinate
  tft.setCursor(x, y);                     // position character on screen
  tft.print(c);                            // and display it
}

void addCharacter(char c) {
  showCharacter(c, textRow,
                textCol);     // display character & current row/col position
  textCol++;                  // go to next position on the current row
  if ((textCol >= MAXCOL) ||  // are we at end of the row?
      ((c == ' ') &&
       (textCol > MAXCOL - 7)))  // or at a wordspace thats near end of row?
  {
    textRow++;
    textCol = 0;  // yes, so advance to beginning of next row
    if (textRow >= MAXROW)
      newScreen();  // if no more rows, clear & start at top.
  }
}

int getMenuSelection()  // Display menu system & get user selection
{
  int item;
  newScreen();  // start with fresh screen
  menuCol = topMenu(mainMenu,
                    ELEMENTS(mainMenu));  // show horiz menu & get user choice
  switch (menuCol)                        // now show menu that user selected:
  {
    case 0:
      item = subMenu(menu0, ELEMENTS(menu0));  // "receive" dropdown menu
      break;
    case 1:
      item = subMenu(menu1, ELEMENTS(menu1));  // "send" dropdown menu
      break;
    case 2:
      item = subMenu(menu2, ELEMENTS(menu2));  // "config" dropdown menu
  }
  return (menuCol * 10 + item);  // return user's selection
}

void setTopMenu(char *str)  // erase menu & replace with new text
{
  clearMenu();
  showMenuItem(str, 0, 0, FG, bgColor);
}

void showSelection(int choice)  // display current activity on top menu
{
  char str[10];                  // reserve room for string
  selectionString(choice, str);  // get menu item that user chose
  setTopMenu(str);               // and show it in menu bar
}

void showMenuItem(char *item, int x, int y, int fgColor, int bgColor) {
  tft.setTextSize(2);  // sets menu text size
  tft.setCursor(x, y);
  tft.setTextColor(fgColor, bgColor);
  tft.print(item);
}

int topMenu(char *menu[],
            int itemCount)  // Display a horiz menu & return user selection
{
  int index = menuCol;                 // start w/ current row
  button_pressed = false;              // reset button flag
  for (int i = 0; i < itemCount; i++)  // for each item in menu
    showMenuItem(menu[i], i * MENUSPACING, 0, FG, bgColor);  // display it
  showMenuItem(menu[index], index * MENUSPACING,  // highlight current item
               0, SELECTFG, SELECTBG);

  while (!button_pressed)  // loop for user input:
  {
    int dir = readEncoder();  // check encoder
    if (dir) {                // did it move?
      showMenuItem(menu[index], index * MENUSPACING, 0, FG,
                   bgColor);                 // deselect current item
      index += dir;                          // go to next/prev item
      if (index > itemCount - 1) index = 0;  // dont go beyond last item
      if (index < 0) index = itemCount - 1;  // dont go before first item
      showMenuItem(menu[index], index * MENUSPACING, 0, SELECTFG,
                   SELECTBG);  // select new item
    }
  }
  return index;
}

void displayFrame(char *menu[], int top, int itemCount) {
  int x = menuCol * MENUSPACING;    // x-coordinate of this menu
  for (int i = 0; i < MAXROW; i++)  // for all items in the frame
  {
    int y = TOPMARGIN + i * ROWSPACING;  // calculate y coordinate
    int item = top + i;
    if (item < itemCount)                           // make sure item exists
      showMenuItem(menu[item], x, y, FG, bgColor);  // and show the item.
  }
}

int subMenu(char *menu[],
            int itemCount)  // Display drop-down menu & return user selection
{
  int index = 0, top = 0, pos = 0, x, y;
  button_pressed = false;     // reset button flag
  x = menuCol * MENUSPACING;  // x-coordinate of this menu
  displayFrame(menu, 0,
               itemCount);             // display as many menu items as possible
  showMenuItem(menu[0], x, TOPMARGIN,  // highlight first item
               SELECTFG, SELECTBG);

  while (!button_pressed)  // exit on button press
  {
    int dir = readEncoder();  // check for encoder movement
    if (dir)                  // it moved!
    {
      if ((dir > 0) &&
          (index == (itemCount - 1)))  // dont try to go below last item
        continue;
      if ((dir < 0) && (index == 0))  // dont try to go above first item
        continue;
      if ((dir > 0) &&
          (pos == (MAXROW - 1)))  // does the frame need to move down?
      {
        top++;                               // yes: move frame down,
        displayFrame(menu, top, itemCount);  // display it,
        index++;                             // and select next item
      } else if ((dir < 0) && (pos == 0))    // does the frame need to move up?
      {
        top--;                               // yes: move frame up,
        displayFrame(menu, top, itemCount);  // display it,
        index--;                             // and select previous item
      } else  // we must be moving within the itemEEPROMframe
      {
        y = TOPMARGIN + pos * ROWSPACING;  // calc y-coord of current item
        showMenuItem(menu[index], x, y, FG,
                     bgColor);  // deselect current item
        index += dir;           // go to next/prev item
      }
      pos = index - top;  // posn of selected item in visible list
      y = TOPMARGIN + pos * ROWSPACING;  // calc y-coord of new item
      showMenuItem(menu[index], x, y, SELECTFG,
                   SELECTBG);  // select new item
    }
  }
  return index;
}

//================================  Main Program Code
//================================

void setBrightness(int level)  // level 0 (off) to 100 (full on)
{
  // not implemented in ESP32 version at this time
}

void initEncoder() {
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  pinMode(ENCODER_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_BUTTON), buttonISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A), rotaryISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B), rotaryISR, CHANGE);
}

void initMorse() {
  // Modifed by VE3OOI. /
  /* There was an error with ledcSetup.
    00,hd_drv:0xE (844) ledc: requested frequency and duty resolution can not be
    achieved, try reducing freq_hz or duty_resolution. div_param=50 [
    848][E][esp32-hal-ledc.c:75] ledcSetup(): ledc setup failed!

  Issue is that frequency and resolution set too high for PWM.
  ledc is used to generate a PWM signal which simulates either a DC level
  (if fed into a cap) or a crude tone frequency (with lots of harmonics)
  ledc supports max frequency based on resolution bits. E.g. The maximum
  PWM frequency with resolution of 10 bits is 78.125KHz.
  */
  //  ledcSetup(0, 1E5, 12);            // smoke & mirrors for ESP32
  ledcSetup(0, 1E4, 8);     // Modified by VE3OOI to allow for proper resolution
  ledcAttachPin(AUDIO, 0);  // since tone() not yet supported
  pinMode(LED, OUTPUT);     // LED, but could be keyer output instead
  pinMode(PADDLE_A, INPUT_PULLUP);  // two paddle inputs, both active low
  pinMode(PADDLE_B, INPUT_PULLUP);
  ditPeriod = intracharDit();            // set up character timing from WPM
  dahPaddle = (ditPaddle == PADDLE_A) ?  // make dahPaddle opposite of dit
                  PADDLE_B
                                      : PADDLE_A;
  roger();  // acknowledge morse startup with an 'R'
}

void initSD() {
  delay(200);       // dont rush things on power-up
  SD.begin(SD_CS);  // initialize SD library
  delay(100);       // takin' it easy
}

void initScreen() {
  tft.begin();                       // initialize screen object
  tft.setRotation(SCREEN_ROTATION);  // landscape mode: use '1' or '3'
  tft.fillScreen(BLACK);             // start with blank screen
  setBrightness(100);                // start screen full brighness
}

void splashScreen()  // not splashy at all!
{                    // have fun sprucing it up.
  tft.setTextSize(3);
  tft.setTextColor(YELLOW);
  tft.setCursor(100, 50);
  tft.print(myCall);  // display user's callsign
  tft.setTextColor(CYAN);
  tft.setCursor(15, 90);
  tft.print((char *)"Morse Code Tutor");  // add title
  tft.setCursor(30, 140);
  tft.setTextSize(2);
  tft.print((char *)"VE3OOI Firmware v0.1");  // add title
  tft.setTextSize(1);
  tft.setCursor(50, 220);
  tft.setTextColor(WHITE);
  tft.print((char *)"Copyright (c) 2020, Bruce E. Hall");  // legal small print
  tft.setTextSize(2);
}

void setup() {
  Serial.begin(115200);  // for debugging only
  initScreen();          // blank screen in landscape mode
  // Changed by VE3OOI
  EEPROM.begin(sizeof(eebuf));  // ESP32 specific for bytes used of flash

  initSD();  // initialize SD library

  // Added by VE3OOI
#ifndef REMOVE_CLI
  resetSerial();
#endif
  loadConfig();  // get saved values from EEPROM
  if (cfg.flag != INIT_FLAG) {
    Serial.println("Initializing EEPROM");
    initializeMem();
  }

  splashScreen();  // show we are ready
  initEncoder();   // attach encoder interrupts
  initMorse();     // attach paddles & adjust speed
  delay(2000);     // keep splash screen on for a while
  clearScreen();
  resetSerial();
}

void loop() {
  int selection = startItem;  // start with user specified startup screen
  if (!inStartup || (startItem < 0))  // but, if there isn't one,
    selection = getMenuSelection();   // get menu selection from user instead
  inStartup = false;                  // reset startup flag
  showSelection(selection);  // display users selection at top of screen
  newScreen();               // and clear the screen below it.
  button_pressed = false;    // reset flag for new presses
  randomSeed(millis());      // randomize!
  score = 0;
  hits = 0;
  misses = 0;  // restart score for copy challenges

  switch (selection)  // do action requested by user
  {
    case 00:
      sendKoch();
      break;
    case 01:
      sendLetters();
      break;
    case 02:
      sendWords();
      break;
    case 03:
      sendNumbers();
      break;
    case 04:
      sendMixedChars();
      break;
    case 05:
      sendFromSD();
      break;
    case 06:
      sendQSO();
      break;
    case 07:
      sendCallsigns();
      break;

    case 10:
      practice();
      break;
    case 11:
      copyOneChar();
      break;
    case 12:
      copyTwoChars();
      break;
    case 13:
      copyWords();
      break;
    case 14:
      copyCallsigns();
      break;
    case 15:
      flashcards();
      break;
    case 16:
      headCopy();
      break;
    case 17:
      twoWay();
      break;

    case 20:
      setSpeed();
      break;
    case 21:
      checkSpeed();
      break;
    case 22:
      setPitch();
      break;
    case 23:
      configKey();
      break;
    case 24:
      setCallsign();
      break;
    case 25:
      setScreen();
      break;
    case 26:
      useDefaults();
      break;
    case 27:
      openCLI();
      break;
    default:;
  }
}

void openCLI(void) {
#ifndef REMOVE_CLI
  printBanner();
  resetSerial();
  printPrompt();

  while (!button_pressed)  // exit on button press
  {
    processSerial();
  }
  resetSerial();

#else
  Serial.println("CLI Not Implemented")
#endif
}

#ifndef REMOVE_CLI

// Place program specific content here
void executeSerial(char *str) {
  // num defined the actual number of entries process from the serial buffer
  // i is a generic counter

  // This function called when serial input in present in the serial buffer
  // The serial buffer is parsed and characters and numbers are scraped and
  // entered in the commands[] and numbers[] variables.
  parseSerial(str);

  // Process the commands
  // Note: Whenever a parameter is stated as [CLK] the square brackets are not
  // entered. The square brackets means that this is a command line parameter
  // entered after the command. E.g. F [n] [m] would be mean "F 0 7000000" is
  // entered (no square brackets entered)
  switch (commands[0]) {
    case 'C':  // Get call sign
      Serial.print("Current: ");
      Serial.println(cfg.myCall);
      readSerialLine((char *)"Call Sign: ", MAX_CALLSIGN_STRING);
      if (strlen(line) > MIN_STRING) {
        Serial.print("\r\nChanging: ");
        Serial.print(cfg.myCall);
        Serial.print(" to: ");
        Serial.println(line);
        // already forced checking for array size MAX_CHAR_STRING
        // forced "line" to be zero filled so no danger of buffer overflow here
        memset(cfg.myCall, 0, sizeof(cfg.myCall));
        strncpy(cfg.myCall, line, strlen(line));
        strncpy(myCall, line, strlen(line));
      } else {
        Serial.println("Empty input");
      }
      break;

    case 'D':  // Dump EEPROM
      dumpMem();
      break;

    case 'E':  // Erase EEPROM
      clearMem();
      break;

    case 'H':  // Help
      Serial.println("Help:");
      Serial.println("C - enter callsign");
      Serial.println("D - dump eeprom");
      Serial.println("E - erase eeprom");
      Serial.println("I - init eeprom with defaults");
      Serial.println("L - load eeprom & run");
      Serial.println("M - enter server name");
      Serial.println("P - print running config");
      Serial.println("P E - print eeprom config");
      Serial.println("R - enter room name");
      Serial.println("S - save running config to eeprom");
      Serial.println("T - test network connection");
      Serial.println("U U - enter MQTT username");
      Serial.println("U P - enter MQTT password");
      Serial.println("W S - enter Wi-Fi SSID");
      Serial.println("W P - enter Wi-Fi password");
      break;

    case 'I':  // Initialize EEPROM
      initializeMem();
      break;

    case 'L':  // Load config EEPROM
      loadConfig();
      break;

    case 'M':  // Get MQTT Server name
      Serial.print("Current: ");
      Serial.println(cfg.mqtt_server);
      readSerialLine((char *)"Server DNS Name: ", MAX_CHAR_STRING);
      if (strlen(line) > MIN_STRING) {
        Serial.print("\r\nChanging: ");
        Serial.print(cfg.mqtt_server);
        Serial.print(" to: ");
        Serial.println(line);
        // already forced checking for array size MAX_CHAR_STRING
        // forced "line" to be zero filled so no danger of buffer overflow here
        memset(cfg.mqtt_server, 0, sizeof(cfg.mqtt_server));
        strncpy(cfg.mqtt_server, line, strlen(line));
      } else {
        Serial.println("Empty input");
      }
      break;

    case 'P':  // Print memory Config
      if (commands[1] == 'E') {
        printConfig(true);  // print out saved EEPROM config
      } else {
        printConfig(false);  // print out running (i.e. loaded) config
      }
      break;

    case 'R':  // Get MQTT Topic which I call room
      Serial.print("Current: ");
      Serial.println(cfg.room);
      readSerialLine((char *)"Room Name: ", MAX_CHAR_STRING);
      if (strlen(line) > MIN_STRING) {
        Serial.print("\r\nChanging: ");
        Serial.print(cfg.room);
        Serial.print(" to: ");
        Serial.println(line);
        // already forced checking for array size MAX_CHAR_STRING
        // forced "line" to be zero filled so no danger of buffer overflow here
        memset(cfg.room, 0, sizeof(cfg.room));
        strncpy(cfg.room, line, strlen(line));
      } else {
        Serial.println("Empty input");
      }
      break;

    case 'S':  // save current memory Config
      saveConfig();
      break;

    case 'T':  // Test WiFi and MQTT Connection
      Serial.print("Testing WiFi and MQTT Connection");
      initWireless();  // look for another unit & connect
      if (!(cfg.conflag & SRV_CONNECTED)) {
        Serial.println("FAILURE! Please check config");
      } else {
        Serial.println("SUCESS!!!  Don't forget to save config");
      }
      Serial.println("Closing WiFi...");
      closeWireless();
      break;

    case 'U':  // Enter user info
      if (commands[1] == 'U') {
        Serial.print("Current: ");
        Serial.println(cfg.mqtt_userid);
        readSerialLine((char *)"Enter username: ", MAX_CHAR_STRING);
      } else if (commands[1] == 'P') {
        Serial.print("Current: ");
        Serial.println(cfg.mqtt_password);
        readSerialLine((char *)"Enter password: ", MAX_CHAR_STRING);
      } else {
        Serial.println("Usage: 'U U' or 'U P'");
        break;
      }
      if (strlen(line) > MIN_STRING && commands[1] == 'U') {
        Serial.print("\r\nChanging: ");
        Serial.print(cfg.mqtt_userid);
        Serial.print(" to: ");
        Serial.println(line);
        // already forced checking for array size MAX_CHAR_STRING
        // forced "line" to be zero filled so no danger of buffer overflow here
        memset(cfg.mqtt_userid, 0, sizeof(cfg.mqtt_userid));
        strncpy(cfg.mqtt_userid, line, strlen(line));
      } else if (strlen(line) > MIN_STRING && commands[1] == 'P') {
        Serial.print("\r\nChanging: ");
        Serial.print(cfg.mqtt_password);
        Serial.print(" to: ");
        Serial.println(line);
        // already forced checking for array size MAX_CHAR_STRING
        // forced "line" to be zero filled so no danger of buffer overflow here
        memset(cfg.mqtt_password, 0, sizeof(cfg.mqtt_password));
        strncpy(cfg.mqtt_password, line, strlen(line));
      } else
        Serial.println("Empty input");
      break;

    case 'W':  // Print memory Config
      if (commands[1] == 'S') {
        Serial.print("Current: ");
        Serial.println(cfg.wifi_ssid);
        readSerialLine((char *)"Enter SSID: ", MAX_CHAR_STRING);
      } else if (commands[1] == 'P') {
        Serial.print("Current: ");
        Serial.println(cfg.wifi_password);
        readSerialLine((char *)"Enter password: ", MAX_CHAR_STRING);

      } else {
        Serial.println("Usage: 'W S' or 'W P'");
        break;
      }
      if (strlen(line) > MIN_STRING && commands[1] == 'S') {
        Serial.print("\r\nChanging: ");
        Serial.print(cfg.wifi_ssid);
        Serial.print(" to: ");
        Serial.println(line);
        // already forced checking for array size MAX_CHAR_STRING
        // forced "line" to be zero filled so no danger of buffer overflow here
        memset(cfg.wifi_ssid, 0, sizeof(cfg.wifi_ssid));
        strncpy(cfg.wifi_ssid, line, strlen(line));
      } else if (strlen(line) > MIN_STRING && commands[1] == 'P') {
        Serial.print("\r\nChanging: ");
        Serial.print(cfg.wifi_password);
        Serial.print(" to: ");
        Serial.println(line);
        // already forced checking for array size MAX_CHAR_STRING
        // forced "line" to be zero filled so no danger of buffer overflow here
        memset(cfg.wifi_password, 0, sizeof(cfg.wifi_password));
        strncpy(cfg.wifi_password, line, strlen(line));
      } else
        Serial.println("Empty input");
      break;

    // If an undefined command is entered, display an error message
    default:
      errorOut();
  }
}

void readSerialLine(char *inprompt, int size) {
  char input = 0;
  unsigned char i = 0;
  unsigned char again = true;

  memset(line, 0, sizeof(line));
  Serial.print(inprompt);
  Serial.flush();  // Flush output buffer

  do {
    if (Serial.available() > 0) {
      char input = Serial.read();
      if (isPrintable(input)) {
#ifdef TERMINAL_ECHO
        Serial.print(input);  // Echo input
#endif
        line[i++] = input;
        if (i >= size) again = false;
      } else {
        if (input == 0xA || input == 0xD) {
          again = false;
          Serial.read();  // Read any remaining CR LF characters in buffer
        }
      }
    }
  } while (again);
  if (i >= size) {
    Serial.println("String Too Long");
    memset(line, 0, sizeof(line));
  }
}

#endif  // REMOVE_CLI