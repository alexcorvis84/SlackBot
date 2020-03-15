/*
   DOIT ESP32 DevKitV1 Real-Time Slack Bot
   Copyright (C) 2020, Alejandro Juan Garcia (@AlexCorvis84)
   EmpathyLabs [https://www.empathy.co/]
   Licensed under the MIT License
*/

#include <Arduino.h>
#include <time.h>
#include <HTTPClient.h>
#include <WebSocketsClient.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>


//-------------------------------------------------------------------------------------------------
//Slack config
//-------------------------------------------------------------------------------------------------
// If Slack changes their SSL, you would need to update with the new SHA-1 fingerprint value
#define SLACK_SSL_FINGERPRINT "C1 0D 53 49 D2 3E E5 2B A2 61 D5 9E 6F 99 0D 3D FD 8B B2 B3"

// 1) Get token by creating new App at https://api.slack.com/apps
// 2) Add a Bot User to your APP
// 3) Then install your App to your Slack workspace to generate the 'Bot User OAuth Access Token'
// 4) Set your OAuth Scope: channels:read / chat:write:bot / bot / incoming-webhook

// MakersAsturias Bot OAuth Token 
#define SLACK_BOT_TOKEN "xoxb-722046927040-901972392550-sGoJOUZpxp9qdkj5AA27jDXq"

// Empathy Bot OAuth Token 
//#define SLACK_BOT_TOKEN "xoxb-4865966397-966266296320-pMcRTdiNSpnQMfRQN2QhUXRO"

// Locate your Member ID in your Slack Space:
// You can find it in Profile & account -> ... (vertical 3dots) -> Copy Member ID
// Member ID MakersAturias 
#define MEMBERID "<@UM81F0N0Y>"

// Member ID Empathy.co 
//#define MEMBERID "<@UPV0X6QTE>"
//-------------------------------------------------------------------------------------------------
//Neopixel attached config
//-------------------------------------------------------------------------------------------------
#define LEDS_NUMPIXELS  16
#define LEDS_PIN        32
Adafruit_NeoPixel pixels(LEDS_NUMPIXELS, LEDS_PIN, NEO_GRB + NEO_KHZ800);
//-------------------------------------------------------------------------------------------------
//Wifi & WSc config
//-------------------------------------------------------------------------------------------------
WebSocketsClient webSocket;
WiFiManager wm;

//-------------------------------------------------------------------------------------------------
//RTC timezone settings config
//-------------------------------------------------------------------------------------------------
const int timezone = 1; // timezone offset CET (UTC+1)
const int dst = 0; //daylight savings
//-------------------------------------------------------------------------------------------------
//Variables
//-------------------------------------------------------------------------------------------------
bool connected = false; // Check Slack connection
int servoPin = 26; // Possible PWM GPIO pins on the ESP32: 0(used by on-board button),2,4,5(used by on-board LED),12-19,21-23,25-27,32-33 
Servo myservo; // create servo object to control a servo
unsigned status;  // Check I2C sensor
Adafruit_BME280 bme; // I2C SDA GPIO21 - SCL GPIO22 - GND - 3.3V
const int LEDPin = 27;        // pin para el LED
const int PIRPin = 35;         // pin de entrada (for PIR sensor) pins 34 to 39 can be used as input only
int pirState = LOW; 
//-------------------------------------------------------------------------------------------------


/* 
 *  NEOPIXELs functions to manage notifications
 */
 void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<pixels.numPixels(); i++) { // For each pixel in strip...
    pixels.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    pixels.show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
  }
}

// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void rainbow(int wait) {
  // Hue of first pixel runs 5 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
  // means we'll make 5*65536/256 = 1280 passes through this outer loop:
  for(long firstPixelHue = 0; firstPixelHue < 5*65536; firstPixelHue += 256) {
    for(int i=0; i<pixels.numPixels(); i++) { // For each pixel in strip...
      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      int pixelHue = firstPixelHue + (i * 65536L / pixels.numPixels());
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
      pixels.setPixelColor(i, pixels.gamma32(pixels.ColorHSV(pixelHue)));
    }
    pixels.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
  }
}

