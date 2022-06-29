#include <Arduino.h>

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

//===================================  Wireless Constants
//===============================
// Removed by VE3OOI
#define CHANNEL 1               // Wifi channel number
#define WIFI_SSID "W8BH Tutor"  // Wifi network name
#define WIFI_PWD "9372947313"   // Wifi password
#define MAXBUFLEN 100           // size of incoming character buffer
#define CMD_ADDME 0x11          // request to add this unit as a peer
#define CMD_LEAVING 0x12        // flag this unit as leaving
// Added by VE3OOI
// Connection Flags and defines
#define AP_CONNECTED  0x1       // Flag: connected to WiFi AP
#define DNS_CONNECTED  0x2      // Flag: connected to DNS and Querry OK
#define SRV_CONNECTED  0x4      // Flag: connected to WiFi AP 
#define WIFI_TIMEOUT 30         // Timeout in 500ms increments to timeout connecting to Access Point


// Processing MQTT
#define MQTT_DELIMETER ':'


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
// Function Prototypes
void MQTTcallback(char *topic, byte *payload, unsigned int len);

void enQueue(char ch);
char deQueue(void);
void initESPNow(void);
void configDeviceAP(void);
void setStatusLED(int color);
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void onDataRecv(const uint8_t *mac_add, const uint8_t *data, int data_len);
bool networkFound(void);
void addPeer2(const uint8_t *peerMacAddress);
void addPeer(void);
void sendWireless(uint8_t data);
void sendAddPeerCmd(void);
void closeWireless(void);
void initWireless(void);

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
