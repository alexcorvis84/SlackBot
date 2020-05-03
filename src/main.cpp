/*
   DOIT ESP32 DevKitV1 Real-Time Slack Bot
   Copyright (C) 2020, Alejandro Juan Garcia (@AlexCorvis84)
   EmpathyLabs [https://www.empathy.co/]
   Licensed under the MIT License
*/
#define ARDUINOJSON_DECODE_UNICODE 1
#include <Arduino.h>
#include <time.h>
#include <HTTPClient.h>
#include <WebSocketsClient.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <FastLED.h>

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

// Locate your <@Member ID> in your Slack Space:
// You can find it in Profile & account -> ... (vertical 3dots) -> Copy Member ID
// Member ID MakersAturias 
#define MEMBERID "<@UM81F0N0Y>"

// Member ID Empathy.co 
//#define MEMBERID "<@UPV0X6QTE>"


//-------------------------------------------------------------------------------------------------
//FastLed attached config
#define NUM_LEDS 16
#define DATA_PIN 32
CRGB leds[NUM_LEDS];
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
bool connected = false;    // Check Slack connection
int servoPin = 26;         // Possible PWM GPIO pins on the ESP32: 0(used by on-board button),2,4,5(used by on-board LED),12-19,21-23,25-27,32-33 
Servo myservo;             // Create servo object to control a servo
unsigned status;           // Check I2C sensor
Adafruit_BME280 bme;       // I2C SDA GPIO21 - SCL GPIO22 - GND - 3.3V
const int LEDPin = 27;     // Pin LED
const int dopplerPin = 35; // Input Pin for RCWL-0516 sensor (34-39 pins can be used as input only)
const int relay = 12;      // Relay 12V Pin to trigger Sound Alarm
int State = LOW;           // Variable state for Microwave doppler sensor
//-------------------------------------------------------------------------------------------------

void ledanimation(CRGB color) {
  for(int dot = 0; dot < NUM_LEDS; dot++) { 
      leds[dot] = color;
      FastLED.show();
      // clear this led for the next time around the loop
      //leds[dot] = CRGB::Black;
      delay(5);
  }
}

void showStrip() {
   FastLED.show();
}

void setPixel(int Pixel, byte red, byte green, byte blue) {
   // FastLED
   leds[Pixel].r = red;
   leds[Pixel].g = green;
   leds[Pixel].b = blue;
}

void setAll(byte red, byte green, byte blue) {
  for(int i = 0; i < NUM_LEDS; i++ ) {
    setPixel(i, red, green, blue);
  }
  showStrip();
}

void Strobe(byte red, byte green, byte blue, int StrobeCount, int FlashDelay, int EndPause){
  for(int j = 0; j < StrobeCount; j++) {
    setAll(red,green,blue);
    showStrip();
    delay(FlashDelay);
    setAll(0,0,0);
    showStrip();
    delay(FlashDelay);
  }
 
 delay(EndPause);
}

void FadeInOut(byte red, byte green, byte blue){
  float r, g, b;
     
  for(int k = 0; k < 256; k=k+1) {
    r = (k/256.0)*red;
    g = (k/256.0)*green;
    b = (k/256.0)*blue;
    setAll(r,g,b);
    showStrip();
  }
     
  for(int k = 255; k >= 0; k=k-2) {
    r = (k/256.0)*red;
    g = (k/256.0)*green;
    b = (k/256.0)*blue;
    setAll(r,g,b);
    showStrip();
  }
}

void trigger_alarm(){
  digitalWrite(relay, HIGH);
  delay(1500);
  digitalWrite(relay, LOW);
  Serial.println("Switching off Sound Alarm");
}

/*
  Gets the local time and prints it on Serial
*/
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

  if(State == LOW){
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
          // Slower:
          Strobe(0xff, 0xff, 0xff, 10, 100, 1000);
          // Fast:
          //Strobe(0xff, 0xff, 0xff, 30, 50, 1000);
          for(i=0;i<3;i++){
            FadeInOut(0xff, 0x00, 0x00);
          }
          trigger_alarm();
       }
  }

  if (text == "red"){
      ledanimation(CRGB::Red);
      return true;
  }
  if (text == "green"){
      ledanimation(CRGB::Green);
      return true;
  }
  if (text == "blue"){
      ledanimation(CRGB::Blue);
      return true;
  }
  if (text == "off"){
      ledanimation(CRGB::Black);
      return true;
  }
  if (text == "on"){
      Serial.println("Recibido comando 'on' ");
      return true;
  }
  if (text == ":door:"){
      myservo.write(45);
      delay(3000); // wait for the servo to get position
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
  pinMode(relay, OUTPUT);
  pinMode(dopplerPin, INPUT);
  //pixels.begin();
  //pixels.setBrightness(64);
  //pixels.show();
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

  status = bme.begin(0x76);
  if (!status) {
        Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
        Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(),16);
  }

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
  wm.resetSettings();

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

  int val = digitalRead(dopplerPin);
  if (val == HIGH)
  { 
    digitalWrite(LEDPin, HIGH);
    if (State == LOW)
    {
      Serial.println("Sensor activado");
      State = HIGH;
    }
  } 
  else
  {
    digitalWrite(LEDPin, LOW);
    if (State == HIGH)
    {
      Serial.println("Sensor parado");
      State = LOW;
    }
  }

  if(!connected){
  connected = connectToSlack();
  }
}