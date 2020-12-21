/*
MQTT NeoPixel Status Multiple Improved
Written by: Alexander van der Sar

website: https://www.vdsar.net/build-status-light-for-devops/
Repository: https://github.com/arvdsar/MQTT_NeoPixel_Status_Multiple_Improved
3D Print design: https://www.thingiverse.com/thing:4665511

This is the improved version of the MQTT_NeoPixel_Status_Multiple. WiFiManager library is replaced with IotWebConf library
This library keeps the configuration portal available so you don't have to reflash to change settings.
It depends on IoTWebConf Libary v3.0.0 (not compatible with 2.x)


IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
and minimize distance between Arduino and first pixel.  Avoid connecting
on a live circuit...if you must, connect GND first.

*/

#define VERSIONNUMBER "v1.0 - 21-12-2020"

#include <ESP8266WiFi.h>        //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <IotWebConf.h>         // https://github.com/prampec/IotWebConf
#include <IotWebConfUsing.h>    // This loads aliases for easier class names.
#ifdef ESP8266
# include <ESP8266HTTPUpdateServer.h>
#elif defined(ESP32)
# include <IotWebConfESP32HTTPUpdateServer.h>
#endif

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
// -- Keep it under 16 characters to handle MQTT compatiblity (thingName+ChipID = unique mqttClientID)
const char thingName[] = "NeoPxLight";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "password";

#define STRING_LEN 128
#define NUMBER_LEN 32
// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "npx1"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
//#define CONFIG_PIN D3

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN

// -- Callback method declarations.
void wifiConnected();
void configSaved();
boolean formValidator();
void handleRoot();
void showLedOffset();
void mqttCallback(char* topic, byte* payload, unsigned int length);

DNSServer dnsServer;
WebServer server(80);
#ifdef ESP8266
ESP8266HTTPUpdateServer httpUpdater;
#elif defined(ESP32)
HTTPUpdateServer httpUpdater;
#endif

WiFiClient espClient;
PubSubClient client(espClient); //MQTT

char mqttServerValue[STRING_LEN];
char mqttUserNameValue[STRING_LEN];
char mqttUserPasswordValue[STRING_LEN];
char mqttTopicValue[STRING_LEN];
char ledOffsetValue[NUMBER_LEN];
char mqttClientId[STRING_LEN];


IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
IotWebConfTextParameter mqttServerParam = IotWebConfTextParameter("MQTT server", "mqttServer", mqttServerValue, STRING_LEN);
IotWebConfTextParameter mqttUserNameParam = IotWebConfTextParameter("MQTT user", "mqttUser", mqttUserNameValue, STRING_LEN);
IotWebConfPasswordParameter mqttUserPasswordParam = IotWebConfPasswordParameter("MQTT password", "mqttPass", mqttUserPasswordValue, STRING_LEN);
IotWebConfTextParameter mqttTopicParam = IotWebConfTextParameter("MQTT Topic", "mqttTopic", mqttTopicValue, STRING_LEN,NULL,"some/thing/#");
IotWebConfNumberParameter ledOffsetParam = IotWebConfNumberParameter("Led Offset", "ledOffset", ledOffsetValue, NUMBER_LEN, "0");

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

int pixel = 0;      //Indicate which Pixel to light
bool inConfig = 0;  //Indicator if you are on config portal or not (for blocking Led Pattern)
long lastMsg = 0;   //timestamp of last MQTT Publish

int ledStateArr[NUMBEROFLEDS]; //Store state of each led 
long previous_time = 0;
long current_time = 0;
int blink = 0;           //to keep track of blinking status (on or off)
bool needReset = false;


//***************************** SETUP ***************************************************
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


  iotWebConf.setStatusPin(STATUS_PIN);
  //iotWebConf.setConfigPin(CONFIG_PIN);
  iotWebConf.addSystemParameter(&mqttServerParam);
  iotWebConf.addSystemParameter(&mqttUserNameParam);
  iotWebConf.addSystemParameter(&mqttUserPasswordParam);
  iotWebConf.addSystemParameter(&mqttTopicParam);
  iotWebConf.addSystemParameter(&ledOffsetParam);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.getApTimeoutParameter()->visible = false; //set to true if you want to specify the timeout in portal

  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  //iotWebConf.setupUpdateServer(&httpUpdater);
  iotWebConf.setupUpdateServer(
    [](const char* updatePath) { httpUpdater.setup(&server, updatePath); },
    [](const char* userName, char* password) { httpUpdater.updateCredentials(userName, password); });

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
  client.setCallback(mqttCallback);

  showLedOffset(); //Display real Led 1 and the Led 1 after offset
  delay(5000); // so you have time to check if the green led is at the right spot.

  //add random string to mqttClientId to make it Unique
   //mqttClientId += String(ESP.getChipId(), HEX); //ChipId seems to be part of Mac Address 

