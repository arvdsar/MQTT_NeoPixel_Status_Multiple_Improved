/*
MQTT NeoPixel Status Multiple
Written by: Alexander van der Sar

website: https://www.vdsar.net/build-status-light-for-devops/
Repository: https://github.com/arvdsar/MQTT_NeoPixel_Status_Multiple
3D Print design: https://www.thingiverse.com/thing:4665511

Before using this code, change the update_username and update_password to secure uploading firmware through the webbrowser.

To reset filesystem & wifi settings, search for below two statements and uncomment those. Flash the firmware, wait a couple of seconds 
comment them again, flash and then your settings are reset.

//LittleFS.format();
//wifiManager.resetSettings();


IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
and minimize distance between Arduino and first pixel.  Avoid connecting
on a live circuit...if you must, connect GND first.

BE AWARE: This version only works with ArduinoJSON library v 5.x and not with 6.x 

*/

//#include <FS.h>                   //Replaced by LittleFS.h - this needs to be first, or it all crashes and burns...
#include "LittleFS.h"               // LittleFS is declared

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>


#include <WiFiClient.h> //ota
#include <ESP8266mDNS.h> //ota
#include <ESP8266HTTPUpdateServer.h> //ota

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define PIN 4 //Neo pixel data pin (GPIO4 / D2)
#define NUMBEROFLEDS 12 //the amount of Leds on the strip
#define blinktime 800 //milliseconds between ON/OFF while blinking

//OTA Webbrowser
const char* host = "esp8266-webupdate";
const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "admin";

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMBEROFLEDS, PIN, NEO_GRB + NEO_KHZ400);

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.


//flag for saving data
bool shouldSaveConfig = false;

int pixel = 0; 

/*
Limit MQTT Retries:
When MQTT cannot connect, it will keep looping but blocks the firmware updater. 
Limit the max_reconnects so that MQTT will stop trying and you can update the firmware to fix it
retry is once every 5 seconds, so a max_reconnect of 5 means you have to wait 25 seconds before 
the script responds again.
*/
int reconnectcount = 0;
int max_reconnect = 5; 

//define your default values here, if there are different values in config.json, they are overwritten.
#define mqtt_server       "xxx.cloudmqtt.com"
#define mqtt_clientname   "build_YourNameHere"
#define mqtt_port         "1883"
#define mqtt_user         "mqtt user"
#define mqtt_pass         "mqtt pass"
#define mqtt_topic        "topic/something/#" // add: /# so you automatically subscribe to subtopics!
#define led_offset        "0"

WiFiClient espClient;
PubSubClient client(espClient);

//Webbrowser OTA
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

/* Initialize variables for light patterns (see commented code at the end)
void colorWipe(uint32_t c, uint8_t wait);
void rainbow(uint8_t wait);
void rainbowCycle(uint8_t wait);
void theaterChase(uint32_t c, uint8_t wait);
void theaterChaseRainbow(uint8_t wait);
uint32_t Wheel(byte WheelPos);
void colorDot(uint32_t c, uint8_t wait);
*/

void callback(char* topic, byte* payload, unsigned int length);

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

int ledStateArr[NUMBEROFLEDS]; //Store state of each led 
long previous_time = 0;
long current_time = 0;
int blink = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  //Setup Ledstrip
  strip.begin();
  strip.setBrightness(10);
  strip.show(); // Initialize all pixels to 'off'
  
  strip.setPixelColor(0,strip.Color(255 ,0, 0)); //Set the first led of the LedRing to Red; 
  for(int x=1; x<NUMBEROFLEDS;x++){
      strip.setPixelColor(x,strip.Color(0 ,0, 200)); //Set the remaining led to blue; 
  }
  strip.show(); 

  //clean FS for testing 
  //LittleFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (LittleFS.begin()) {
    Serial.println("mounted file system");
    if (LittleFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_clientname, json["mqtt_clientname"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_pass, json["mqtt_pass"]);
          strcpy(mqtt_topic, json["mqtt_topic"]);
          strcpy(led_offset, json["led_offset"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read



  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_clientname("clientname", "mqtt clientname", mqtt_clientname, 20);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 20);
  WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", mqtt_pass, 20);
  WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", mqtt_topic, 20);
  WiFiManagerParameter custom_led_offset("led offset", "led offset", led_offset, 3);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

 //Reset Wifi settings for testing  
 //wifiManager.resetSettings();

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_clientname);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);
  wifiManager.addParameter(&custom_mqtt_topic);
  wifiManager.addParameter(&custom_led_offset);


  //set minimum quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("WiFi connected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_clientname, custom_mqtt_clientname.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pass, custom_mqtt_pass.getValue());
  strcpy(mqtt_topic, custom_mqtt_topic.getValue());
  strcpy(led_offset, custom_led_offset.getValue());


  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_clientname"]= mqtt_clientname;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;
    json["mqtt_topic"] = mqtt_topic;
    json["led_offset"] = led_offset;


    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

