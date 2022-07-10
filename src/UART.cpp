/*

  Program Written by Dave Rajnauth, VE3OOI to control the UART for CLI.

  Software is licensed under a Creative Commons Attribution 4.0 International
  License.

*/

#include "Arduino.h"
#include "main.h"
#include "network.h"

#ifndef REMOVE_CLI
#include "UART.h"  // VE3OOI Serial Interface Routines (TTY Commands)

// These variables are defined in UART.cpp and used for Serial interface
// rbuff is used to store all keystrokes which is parsed by Execute()
// commands[] and numbers[] store all characters or numbers entered provided
// they are separated by spaces. ctr is counter used to process entries
// command_entries contains the total number of charaters/numbers entered
char rbuff[RBUFF];
char commands[MAX_COMMAND_ENTRIES];
unsigned char command_entries;
unsigned long numbers[MAX_COMMAND_ENTRIES];
unsigned char ctr;

char prompt[6] = {0xa, 0xd, ':', '>', ' ', 0x0};
char ovflmsg[9] = {'O', 'v', 'e', 'r', 'f', 'l', 'o', 'w', 0x0};
char errmsg[4] = {'E', 'r', 'r', 0x0};

void flushSerialInput(void) {
  while (Serial.available() > 0) Serial.read();
}

void resetSerial(void)
// This routine is used to flush all serial input output and zero out all serial
// buffers
{
  Serial.flush();
  flushSerialInput();
  memset(rbuff, 0, sizeof(rbuff));
  memset(numbers, 0, sizeof(numbers));
  memset(commands, 0, sizeof(commands));
  ctr = 0;
}

void printPrompt(void) {  // this seems silly but would be better if had a C++
                          // object and this was a private routine
  Serial.write(prompt);
}

void printBanner(void) {  // this seems silly but would be better if had a C++
                          // object and this was a private routine
  Serial.write(BANNER);
}

void processSerial(void)
// This routing is called to check is there is serial input and store the input
// into the serial buffer if a CR/LF (Enter pressed) is received, then process
// the command and flush the buffer.
{
  char temp;

  // Serial.available() returns the number of character that have been entered
  // at the keyboard. The idea here is that you keep processing characters until
  // none are left. This routine is faster that a user's typing and it needs to
  // check for CR/LF
  while (Serial.available() > 0) {
    if (ctr < sizeof(rbuff)) {
      temp = Serial.read();        // Read a character
      Serial.write(temp);          // Echo the character back to the user
      if (isPrintable(temp)) {  // If the character is alphabetic than store
                                   // it in the buffer
        rbuff[ctr++] = temp;
      } else if (temp == 0xD ||
                 temp == 0xA) {  // If the character is not printable and its a
                                 // CR/LF then process the buffer
        Serial.read();           // read any remaining CR LF character
        if (ctr) {
          Serial.println("");
          executeSerial(rbuff);
          resetSerial();
        }
        Serial.write(prompt);
      }
    }
    // This checks to see if the users has entered too much data which would
    // overflew the serial buffer the UART.h file details the MAX number of
    // characters
    if (ctr > sizeof(rbuff)) {
      Serial.println("Overflow");
      resetSerial();
      Serial.write(prompt);
      break;
    }
  }
}

unsigned char parseSerial(char* str)
// This routine is used to parse all character and numbers in the serial buffer
// (str pointer)
//  Characters are entered into the ccmmands[] array and numbers are entered
//  into the numbers[] array.
{
  static unsigned char i, j, k;  // Misc counters used, i is for serial buffer,
                                 // j is for commands[], k is for numbers[]
  memset(numbers, 0, sizeof(numbers));  // Flush out the arrays
  memset(commands, 0, sizeof(commands));

  for (i = 0, j = 0, k = 0; i < strlen(str);
       i++) {  // Step through the serial buffer - str is a pointer to the
               // serial buffer
    if (isalpha(str[i])) {  // Its alphabetic so store it in commands[]
      commands[j++] = toupper(str[i]);
    } else if (isdigit(str[i])) {
      numbers[k++] = strtol((char*)&str[i], NULL, 10);
      while (isdigit(str[i + 1])) {
        if (i >= strlen(str)) break;
        i++;
      }
    } else if (isgraph(str[i])) {
      commands[j++] = str[i];
    }
  }
  return j;
}

void errorOut(void)
// This routine is used to print out an error message.
// An argument could be passed with an error code which is decoded here.
{
  Serial.println(errmsg);
}

#endif  // REMOVE_CLI