// Theater-marquee-style chasing lights. Pass in a color (32-bit value,
// a la strip.Color(r,g,b) as mentioned above), and a delay time (in ms)
// between frames.
void theaterChase(uint32_t color, int wait) {
  for(int a=0; a<10; a++) {  // Repeat 10 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      pixels.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in steps of 3...
      for(int c=b; c<pixels.numPixels(); c += 3) {
        pixels.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      pixels.show(); // Update strip with new contents
      delay(wait);  // Pause for a moment
    }
  }
}

//-------------------------------------------------------------------------------------------------
void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

//-------------------------------------------------------------------------------------------------
/*
  Gets the different words separated by spaces from text received via WSc
*/
String getValue(String data, char separator, int index){
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length();

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

/*
  Sends a reply message to Slack with the text sensor values
*/
void sendWConditions(String channel) {
  
  DynamicJsonDocument  doc(1024);
  doc["type"] = "message";

  String msg_condT = "Room Temperature: ";
  String msg_temp = msg_condT + bme.readTemperature() + " ºC";
  String msg_condH = msg_temp + "\nRoom Humidity: ";
  String msg_condR = msg_condH + bme.readHumidity() + " %";
  String msg_complete = msg_condR;
  Serial.println(msg_complete);

  if(pirState == LOW){
    msg_complete = msg_condR + "\nRoom State: :heavy_check_mark:" ;
  }
  else{
    msg_complete = msg_condR + "\nRoom State: :x:" ;
  }
  
  doc["text"] = msg_complete;
  doc["channel"] = channel;

  String json;
  serializeJson(doc, Serial);
  Serial.println("\nReplying JSON message via WSc");
  serializeJson(doc, json);
  webSocket.sendTXT(json);
}

/*
  Process the word command
*/
bool process_command(String text, String channel){

  // Split the message words by spaces and looks for @MEMBERID in case you want personal notifications
  for(int i=0; i < text.length(); i++){
    
       String  var = getValue(text, ' ', i);
       if (var == MEMBERID){
          Serial.print("User "+ var + " found!");
          rainbow(20);
       }
  }

  if (text == "red"){
       colorWipe(pixels.Color(255,   0,   0), 50);
      return true;
  }
  if (text == "green"){
       colorWipe(pixels.Color(0,   255,   0), 50);
      return true;
  }
  if (text == "blue"){
       colorWipe(pixels.Color(0,   0,   255), 50);
      return true;
  }
  if (text == "off"){
      pixels.clear();
       colorWipe(pixels.Color(0,   0,   0), 50);
      return true;
  }
  if (text == "on"){
      pixels.clear();
        theaterChase(pixels.Color(127, 127, 127), 50);
        theaterChase(pixels.Color(127, 0, 0), 50);
        theaterChase(pixels.Color(0, 0, 127), 50);
        colorWipe(pixels.Color(0,   0,   0), 50);
      return true;
  }
  if (text == ":door:"){
      myservo.write(45);
      delay(200); // wait for the servo to get position
      myservo.write(0);
      delay(200);
      return true;
  }
  if (text == "room_status"){
        sendWConditions(channel);
        return true;
  }
  else{
     Serial.println("Command not recognized!!");
     return false;
  } 
}

/*
  Sends a reply message to Slack with the text command received
*/
void sendReply(String var, String channel) {

  //Serial.println(var);
  DynamicJsonDocument  doc(1024);
  doc["type"] = "message";
  doc["text"] = "Command Received: "+var;
  doc["channel"] = channel;

  String json;
  serializeJson(doc, Serial);
  Serial.println("\nReplying JSON message via WSc");
  serializeJson(doc, json);
  webSocket.sendTXT(json);
}

/*
  Process the slack message received accordingly. 
  If recognized Bot will reply back with the command received.
*/
void processSlackMessage(String receivedpayload){

  bool recogn_comm = false;
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, receivedpayload);

  DeserializationError error = deserializeJson(doc, receivedpayload);
  if (error) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
    return;
  }

  JsonObject obj = doc.as<JsonObject>();

  String type_msg = obj["type"];
  String texto = obj["text"];
  String channel = obj["channel"];


  if(type_msg.equals("message")){
    Serial.println(type_msg + " "+ channel + " " + texto);
    recogn_comm = process_command(texto, channel);
  }

  if(recogn_comm){
    sendReply(texto, channel);
  }
  
}