//Webbrowser OTA
  MDNS.begin(host);
  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local%s in your browser\n", host,update_path);

  //Set MQTT Server and port 
  client.setServer(mqtt_server, atoi(mqtt_port));
  client.setCallback(callback);

  //Handle led_offset
  pixel = 0 + atoi(led_offset);
  if(pixel > (NUMBEROFLEDS-1)){
    pixel = pixel - NUMBEROFLEDS;
  }  
  strip.setPixelColor(pixel,strip.Color(0 ,255, 0)); //Set the first led with offset to Green. Ready to go.
  strip.show(); // Initialize all pixels to 'off'


delay(5000); // so you have time to check if the green led is at the right spot.

} //end of setup function

//MQTT Callback function
void callback(char* topic, byte* payload, unsigned int length) {
 // Serial.print("Message arrived [");
 // Serial.print(topic);
 // Serial.print("] ");

  int LedId = 0;

  //you should subscribe to topics like topic/# or topic/subtopic/#
  //This will result in topics like: topic/subtopic/0, topic/subtopic/1 where the number corresponds with the LED
  //ledStateArr[LedId] will contain the led status (what color you want) per led.
  
  char *token = strtok(topic, "/"); //split on /
    // Keep printing tokens while one of the 
    // delimiters present in str[]. 
    while (token != NULL) 
    { 
        LedId = atoi(token); 
        token = strtok(NULL, "/"); //break the while. LedId contains the last token
    } 

  payload[length] = '\0';
 
  /*
  Define color codes and with or without blink:
  green - greenblink (1 / 2)
  red - redblink (3 / 4)
  yellow - yellowblink (5 / 6)
  purple - purpleblink (7 /8)
  blue - blueblink (9 / 10)
  orange - orangeblink (11 / 12)
  off (0)

  */
  //check for possible topics
  if(strcmp((char*)payload,"green") == 0){ 
      ledStateArr[LedId] = 1;
    }
  else if(strcmp((char*)payload,"greenblink") == 0){ 
      ledStateArr[LedId] = 2;
    }
  else if(strcmp((char*)payload,"red") == 0){
          ledStateArr[LedId] = 3;
    }
  else if(strcmp((char*)payload,"redblink") == 0){
          ledStateArr[LedId] = 4;
    }
  else if(strcmp((char*)payload,"yellow") == 0){
          ledStateArr[LedId] = 5; 
  }
    else if(strcmp((char*)payload,"yellowblink") == 0){
          ledStateArr[LedId] = 6; 
  }
  else if(strcmp((char*)payload,"purple") == 0){
        ledStateArr[LedId] = 7;
    }
  else if(strcmp((char*)payload,"purpleblink") == 0){
        ledStateArr[LedId] = 8;
    }
  else if(strcmp((char*)payload,"blue") == 0){
          ledStateArr[LedId] = 9;
          }
  else if(strcmp((char*)payload,"blueblink") == 0){
          ledStateArr[LedId] = 10;
          }
  else if(strcmp((char*)payload,"orange") == 0){
          ledStateArr[LedId] = 11; 
  }
    else if(strcmp((char*)payload,"orangeblink") == 0){
          ledStateArr[LedId] = 12; 
  }
    else if(strcmp((char*)payload,"off") == 0){
          ledStateArr[LedId] = 0; 
  }
}



void reconnect() {
  // Loop until we're reconnected
  while (!client.connected() && reconnectcount < max_reconnect) { //stop trying after max_reconnect attempts to stop blocking the script.
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect(mqtt_clientname, mqtt_user, mqtt_pass)) { //mqtt_user, mqtt_pass
      Serial.println("connected");
      Serial.println(mqtt_topic);
      client.subscribe(mqtt_topic); //subscribe to topic
      reconnectcount = 0; //reset the reconnectcounter
    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      reconnectcount++;
      delay(5000);
    }

    
  }
}


long lastMsg = 0;


