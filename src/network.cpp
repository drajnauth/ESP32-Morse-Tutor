#include <arduino.h>
#include <WiFi.h>
//#include <esp_now.h>

// added by VE3OOI
#include <PubSubClient.h>

#include "main.h"

// Added by VE3OOI
const char *wifi_ssid = "IKILLU_SLOW";
const char *wifi_password = "feedface012345678910111213";
const char *mqtt_userid = "user1";
const char *mqtt_password = "1234";
const char *mqtt_server = "ve3ooi.ddns.net";
const char *localid = "VE3OOI";
const char *room = "morsetutor";
unsigned char conflag = 0;              // Wireless connection Status
char tbuf[MAXBUFLEN], rbuf[MAXBUFLEN];  // send receive MQTT buffer

WiFiClient espClient;
PubSubClient client(espClient);
//

char buf[MAXBUFLEN];        // buffer for incoming characters
int inPtr = 0, outPtr = 0;  // pointer to location of next character

void processMQTT(void) { client.loop(); }

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
  //  esp_err_t result;

  if (conflag & SRV_CONNECTED) {
    memset(tbuf, 0, sizeof(tbuf));
    sprintf(tbuf, "%s:%c", localid, data);
    client.publish(room, (char *)tbuf, strlen(tbuf));
    Serial.print("Sending MQTT message: ");
    Serial.println((char *)tbuf);
  } else {
    Serial.println("Error: No MQTT server connection");
  }
  //  const uint8_t *peer_addr = peer.peer_addr;
  //  result = esp_now_send(peer_addr, &data, sizeof(data));  // send data to
  //  peer Serial.print(" - result "); Serial.println((int)result, HEX);
}

// Modified by VE3OOI
void closeWireless() {
  setStatusLED(BLACK);  // erase two-way status LED
                        //  Serial.println("Telling peer I am closing");
  //  sendWireless(CMD_LEAVING);  // message other unit: I am leaving
  //  esp_now_deinit();           // quit esp_now
  //  WiFi.softAPdisconnect(true);
  client.disconnect();
  Serial.println("Disconnected from MQTT");
  WiFi.disconnect();
  Serial.println("Wireless now closed");
}

// Modified by VE3OOI
void initWireless() {
  Serial.println("\r\n\r\nMQTT Sensor v0.1 Initialization\r\n");
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  conflag = 0;
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < WIFI_TIMEOUT) {
    Serial.print(".");
    timeout++;
    delay(500);
  }

  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Error Connecting to ");
    Serial.println(wifi_ssid);
    conflag = 0;
    return;
  }

  conflag |= AP_CONNECTED;
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("DNS IP address: ");
  Serial.println(WiFi.dnsIP());
  Serial.print("Gateway IP address: ");
  Serial.println(WiFi.gatewayIP());

  randomSeed(micros());

  memset(tbuf, 0, sizeof(tbuf));

  timeout = 0;
  IPAddress serverIP(0, 0, 0, 0);
  //  IPAddress server(192, 168, 1, 50);  // Used for local testing.

  Serial.println("Resolving MQTT hostname...");
  while (serverIP.toString() == "0.0.0.0" && timeout < 10) {
    WiFi.hostByName(mqtt_server, serverIP);
    timeout++;
    delay(250);
  }

  if (serverIP.toString() != "0.0.0.0") {
    Serial.print("MQTT host address resolved:");
    Serial.println(serverIP.toString());
    conflag |= DNS_CONNECTED;
  } else {
    Serial.println("Error resolving MQTT hostname via DHCP provided DNS");
    conflag = 0;
    return;
  }

  client.setServer(serverIP, 1883);
  client.setCallback(MQTTcallback);

  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    // Attempt to connect to MQTT Server
    if (client.connect(localid, mqtt_userid, mqtt_password)) {
      sprintf(tbuf, "%s-%s", localid, "Online");
      client.publish(room, (char *)tbuf, strlen(tbuf));
      client.subscribe(room);
      conflag |= SRV_CONNECTED;
      delay(500);
      // Error connecting to MQTT Server
    } else {
      Serial.print("Error Connecting to MQTT Server: ");
      Serial.println(client.state());
      conflag = 0;
      delay(5000);
      return;
    }
  }
  inPtr = outPtr = 0;  // reset pointer to location of next character
  memset(buf, 0, sizeof(buf));
  memset(rbuf, 0, sizeof(rbuf));
  memset(tbuf, 0, sizeof(tbuf));
  Serial.println("MQTT Connected");
}

// Added by VE3OOI
void MQTTcallback(char *topic, byte *data, unsigned int data_len) {
  char *substring, ch;
  // In order to republish this payload, a copy must be made
  // as the orignal payload buffer will be overwritten whilst
  // constructing the PUBLISH packet.

  if (!strstr((char *)data, localid)) {
    memset(rbuf, 0, sizeof(rbuf));

    strncpy((char *)rbuf, (char *)data, data_len);

    ch = (char)NULL;
    substring = strrchr(rbuf, MQTT_DELIMETER);
    if (substring != NULL) {
//      Serial.print("MQTTcallback Received valid string: '");
//      Serial.print((char *)substring);
//      Serial.println("'");

      if (strlen(substring) > 1 && substring[1] != (char)NULL) {
        ch = substring[1];
//        Serial.print("Processing valid character: ");
//        Serial.println((char)ch);
        setStatusLED(GREEN);  // green status LED for data received
        enQueue(ch);          // put recieved data into queue
      }
    } else {
      Serial.print("MQTTcallback Received invalid message: ");
      Serial.println((char *)rbuf);
    }
  }
}