/*
  Called on each web socket event. 
  Handles disconnection, and incoming messages from Slack.
*/
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length){
  
  switch (type)
  {
    case WStype_DISCONNECTED:
      Serial.printf("[WebSocket] Disconnected :-( \n");
      connected = false;
      break;

    case WStype_CONNECTED:
      Serial.printf("[WebSocket] Connected to: %s\n", payload);
      //sendReply();
      break;

    case WStype_TEXT:
      Serial.printf("[WebSocket] Message: %s\n", payload);
      // send message to 
      //webSocket.sendTXT("message here");
      
      String receivedpayload;
      for (int i = 0; i < length; i++)
      {
         receivedpayload += (char)payload[i];
      }

      processSlackMessage(receivedpayload);
      break;
   
    /*case WStype_PING:
      // pong will be send automatically
      Serial.printf("[WSc] get ping\n");
      break;
      
    case WStype_PONG:
        // answer to a ping we send
        Serial.printf("[WSc] get pong\n");
        break;*/
  }
}

/*
  Establishes a bot connection to Slack:
  1. Performs a REST call to get the WebSocket URL
  2. Conencts the WebSocket
  Returns true if the connection was established successfully.
*/

bool connectToSlack() {
  // Step 1: Find WebSocket address via RTM API (https://api.slack.com/methods/rtm.connect)
  HTTPClient http;
  http.begin("https://slack.com/api/rtm.connect?token=" SLACK_BOT_TOKEN, SLACK_SSL_FINGERPRINT);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed with code %d\n", httpCode);
    return false;
  }

  WiFiClient *client = http.getStreamPtr();
  client->find("wss:\\/\\/");
  String host = client->readStringUntil('\\');
  String path = client->readStringUntil('"');
  path.replace("\\/", "/");

  // Step 2: Open WebSocket connection and register event handler
  Serial.println("WebSocket Host=" + host + " Path=" + path);
  webSocket.beginSSL(host, 443, path, "", "");
  webSocket.onEvent(webSocketEvent);

  webSocket.setReconnectInterval(30000);
   // start heartbeat (optional)
   // ping server every 15000 ms
   // expect pong from server within 3000 ms
   // consider connection disconnected if pong is not received 2 times
   webSocket.enableHeartbeat(15000, 3000, 2);
   return true;
}

void setup() {
  
  Serial.begin(115200);
  pinMode(LEDPin, OUTPUT); 
  pinMode(PIRPin, INPUT);

  status = bme.begin(0x76);
  if (!status) {
        Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
        Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(),16);
  }

  pixels.begin();
  pixels.show();
  pixels.setBrightness(50);

  myservo.setPeriodHertz(50); // Standard 50hz servo
  myservo.attach(servoPin, 500, 2400);  // attaches the servo on pin 18 to the servo object
                                        // using SG90 servo min/max of 500us and 2400us
                                        // for MG995 large servo, use 1000us and 2000us,
                                        // which are the defaults, so this line could be
                                        // "myservo.attach(servoPin);"

  WiFi.mode(WIFI_STA);

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ("AutoConnectAP"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result
  
  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  // wm.autoConnect("EmpathyBot_AP","labs2020"); // password protected AP

  //Reset settings - wipe credentials for Testing
  //wm.resetSettings();

  if(!wm.autoConnect("EmpathyBot_AP","labs2020")) {
      Serial.println("Failed to connect");
      ESP.restart();
      delay(1000);
  } 
  else {
      //if you get here you have connected to the WiFi    
      Serial.println("Connected to WiFi AP");
  }

  //Setup network time protocol (ntp)
  configTime(timezone * 3600, dst, "pool.ntp.org", "time.nist.gov");
  printLocalTime();

  Serial.println("\nWaiting for time sync");
     while (!time(nullptr)) {
       Serial.print(".");
       delay(1000);
     }
     Serial.println("Time is synced");

     delay(500);
     
  Serial.println("Connecting to Slack via Websocket");
  connected = connectToSlack();
}

void loop() {
  
  webSocket.loop();

  int val = digitalRead(PIRPin);
  if (val == HIGH)
  { 
    digitalWrite(LEDPin, HIGH);
    if (pirState == LOW)
    {
      Serial.println("Sensor activado");
      pirState = HIGH;
    }
  } 
  else
  {
    digitalWrite(LEDPin, LOW);
    if (pirState == HIGH)
    {
      Serial.println("Sensor parado");
      pirState = LOW;
    }
  }

  if(!connected){
  connected = connectToSlack();
  }
}