void loop() {
  // put your main code here, to run repeatedly:

  //Webbrowser OTA
  httpServer.handleClient(); 
  MDNS.update(); 

 /*
  DRIVE THE LEDS
  green - greenblink (1 / 2)
  red - redblink (3 / 4)
  yellow - yellowblink (5 / 6)
  purple - purpleblink (7 /8)
  blue - blueblink (9 / 10)
  orange - orangeblink (11 / 12)
  off (0)
  */

 for (int x=0;x<NUMBEROFLEDS;x++){ //loop through all leds and set the required color (R,G,B)
      client.loop(); //make sure MQTT Keeps running (hopefully prevents watchdog from kicking in)

        //Handle led_offset
        pixel = x + atoi(led_offset);
        if(pixel > (NUMBEROFLEDS-1)){
            pixel = pixel - NUMBEROFLEDS;
        }
  
  //GREEN
    if(ledStateArr[x] == 1) //GREEN
      strip.setPixelColor(pixel,strip.Color(0,255, 0)); //led on

    else if(ledStateArr[x] == 2){ //GREEN BLINKING
        if(blink == 1)
          strip.setPixelColor(pixel,strip.Color(0 ,255, 0)); //led on
        else if(blink == 0)
             strip.setPixelColor(pixel,strip.Color(0,0, 0)); //led off
        }
  //RED
  else if(ledStateArr[x] == 3) //RED
        strip.setPixelColor(pixel,strip.Color(255,0, 0)); //led on
  else if(ledStateArr[x] == 4){ //RED BLINKING
        if(blink == 1)
          strip.setPixelColor(pixel,strip.Color(255 ,0, 0)); //led on
        else if(blink == 0)
             strip.setPixelColor(pixel,strip.Color(0,0, 0)); //led off
        }
  //YELLOW     
  else if(ledStateArr[x] == 5) //YELLOW
        strip.setPixelColor(pixel,strip.Color(128,128, 0)); //led on
  else if(ledStateArr[x] == 6){ //YELLOW BLINKING)
        if(blink == 1)
          strip.setPixelColor(pixel,strip.Color(128,128, 0)); //led on
        else if(blink == 0)
             strip.setPixelColor(pixel,strip.Color(0,0, 0)); //led off
        }
//PURPLE
    else if(ledStateArr[x] == 7) //PURPLE
        strip.setPixelColor(pixel,strip.Color(128,0, 128)); //led on
     else if(ledStateArr[x] == 8){ //PURPLE BLINKING)
        if(blink == 1)
          strip.setPixelColor(pixel,strip.Color(128,0, 128)); //led on
        else if(blink == 0)
             strip.setPixelColor(pixel,strip.Color(0,0, 0)); //led off
        }
  //BLUE
    else if(ledStateArr[x] == 9) //BLUE
        strip.setPixelColor(x,strip.Color(0,0, 255)); //led on
    else if(ledStateArr[x] == 10){ //BLUE BLINKING)
        if(blink == 1)
          strip.setPixelColor(pixel,strip.Color(0,0, 255)); //led on
        else if(blink == 0)
             strip.setPixelColor(pixel,strip.Color(0,0, 0)); //led off
        }
    //ORANGE
    else if(ledStateArr[x] == 11) //BLUE
        strip.setPixelColor(pixel,strip.Color(255,128, 0)); //led on
    else if(ledStateArr[x] == 12){ //ORANGE (needs blinking)
        if(blink == 1)
          strip.setPixelColor(pixel,strip.Color(255,128, 0)); //led on
        else if(blink == 0)
             strip.setPixelColor(pixel,strip.Color(0,0, 0)); //led off
    }
    //OFF
     else if(ledStateArr[x] == 0) //LED OFF
        strip.setPixelColor(pixel,strip.Color(0,0, 0)); //led off

  } //end for-loop

strip.show(); //set all pixels  
 
 //Handle blinking of leds by switching blink value every x-milliseconds (blinktime)
 current_time = millis();
 if(current_time > previous_time + blinktime){
   if (blink == 0)
    blink = 1;
    else 
      blink = 0;
    previous_time = millis(); //set current time
 }
   
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(10);

  long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
 // client.publish("build/test", "ONLINE"); //publish 'ONLINE' message to topic.

  }
}



//  Some example procedures showing how to display to the pixels:

/*
 colorWipe(strip.Color(255, 0, 0), 100); // Red
 colorDot(strip.Color(255,255,0),500);
 colorWipe(strip.Color(0, 255, 0), 100); // Green
 colorWipe(strip.Color(0, 0, 255), 100); // Blue
 colorWipe(strip.Color(0, 0, 0, 255), 50); // White RGBW
 Send a theater pixel chase in...
 theaterChase(strip.Color(127, 127, 127), 100); // White
 theaterChase(strip.Color(0, 0, 127), 200); // Red
 theaterChase(strip.Color(0, 0, 127), 100); // Blue

 rainbow(20);
 rainbowCycle(20);
 theaterChaseRainbow(50);
 */

/* 
//Some functions with light effects for possible future use.

//All kind of functions for the neopixels
// Fill the dots one after the other with a color
void colorDot(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.setPixelColor(i-1,strip.Color(0,0,0));
    strip.show();
    delay(wait);
  }

}


// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, c);    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, strip.Color(0,0,0));        //turn every third pixel off
      }
    }
  }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}


// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
*/