//Create UNIQUE MQTT ClientId - When not unique on the same MQTT server, you'll get strange behaviour
//It uses the unique chipID of the ESP.
sprintf(mqttClientId, "%s%u", thingName,ESP.getChipId()); 
Serial.print("mqttclientid: ");
Serial.println(mqttClientId);
}
//************************ END OF SETUP ********************************************


/*
MQTT Callback function
Determine Topic number and store the payload in ledStateArr (Array)
*/
void mqttCallback(char* topic, byte* payload, unsigned int length) {
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
//**************** END OF MQTT CALLBACK FUNCTION *********************************


void reconnect() {
  // Loop until we're reconnected
  while ((iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE) && !client.connected()) { 
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect(mqttClientId, mqttUserNameValue, mqttUserPasswordValue)) { //mqtt_user, mqtt_pass
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

//******************** START OF LOOP () *****************************************************

void loop() {

  iotWebConf.doLoop();
  client.loop(); //make sure MQTT Keeps running (hopefully prevents watchdog from kicking in)
  delay(10);

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

//Block updating the LEDs while in Configuration portal (inConfig)
if(inConfig == 0) 
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

  if (needReset)
  {
    Serial.println("Rebooting after 1 second.");
    iotWebConf.delay(1000);
    ESP.restart();
  }
/* 
// Publish 'ONLINE' Message to Topic. Uncomment if you want to use this.
  long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
  client.publish("build/test", "ONLINE"); //publish 'ONLINE' message to topic.
  }
  */
}
//******************** END OF LOOP () *****************************************


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
  inConfig = 1; //You are in the Config Portal
  showLedOffset(); //Show real LED1 and your Led 1 at offset

  String s = F("<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>");
  s += iotWebConf.getHtmlFormatProvider()->getStyle();
  s += "<title>MQTT NeoPixel Status Light</title></head><style><body>";
  s += "<H1>";
  s += iotWebConf.getThingName();
  s+= "</H1>";
  s += "<div>MQTT ClientId: ";
  s += mqttClientId;
  s += "</div>";
  s += "<div>MAC address: ";
  s += WiFi.macAddress();
  s += "</div>";
  s += "<div>MQTT Server: ";
  s += mqttServerValue;
  s += "</div>";
  s += "<div>MQTT Topic: ";
  s += mqttTopicValue;
  s += "</div>";
  s += "<div>LED Offset: ";
  s += ledOffsetValue;
  s += "</div>";
  s += "<button type='button' onclick=\"location.href='';\" >Refresh</button>";
  s += "<div>Go to <a href='config'>configure page</a> to change values.</div>";
  s +="<p><div><small>MQTT NeoPixel Status Multiple - Version: ";
  s += VERSIONNUMBER;
  s += " - Get latest version on <a href='https://github.com/arvdsar/MQTT_NeoPixel_Status_Multiple_Improved' target='_blank'>Github</a>.</div>";
  s += "</small></div>";

  s += "</body></html>\n";
  server.send(200, "text/html", s);
}

void wifiConnected()
{
  //needMqttConnect = true; //not using this.
}

void configSaved()
{
  Serial.println("Configuration was updated.");
  showLedOffset(); //Show real LED1 and your Led 1 at offset so you can check the offset
  delay(5000);
  inConfig = 0; // Enable Led Pattern again
  needReset = true; 
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


/*
 Handle led_offset
 Set all leds to blue, next make the original first led Red and then set the 
 led with offset to green. The Green Led should be where YOU want to see LED 1.
*/
void showLedOffset(){

  for(pixel =0;pixel < NUMBEROFLEDS;pixel++)
      strip.setPixelColor(pixel,strip.Color(0 ,0, 255)); //Set all leds to Blue
  strip.setPixelColor(0,strip.Color(255 ,0, 0)); //Set the offical first led to Red.

  pixel = 0 + atoi(ledOffsetValue);
  if(pixel > (NUMBEROFLEDS-1)){
    pixel = pixel - NUMBEROFLEDS;
  }  
  strip.setPixelColor(pixel,strip.Color(0 ,255, 0)); //Set the first led with offset to Green. Ready to go.
  strip.show(); 

}





