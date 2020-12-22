<!-- TOC -->

- [1. MQTT_NeoPixel_Status_Multiple_Improved](#1-mqtt_neopixel_status_multiple_improved)
- [2. Hardware](#2-hardware)
    - [2.1. Required Components](#21-required-components)
    - [2.2. Pinout & wiring](#22-pinout--wiring)
- [3. Device Setup](#3-device-setup)
    - [3.1. Flashing firmware](#31-flashing-firmware)
    - [3.2. Initial setup of the device](#32-initial-setup-of-the-device)
        - [3.2.1. WiFi & MQTT](#321-wifi--mqtt)
        - [3.2.2. Led offset](#322-led-offset)
        - [3.2.3. Led brightness](#323-led-brightness)
    - [3.3. Change configuration](#33-change-configuration)
    - [3.4. OTA Firmware update](#34-ota-firmware-update)
- [4. Controlling the LED's](#4-controlling-the-leds)
    - [4.1. MQTT Topic](#41-mqtt-topic)
    - [4.2. MQTT Payload](#42-mqtt-payload)
    - [4.3. NodeRed](#43-nodered)

<!-- /TOC -->


# 1. MQTT_NeoPixel_Status_Multiple_Improved
Drive Neopixel LEDs based on MQTT Messages. 

Github Repository: [MQTT_NeoPixel_Status_Multiple_Improved ](https://github.com/arvdsar/MQTT_NeoPixel_Status_Multiple_Improved)

Website: [https://www.vdsar.net/build-status-light-for-devops/](https://www.vdsar.net/build-status-light-for-devops/)

Download: [Prebuild firmware binaries](https://vdsar.net/firmware/MQTT%20NeoPixel%20Build%20Light/)

Control each individual LED of a NeoPixel LedRing (or ledstrip) by publishing a color 'green, red, yellow, etc' to a specific MQTT Topic per Led.

You could use it to indicate the build status of a CI/CD build pipeline.

Flash an updated firmware or change configuration using your browser.

This version requires the just released IotWebConf Library v3.x (incompatible with v2.x)

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

The source code for the new version where each led indicates another pipeline. <https://github.com/arvdsar/MQTT_NeoPixel_Status_Multiple_Improved>

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
Power on the device and connect your laptop to the wireless access point `"NeoPxLight"` with password `"password"`. Now visit 192.168.4.1 where you can configure the device.
Be aware that you have to disconnect from this accesspoint before the device connects to your home WiFi. It also takes about 30 seconds after boot before the device switches to WiFi. In these first 30 seconds you can connect to `"NeoPxLight"` if you need to.

The field `AP password` sets you password when you want to access the configuration portal. For what I have experience now, it does not change the default password of the accesspoint of the device. Do not forget this password! If you need to reset it, you have to use wires!

![alt text](https://www.vdsar.net/wordpress/wp-content/uploads/2020/12/SetAPPassword.png "Demo of original position vs offset position")


### 3.2.1. WiFi & MQTT ###
Setup the WiFi by typing your SSID and provide your MQTT details. As topic you use something like: `some/thing/#` (read more below). 
A unique MQTT ClientID is created based on the ChipID. 
Be aware of case sensitive usernames and passwords and auto correction messing this up :-)

### 3.2.2. Led offset ###
To align the first led to the position where you want to see the first led, you can provide the led offset. While you are connected to the configuration page, the device shows the 'real' first LED in red (or green when offset = 0). Now count how many LEDs (clockwise) further you want to position the first LED and enter that value as the offset. After applying the settings you will see the Green LED indicating what will be used as LED 1 for 5 seconds. 
_As long as you have the configuration page open this green and red led will be displayed. Close the webpage (and reset the device) to get it running.

![alt text](https://www.vdsar.net/wordpress/wp-content/uploads/2020/12/ledoffset.jpg "Demo of original position vs offset position")

### 3.2.3. Led brightness ###
You can set the brightness of the Leds on a value between 5 and 200. (if you really want and your powersupply can handle it you could change the max to 255 in the source code).
Each LED Pixel is a Red, Green and Blue led. Each drawing up to 20 mA. So a bright white pixel draws 3 x 20 mA = 60 mA. All 12 LED Pixels on full white means a current of 720 mA.
The Wemos D1 onboard power regulator can handle max 500 mA. So with 200 instead of 255 as max and not using white pixels it should be fine. 

## 3.3. Change configuration ##
Browse to the IP of your device and it will show the current setting and a link to the configuration page. Once you visit this page it will show the Led offset indicator.

## 3.4. OTA Firmware update ##
You can update the firmware from the configuration webpage. 

When you 'Build' the application in PlatformIO, you can find the firmware.bin file in a hidden directory .pio in your project folder (/MQTT_NeoPixel_Status_Multiple/.pio/build/d1_mini/). 

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
