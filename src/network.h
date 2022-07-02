

#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <Arduino.h>
#include "main.h"

//===================================  Wireless Constants
//===============================
// Modified by VE3OOI
#define MAXBUFLEN 100  // size of incoming character buffer

// Added by VE3OOI
// Connection Flags and defines
#define AP_CONNECTED 0x1   // Flag: connected to WiFi AP
#define DNS_CONNECTED 0x2  // Flag: connected to DNS and Querry OK
#define SRV_CONNECTED 0x4  // Flag: connected to WiFi AP
#define WIFI_TIMEOUT \
  30  // Timeout in 500ms increments to timeout connecting to Access Point


// Processing MQTT
#define MQTT_DELIMETER ':'
#define MAX_MQTT_MSG_LEN \
  6  // Only allow 10 character messages xxnxxxx:x (x=alphabetic, n=digit)


// Added by VE3OOI
// Function Prototypes
void MQTTcallback(char *topic, byte *payload, unsigned int len);
void processMQTT(void);

void enQueue(char ch);
char deQueue(void);
void setStatusLED(int color);
void sendWireless(uint8_t data);
void closeWireless(void);
void initWireless(void);
void initializeMem(void);

#endif  // _NETWORK_H_