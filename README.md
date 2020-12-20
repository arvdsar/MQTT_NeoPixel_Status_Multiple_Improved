<!-- TOC -->

- [1. MQTT_NeoPixel_Status_Multiple](#1-mqtt_neopixel_status_multiple)
- [2. Hardware](#2-hardware)
    - [2.1. Required Components](#21-required-components)
    - [2.2. Pinout & wiring](#22-pinout--wiring)
- [3. Device Setup](#3-device-setup)
    - [3.1. Flashing firmware](#31-flashing-firmware)
    - [3.2. Initial setup of the device](#32-initial-setup-of-the-device)
        - [3.2.1. WiFi & MQTT](#321-wifi--mqtt)
        - [3.2.2. Led offset](#322-led-offset)
    - [3.3. Reset the settings](#33-reset-the-settings)
    - [3.4. OTA Firmware update](#34-ota-firmware-update)
- [4. Controlling the LED's](#4-controlling-the-leds)
    - [4.1. MQTT Topic](#41-mqtt-topic)
    - [4.2. MQTT Payload](#42-mqtt-payload)
    - [4.3. NodeRed](#43-nodered)

<!-- /TOC -->


# 1. MQTT_NeoPixel_Status_Multiple
Drive Neopixel LEDs based on MQTT Messages. 

Github Repository: [MQTT_NeoPixel_Status_Multiple ](https://github.com/arvdsar/MQTT_NeoPixel_Status_Multiple)

Website: [https://www.vdsar.net/build-status-light-for-devops/](https://www.vdsar.net/build-status-light-for-devops/)

Control each individual LED of a NeoPixel LedRing (or ledstrip) by publishing a color 'green, red, yellow, etc' to a specific MQTT Topic per Led.

You could use it to indicate the build status of a CI/CD build pipeline.

Flash an updated firmware using your browser.


![alt text](https://www.vdsar.net/wordpress/wp-content/uploads/2020/12/IMG_3071-1.jpeg "Build Status Light")



# 2. Hardware #

## 2.1. Required Components ##
* Wemos D1 Mini 4Mb (ESP8266) Eur: 4,-
* NeoPixel 12 pixel LED Ring from AliExpress (has a different size than the original from Adafruit) Eur: 3,-
* 1000uF Capacitor
* 470 Ohm resistor
* 3D Printer, solder iron and a couple of wires.

The case can be 3D printed using PLA. The STL and Fusion360 files can be found here: <https://www.thingiverse.com/thing:4665511>

The source code for the initial version where all leds indicate the status of one pipeline: <https://github.com/arvdsar/MQTT_NeoPixel_Status>

The source code for the new version where each led indicates another pipeline. <https://github.com/arvdsar/MQTT_NeoPixel_Status_Multiple>

## 2.2. Pinout & wiring ##
* Connect 5V of LedRing with 5V on Wemos
* Connect GND of LedRing with GND on Wemos
* Connect DI (data in) of LedRing with D2 on Wemos (GPIO4)
* Put the 1000uF capacitor in parallel with the LedRing (so the + on 5v and – on the GND. _An Electrolytic capacitor has a + and – pin so pay attention!_
* Add the resistor to the data input of the LedRing

An additional power supply is not required (for 12 Pixel ledring) as long as you don’t turn on all leds on white full power.

# 3. Device Setup #

## 3.1. Flashing firmware ##
Flashing the firmware can be done using [PlatformIO](https://platformio.org) which is an OpenSource Embedded development platform with a great plugin for Visual Studio Code. Install PlatformIO in Visual studio Code and clone this repository to a local folder.  The repository is setup for use with PlatformIO and will take care of required libraries and such.

The platformio.ini is setup for both Wemos D1-MINI (4Mb) and D1-MINI-PRO (16Mb).

Never used PlatformIO? Check this page: [PlatformIO - How to flash firmware](https://www.vdsar.net/platformio-flash-firmware)

## 3.2. Initial setup of the device ##
Power on the device and connect your laptop to the wireless access point “AutoConnectAP” with password "password". Now visit 192.168.4.1 where you can configure the device.

### 3.2.1. WiFi & MQTT ###
Setup to WiFi and provide your MQTT details. As topic you use something like: `some/thing/#` (read more below). 
Be aware to use a unique ClientID otherwise you'll get connection errors and weird behavior. Be aware of case sensitive usernames and passwords and your computer auto correction. :-)

### 3.2.2. Led offset ###
You can also provide a led offset. This is to align the first led to the position where you want to see the first led. At start of the device the 'real' first LED is red. Now count how many LEDs (clockwise) further you want to position the first LED and enter that value as the offset. After succesful boot of the device and connected to WiFi you will see the 'moved' LED 1 in green for 5 seconds. 


![alt text](https://www.vdsar.net/wordpress/wp-content/uploads/2020/12/ledoffset.jpg "Demo of original position vs offset position")

Once the device can connect to your WiFi it will never start this configuration panel anymore. The only way to change your MQTT settings or led offset is to either turn of the WiFi at home so it cannot connect or to reset the settings.

## 3.3. Reset the settings ##
The configuration panel is only showed when the device cannot connect to a WiFi network. To reset all the settings you have a couple of options:
* Disable your home WiFi so the device cannot connect
* Flash the device with reset statements

Update the source code by uncommenting the following two statements. 

``` C  
//LittleFS.format();
//wifiManager.resetSettings();
```

Now flash the updated firmware, wait a couple of seconds, comment the statements and flash the firmware again. Now you are able to configure the WiFi and MQTT settings again by following 'initial setup of the device'

_This procedure also works using OTA Firmware. Continue reading._

## 3.4. OTA Firmware update ##
As long as your device can connect to WiFi you can update the firmware via your webbrowser. Visit http://ip-address/firmware or http://esp8266-webupdate.local/firmware and you can upload a firmware file. The default user/password is admin/admin (change it in the code to make it more secure).

When you 'Build' the application in PlatformIO, you can find the firmware.bin file in a hidden directory .pio in your project folder (/MQTT_NeoPixel_Status_Multiple/.pio/build/d1_mini/). Build one with and without the LittleFS.format() and wifiManager.resetSettings(); uncommented so you can easily reset settings by flashing via the browser.

Flash your firmware with the 'reset' firmware. Now you can connect to the device accesspoint “AutoConnectAP”, setup the WiFi connection (and the MQTT settings if you want) and reboot. Now you can connect to the firmware update page and flash the final version. Connect again to the “AutoConnectAP” and setup WiFi, MQTT and Led_offset again. 

_Pay attention: If the device cannot connect to the provided MQTT server it blocks the firmware update page for about 25 seconds. So be patient and wait 30 seconds. After that time the device stops retrying and the firmware update page becomes available._ 

![alt text](https://www.vdsar.net/wordpress/wp-content/uploads/2020/12/firmwares-1024x585.png "Firmware.bin location")

# 4. Controlling the LED's #

## 4.1. MQTT Topic ##
Subscribe the device to a topic like: `some/thing/#`

It is very important to add the `/#` at the end. This way the device receives all messages of all topics below `some/thing`. Now each led will respond to its own topic:

Led 1 --> `some/thing/1`

Led 2 --> `some/thing/2`

Led 3 --> `some/thing/3`

...

Led 12 --> `some/thing/12`

## 4.2. MQTT Payload ##
To set the color of a LED you send a specific payload to the MQTT Topic of that LED. It is case sensitive. 
You can choose to have a static LED or blinking LED.
  * green 
  * greenblink 
  * red 
  * redblink 
  * yellow
  * yellowblink 
  * purple
  * purpleblink
  * blue
  * blueblink
  * orange
  * orangeblink 
  * off


  So, to make LED 5 Blinking purple you send: `purpleblink` to topic: `some\thing\5`

## 4.3. NodeRed ##
I use [NodeRed](https://nodered.org) to listen to all kind of statusses of Domotica or IoT sensors and then act upon that status by sending MQTT Messages to the device. 