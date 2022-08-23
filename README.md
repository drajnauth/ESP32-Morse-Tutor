# ESP32-Morse-Tutor

This documents the configuration of the W8BH morse code tutor running on an ESP32 microcontroller that supports WiFi.  This is NOT applicable to the STM32 version.
1.	Load the new firmware onto the ESP32.  If you have not done this before, you are in for a ride.  If you have not loaded firmware to the ESP32 then I strongly recommend that you get someone that has done it to do it for you.

	* I prefer to use PlatformIO and my github page has the files for PlatformIO. With PlatformIO, you need to import the project as from the GitHub repo. See “Morse Tutor Arduino.docx” file for guidance on loading with Arduino IDE.

2.	Once the device boots up properly and the paddle or key is working. See Bruce Hall's website for information on how to use the morse tutor (as a tutor). His instructions for 2-way communication is similar to my new code except communication is done across the internet. For details on how the new firmware works see [Firmware Update YouTube Video](https://youtu.be/wOhMsPQrY3k/).

	<a href="http://www.youtube.com/watch?feature=player_embedded&v=wOhMsPQrY3k" target="_blank"><img src="http://img.youtube.com/vi/wOhMsPQrY3k/0.jpg" width="240" height="180" border="10" /></a>

4.	You will need to ensure your Morse Tutor is connected to your PC’s USB and you can open a TTY session.  Arduino calls this the `Serial Monitor`.  The port must be set to 115200 bps.  
5.	Navigate, using the rotary encoder to the `Config` option and then select `CLI` to enable CLI commands to be entered using the TTY interface.  The following should appear on the console:
```
	CLI v0.1 (c)VE3OOI
	
	:>
```
5.	Here is a list of the commands available that are case insensitive:
```
  	C - enter callsign

  	D - dump eeprom

  	E - erase eeprom (DANGERIOUS)

  	I - init eeprom with defaults (similar to factory defaults)

  	L - load eeprom & run

  	M - enter server name

  	P - print running config

	P E - print eeprom config

	R - enter room name

  	S - save running config to eeprom

  	U U - enter MQTT username

  	U P - enter MQTT password

  	W S - enter Wi-Fi SSID

  	W P - enter Wi-Fi password
```
6.	You will need to enter your callsign, server name, room name, username, password and of course WIFI SSID and password.
7.	To enter your call sign simply enter `C` and press return. Then enter your callsign at the prompt. Here is an example:
```
	:> C
  	
	Current: W8BH 
  	
	Call Sign: VE3OOI
  	
	Changing: W8BH to: VE3OOI
```
8.	To enter the server’s name, enter `M` and press return. Enter `mqtt.ve3ooi.ca` as the server name
```  
  	:> M

  	Current: ****

  	Server DNS Name: mqtt.ve3ooi.ca

  	Changing: **** to: mqtt.ve3ooi.ca
```
9.The room name (MQTT Topic) is the group name that can communicate together.  All tutors that use this name will receive and send morse to each other.  You can use any name you want.  However, all tutors you want to communicated with must also use this name.  To enter the room name, enter `R` and press return. Enter `moresetutot` (the default).
```
  	:> R

  	Current: ****

  	Room Name: morsetutor

  	Changing: **** to: morsetutor
```
10.	Next enter the MQTT username and password.  Currently there are two usernames and passwords.  Contact me for this information (drajnauth@rogers.com). 
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
12.	Finally, enter the `S` command to save the information to the EEPROM.  If you don't do this, all the information you entered will be lost next time to restart the tutor.  You can print out the configuration information with the `P` command or the `P E` command.  The `P` command shows what configuration that the tutor is currently running, and the `P E` command show what is stored in EEPROM.  The configuration stored in EEPROM is loaded every time the tutor is restarted.
13.	Once the tutor is configured you can navigate to the `Send` menu and then select `Two-Way` to enable communication with anyone else that is using the room you defined.

