/*
MQTT NeoPixel Status Multiple_Improved
Written by: Alexander van der Sar

website: https://www.vdsar.net/build-status-light-for-devops/
Repository: https://github.com/arvdsar/MQTT_NeoPixel_Status_Multiple_Improved
3D Print design: https://www.thingiverse.com/thing:4665511

This is the improved version of the MQTT_NeoPixel_Status_Multiple. WiFiManager library is replaced with IotWebConf library
This library keeps the configuration portal available.


IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
and minimize distance between Arduino and first pixel.  Avoid connecting
on a live circuit...if you must, connect GND first.

*/

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <IotWebConf.h>            // https://github.com/prampec/IotWebConf

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "testThing";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "password";

#define STRING_LEN 128

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "mqt3"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN D2

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN

// -- Callback method declarations.
void wifiConnected();
void configSaved();
boolean formValidator();
void handleRoot();

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;

WiFiClient espClient;
PubSubClient client(espClient);

char mqttServerValue[STRING_LEN];
char mqttUserNameValue[STRING_LEN];
char mqttUserPasswordValue[STRING_LEN];
char mqttTopicValue[STRING_LEN];
char ledOffsetValue[STRING_LEN];

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
IotWebConfParameter mqttServerParam = IotWebConfParameter("MQTT server", "mqttServer", mqttServerValue, STRING_LEN);
IotWebConfParameter mqttUserNameParam = IotWebConfParameter("MQTT user", "mqttUser", mqttUserNameValue, STRING_LEN);
IotWebConfParameter mqttUserPasswordParam = IotWebConfParameter("MQTT password", "mqttPass", mqttUserPasswordValue, STRING_LEN, "password");
IotWebConfParameter mqttTopicParam = IotWebConfParameter("MQTT Topic", "mqttTopic", mqttTopicValue, STRING_LEN);
IotWebConfParameter ledOffsetParam = IotWebConfParameter("Led Offset", "ledOffset", ledOffsetValue, STRING_LEN);

#define PIN 4 //Neo pixel data pin (GPIO4 / D2)
#define NUMBEROFLEDS 12 //the amount of Leds on the strip
#define blinktime 800 //milliseconds between ON/OFF while blinking


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

int pixel = 0; 

/*
Limit MQTT Retries: KAN WEG?
When MQTT cannot connect, it will keep looping but blocks the firmware updater. 
Limit the max_reconnects so that MQTT will stop trying and you can update the firmware to fix it
retry is once every 5 seconds, so a max_reconnect of 5 means you have to wait 25 seconds before 
the script responds again.
*/
//int reconnectcount = 0;
//int max_reconnect = 5; 


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


int ledStateArr[NUMBEROFLEDS]; //Store state of each led 
long previous_time = 0;
long current_time = 0;
int blink = 0;

void setup() {

  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");


  //Setup Ledstrip
  strip.begin();
  strip.setBrightness(10);
  strip.show(); // Initialize all pixels to 'off'
  
  strip.setPixelColor(0,strip.Color(255 ,0, 0)); //Set the first led of the LedRing to Red; 
  for(int x=1; x<NUMBEROFLEDS;x++){
      strip.setPixelColor(x,strip.Color(0 ,0, 200)); //Set the remaining led to blue; 
  }
  strip.show(); 


  //iotWebConf.setStatusPin(STATUS_PIN);
  //iotWebConf.setConfigPin(CONFIG_PIN);
  iotWebConf.addParameter(&mqttServerParam);
  iotWebConf.addParameter(&mqttUserNameParam);
  iotWebConf.addParameter(&mqttUserPasswordParam);
  iotWebConf.addParameter(&mqttTopicParam);
  iotWebConf.addParameter(&ledOffsetParam);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.setupUpdateServer(&httpUpdater);

  // -- Initializing the configuration.
  boolean validConfig = iotWebConf.init();
  if (!validConfig)
  {
    mqttServerValue[0] = '\0';
    mqttUserNameValue[0] = '\0';
    mqttUserPasswordValue[0] = '\0';
    mqttTopicValue[0] ='\0';
    ledOffsetValue[0] = '\0';
  }

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  //Set MQTT Server and port 
  client.setServer(mqttServerValue, 1883);
  client.setCallback(callback);

  /*
  Handle led_offset
  Set all leds to blue, next make the original first led to Red and then set the 
  led with offset to green
  */
  for(pixel =0;pixel < NUMBEROFLEDS;pixel++)
      strip.setPixelColor(pixel,strip.Color(0 ,0, 255)); //Set all leds to Blue
  strip.setPixelColor(0,strip.Color(255 ,0, 0)); //Set the offical first led to Red.

  pixel = 0 + atoi(ledOffsetValue);
  if(pixel > (NUMBEROFLEDS-1)){
    pixel = pixel - NUMBEROFLEDS;
  }  
  strip.setPixelColor(pixel,strip.Color(0 ,255, 0)); //Set the first led with offset to Green. Ready to go.
  strip.show(); 

delay(5000); // so you have time to check if the green led is at the right spot.

} //end of setup function

/*
MQTT Callback function
Determine Topic number and store the payload in ledStateArr (Array)
*/
void callback(char* topic, byte* payload, unsigned int length) {
  //Serial.print("Message arrived [");
  //Serial.print(topic);
  //Serial.print("] ");

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
  while ((iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE) && !client.connected()) { //stop trying after max_reconnect attempts to stop blocking the script.
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect("TIJDELIJK", mqttUserNameValue, mqttUserPasswordValue)) { //mqtt_user, mqtt_pass
      Serial.println("connected");
      Serial.println(mqttTopicValue);
      client.subscribe(mqttTopicValue); //subscribe to topic
    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }

    
  }
}


long lastMsg = 0;


void loop() {
  // put your main code here, to run repeatedly:
  iotWebConf.doLoop();

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
        pixel = x + atoi(ledOffsetValue);
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

/* 
// Publish 'ONLINE' Message to Topic. Uncomment if you want to use this.
  long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
  client.publish("build/test", "ONLINE"); //publish 'ONLINE' message to topic.
  }
  */
}


/**
 * Handle web requests to "/" path.
 */
void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>MQTT NeoPixel Status Light</title></head><body>MQTT NeoPixel Status Light";
  s += "<ul>";
  s += "<li>MQTT server: ";
  s += mqttServerValue;
  s += "</ul>";
  s += "<ul>";
  s += "<li>MQTT Topic: ";
  s += mqttTopicValue;
  s += "</ul>";
  s += "<ul>";
  s += "<li>LED Offset: ";
  s += ledOffsetValue;
  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change values and update firmware.<p>";
  s += "Check out latest version on <a href='https://github.com/arvdsar/MQTT_NeoPixel_Status_Multiple_Improved' target='_blank'>Github</a>.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

void wifiConnected()
{
  //needMqttConnect = true;
}

void configSaved()
{
  Serial.println("Configuration was updated.");
  //needReset = true;
}

boolean formValidator()
{
  Serial.println("Validating form.");
  boolean valid = true;

  int l = server.arg(mqttServerParam.getId()).length();
  if (l < 3)
  {
    mqttServerParam.errorMessage = "Please provide at least 3 characters!";
    valid = false;
  }

  return valid;
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
