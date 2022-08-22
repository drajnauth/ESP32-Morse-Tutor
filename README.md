# ESP32-Morse-Tutor

This documents the configuration of the W8BH morse code tutor running on an ESP32 microcontroller that support WiFi.  This is NOT applicable to the STM32 version.
1.	Load the new firmware onto the ESP32.  If you have not done this before, you are in for a ride.  If you have not loaded firmware to the ESP32 then I strongly recommend that you get someone that has done it to do it for you.
```
	•	I prefer to use PlatformIO and my github page has the files for PlatformIO. You need to import the project.  
		See “Morse Tutor Arduino.docx” file for guidance
```
2.	Once the device boot up properly and the paddle or key is working. See Bruce Halls website for information on how to use the morse tutor. His instructions for 2-way communication is similar to this new code except this is done across the internet. For details on how the new firmware works see [Firmware Update](https://youtu.be/wOhMsPQrY3k/).
3.	You will need to ensure your Morse Tutor is connect to your PC’s USB and you can open a TTY session.  Arduino calls this the `Serial Monitor`.  The port must be set to 115200 bps.  
4.	Navigate, using the rotatory encoded to the `Config` option and then select `CLI` to enable CLI commands to be entered using the TTY interface.  The following should appear on the console:
```
	CLI v0.1 (c)VE3OOI
	
	:>
```
5.	Here is a list of the commands available:
```
  	•	      C - enter callsign

  	•	      D - dump eeprom

  	•	      E - erase eeprom (DANGERIOUS)

  	•	      I - init eeprom with defaults

  	•	      L - load eeprom & run

  	•	      M - enter server name

  	•	      P - print running config

	•	      P E - print eeprom config

	•	      R - enter room name

  	•	      S - save running config to eeprom

  	•	      U U - enter MQTT username

  	•	      U P - enter MQTT password

  	•	      W S - enter Wi-Fi SSID

  	•	      W P - enter Wi-Fi password
```
6.	You will need to enter your callsign, server name, room name, username, password and of course WIFI SSID and password.
7.	To enter your call sign simply enter C and press return. Then enter your callsign at the prompt. Here is an example:
```
	:> C
  	
	Current: W8BH 
  	
	Call Sign: VE3OOI
  	
	Changing: W8BH to: VE3OOI
```
8.	To enter the server’s name, enter M and press return. Enter ve3ooi.ddns.net as the server name
```  
  	:> M

  	Current: ****

  	Server DNS Name: ve3ooi.ddns.new

  	Changing: **** to: ve3ooi.ddns.new
```
9.The room name is the group name.  Anyone that uses this name will receive and send morse to each other.  You can use any name you want.  If two using have different room names, then will not communicate.  To enter the room name, enter R and press return. Enter moresetutot (the default).
```
  	:> R

  	Current: ****

  	Room Name: morsetutor

  	Changing: **** to: morsetutor
```
10.	Next enter the username and password.  Currently there are two usernames and passwords.  Contact me for this information (drajnauth@rogers.com). 
Here is an example. PLEASE DO NOT USE THIS. It will not work!
```
	:> U U

	Current: ****

	Enter username: dummy

	Changing: **** to: dummy
```
```
	:> U P

	Current: ****

	Enter password: hey here is my password

	Changing: **** to: hey here is my password
```
11.	Repeat similar for WiFi SSID and password.  Enter information associated with your local WiFi
Here is an example. PLEASE DO NOT USE THIS. It will not work!
```
	:> W S

	Current: ****

	Enter SSID: MYWIFI

	Changing: **** to: MYWIFI
```
```
	:> W P
	
	Current: ****
	
	Enter password: MYPASSWORD
	
	Changing: **** to: MYPASSWORD
```
12.	Finally, `S` command to save the information to the EEPROM.  You can print out the information with the `P` command or the `P E` command.  The P command shows what configuration that the tutor is currently running, and the `P E` command show what is stored in EEPROM.  The configuration stored in EEPROM is loaded every time the tutor is restarted.
13.	Once the tutor is configured you can navigate to the `Send` menu and then select `Two-Way` to enable communication with anyone else that is using the room you defined.

