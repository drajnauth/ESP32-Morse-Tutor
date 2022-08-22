#include <WiFi.h>
#include <arduino.h>

// Added by VE3OOI
#include <PubSubClient.h>

#include "UART.h"  // VE3OOI Serial Interface Routines (TTY Commands)
#include "main.h"
#include "network.h"

// Added by VE3OOI
extern char myCall[10];  // Defined in main.cpp
extern TUTOR_STRUT cfg;  // Defined in main.cpp

char localid[10];
char tbuf[MAXBUFLEN],
    rbuf[MAXBUFLEN];  // send receive MQTT buffer

WiFiClient espClient;
PubSubClient client(espClient);
//

// orign code here
char buf[MAXBUFLEN];        // buffer for incoming characters
int inPtr = 0, outPtr = 0;  // pointer to location of next character

// Added by VE3OOI to process MQTT messages from main.cpp
void processMQTT(void) { client.loop(); }

// orign code here
void enQueue(char ch) {
  buf[inPtr++] = ch;  // add character to buffer, increment pointer
  if (inPtr == MAXBUFLEN) inPtr = 0;  // circularize it!
}

char deQueue() {
  char ch = 0;
  if (outPtr != inPtr)                  // return 0 if no characters available
    ch = buf[outPtr++];                 // get character and increment pointer
  if (outPtr == MAXBUFLEN) outPtr = 0;  // circularize it!
  return ch;
}

// Modified by VE3OOI
void sendWireless(uint8_t data) {
  if (cfg.conflag & SRV_CONNECTED) {
    memset(tbuf, 0,
           sizeof(tbuf));  // Flush string.  Ensure it NULL terminated string
    sprintf(tbuf, "%s:%c", localid, data);
    client.publish(cfg.room, (char *)tbuf, strlen(tbuf));
    Serial.print("Sending MQTT message: ");
    Serial.println((char *)tbuf);
  } else {
    Serial.println("Error: No MQTT server connection");
  }
}

// Modified by VE3OOI
void closeWireless() {
  setStatusLED(BLACK);  // erase two-way status LED
                        //  Serial.println("Telling peer I am closing");
  client.disconnect();
  Serial.println("Disconnected from MQTT");
  WiFi.disconnect();
  cfg.conflag = 0;
  Serial.println("Wireless now closed");
}

// Modified by VE3OOI
void initWireless() {
  Serial.println("\r\n\r\nMQTT Sensor v0.1 Initialization\r\n");
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(cfg.wifi_ssid);

  WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);

  cfg.conflag = 0;
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < WIFI_TIMEOUT) {
    Serial.print(".");
    timeout++;
    delay(500);
  }

  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Error Connecting to ");
    Serial.println(cfg.wifi_ssid);
    cfg.conflag = 0;
    return;
  }

  cfg.conflag |= AP_CONNECTED;
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("DNS IP address: ");
  Serial.println(WiFi.dnsIP());
  Serial.print("Gateway IP address: ");
  Serial.println(WiFi.gatewayIP());

  randomSeed(micros());

  timeout = 0;
  IPAddress serverIP(0, 0, 0, 0);
  //  IPAddress server(192, 168, 1, 50);  // Used for local testing.

  Serial.println("Resolving MQTT hostname...");
  while (serverIP.toString() == "0.0.0.0" && timeout < 10) {
    WiFi.hostByName(cfg.mqtt_server, serverIP);
    timeout++;
    delay(250);
  }

  if (serverIP.toString() != "0.0.0.0") {
    Serial.print("MQTT host address resolved:");
    Serial.println(serverIP.toString());
    cfg.conflag |= DNS_CONNECTED;
  } else {
    Serial.println("Error resolving MQTT hostname via DHCP provided DNS");
    cfg.conflag = 0;
    return;
  }

  client.setServer(serverIP, 1883);
  client.setCallback(MQTTcallback);

  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    // Attempt to connect to MQTT Server
    memset(localid, 0,
           sizeof(localid));  // Flush string.  Ensure it NULL terminated string

    sprintf(localid, "%c%c%c", (char)random(65, 90), (char)random(65, 90),
            (char)random(65, 90));

    if (client.connect(localid, cfg.mqtt_userid, cfg.mqtt_password)) {
      memset(tbuf, 0,
             sizeof(tbuf));  // Flush string.  Ensure it NULL terminated string
      // Announce arrival
      sprintf(tbuf, "%s:%s-%s", myCall, localid, "Online");

      client.publish(cfg.room, (char *)tbuf, strlen(tbuf));
      delay(500);  // Allow message to reach device.  To allow receive buffer to
                   // be processed

      // Subscribe to topic (which i call the "room")
      client.subscribe(cfg.room);
      cfg.conflag |= SRV_CONNECTED;

      // Send CQ
      sendWireless(' ');
      sendWireless('c');
      sendWireless('q');
      sendWireless(' ');
      delay(500);  // Allow messages to reach device.  To allow receive buffer
                   // to be processed

      // Error connecting to MQTT Server
    } else {
      Serial.print("Error Connecting to MQTT Server: ");
      Serial.println(client.state());
      cfg.conflag = 0;
      return;
    }
  }
  inPtr = outPtr = 0;  // reset pointer to location of next character
  memset(buf, 0,
         sizeof(buf));  // Flush string.  Ensure it NULL terminated string
  memset(rbuf, 0,
         sizeof(rbuf));  // Flush string.  Ensure it NULL terminated string
  memset(tbuf, 0,
         sizeof(tbuf));  // Flush string.  Ensure it NULL terminated string
  Serial.println("MQTT Connected");
}

// Added by VE3OOIt process incomming MQTT message
void MQTTcallback(char *topic, byte *data, unsigned int data_len) {
  char *substring, ch;
  // In order to republish this payload, a copy must be made
  // as the orignal payload buffer will be overwritten whilst
  // constructing the PUBLISH packet.

  ///////// Troubleshooting
  // Serial.println(data_len);
  // Serial.println(localid);
  // for (int i = 0; i < data_len; i++) {
  //   Serial.print((char)data[i]);
  // }
  // Serial.println();

  if (data_len <= MAX_MQTT_MSG_LEN) {  // Ensure message is not too large
    memset(rbuf, 0,
           sizeof(rbuf));  // Flush string.  Ensure it NULL terminated string
    strncpy((char *)rbuf, (char *)data, data_len);

    if (!strstr((char *)rbuf,
                localid)) {  // Ignore message with own callsign or ID
      ch = (char)NULL;
      substring = strrchr(rbuf, MQTT_DELIMETER);
      if (substring != NULL) {
        if (strlen(substring) > 1 && substring[1] != (char)NULL) {
          ch = substring[1];
          setStatusLED(GREEN);  // green status LED for data received
          enQueue(ch);          // put recieved data into queue
        } else {
          Serial.println("MQTTcallback Received null message");
        }
      } else {
        Serial.print("MQTTcallback Received invalid message: ");
        Serial.println((char *)rbuf);
      }
    }
  } else {
    Serial.println("MQTTcallback received message thats too long");
  }
}