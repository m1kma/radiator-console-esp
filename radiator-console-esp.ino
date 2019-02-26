/*
 * Mika Mäkelä - 2019
 * 
 * Oldschool AWS Radiator.
 * Show AWS alerts and pipelines by leds and LCD.
 */

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include "arduino_secrets.h"

//const int RS = D2, EN = D3, d4 = D5, d5 = D6, d6 = D7, d7 = D1;   
//LiquidCrystal lcd(RS, EN, d4, d5, d6, d7);

LiquidCrystal_I2C lcd(0x27, 16, 2);

const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
const char* host_dev = SECRET_AWS_HOST_DEV;
const char* host_test = SECRET_AWS_HOST_TEST;
const char* host_prod = SECRET_AWS_HOST_PROD;
const int httpsPort = 443;

// SHA1 fingerprint of the certificate
const char* fingerprint = "7B C9 47 AE F4 1E F6 79 F9 B5 40 29 07 61 7B 66 93 17 5B 68";

#define ALARMS_LED D3
#define PIPE_FAILED_LED D4
#define PIPE_RUNNING_LED D5

void setup() {
  Serial.begin(115200);
  
  pinMode(ALARMS_LED, OUTPUT);
  pinMode(PIPE_FAILED_LED, OUTPUT);
  pinMode(PIPE_RUNNING_LED, OUTPUT);

  Serial.print("START");

  lcd.init();
  lcd.backlight();

  lcd.print("    Radiator    ");
  lcd.setCursor(0, 1);
  lcd.print(" ver 22.02.2019 ");

  delay(3000);
  lcd.clear();
  
  Serial.println();
  Serial.print("connecting to ");
  Serial.println(ssid);

  lcd.setCursor(0, 0);
  lcd.print("Connecting...");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  lcd.setCursor(0, 0);
  lcd.print("Wifi connected     ");
  lcd.setCursor(0, 1);
  lcd.print("Call AWS...      ");
}


void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    callAWS(host_dev);
  } else {
    lcd.print("Wifi Conn lost   ");
  }
}



void callAWS(const char* host) {
  
  lcd.setCursor(15,0);
  lcd.print(":");
  
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  Serial.print("connecting to ");
  Serial.println(host);

  // ###### Open HTTP connection #######
  
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    lcd.print("Connection fail");
    delay(10000);
    return;
  }

  if (!client.verify(fingerprint, host)) {
    Serial.println("certificate doesn't match");
    lcd.print("Cert not match");
    delay(10000);
    return;
  }

  lcd.setCursor(15,0);
  lcd.print(".");

  String url = "/dev";

  // ###### Send GET Request ######

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: RadiatorConsoleESP\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("request sent");

  lcd.setCursor(15,0);
  lcd.print(":");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }

  lcd.setCursor(15,0);
  lcd.print(".");
  
  String line = client.readStringUntil('\n');

  client.stop();
  
  Serial.println("reply was:");
  Serial.println(line);

  lcd.setCursor(15,0);
  lcd.print(" ");

  JsonObject& jsonbody = parseJSON(line);

  bool alarms_raised = jsonbody["alarms_raised"];
  bool pipelines_running = jsonbody["pipelines_running"];
  bool pipelines_failed = jsonbody["pipelines_failed"];

// ###### Set LEDs ######

  digitalWrite(ALARMS_LED, LOW);
  digitalWrite(PIPE_FAILED_LED, LOW);
  digitalWrite(PIPE_RUNNING_LED, LOW);

  if (alarms_raised == true) {
    digitalWrite(ALARMS_LED, HIGH);
  }

  if (pipelines_running == true) {
    digitalWrite(PIPE_RUNNING_LED, HIGH);
  }

  if (pipelines_failed == true) {
    digitalWrite(PIPE_FAILED_LED, HIGH);
  }

// ###### Set LCD #######

  if (alarms_raised == true || pipelines_running == true || pipelines_failed == true) {

    const String alarms_0 = jsonbody["alarms_list"][0];
    const String pipes_failed_0 = jsonbody["pipelines_failed_list"][0]; 
    const String pipes_running_0 = jsonbody["pipelines_running_list"][0]; 
   
    for (int i=0; i < 10; i++) {
    
      if (alarms_raised == true) {  
        lcd.setCursor(0,0);
        lcd.print("Alarms:       ");
        lcd.setCursor(0,1);
        lcd.print(alarms_0);
  
        delay(5000);
      }  
    
      if (pipelines_failed == true) {
        lcd.setCursor(0,0);
        lcd.print("Pipes failed: ");
        lcd.setCursor(0,1);
        lcd.print(pipes_failed_0);
  
        delay(5000);
      }  
        
      if (pipelines_running == true) {  
        lcd.setCursor(0,0);
        lcd.print("Pipes running:  ");
        lcd.setCursor(0,1);
        lcd.print(pipes_running_0);
  
        delay(5000);
      }  
    }
  } else {    
    lcd.clear();
    lcd.print("     All OK     ");

    delay(60000);
  }
}


JsonObject& parseJSON(String payload) {

  // Length (with one extra character for the null terminator)
  int str_len = payload.length() + 1; 
  
  // Prepare the character array (the buffer) 
  char json[str_len];
  
  // Copy it over 
  payload.toCharArray(json, str_len);

  const size_t capacity = 2*JSON_ARRAY_SIZE(0) + JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(7) + 800;

  DynamicJsonBuffer jsonBuffer(capacity);
  
  JsonObject& root = jsonBuffer.parseObject(json);  
  JsonObject& body = root["body"];
  
  bool body_alarms_raised = body["alarms_raised"];
  bool body_pipelines_running = body["pipelines_running"];
  bool body_pipelines_failed = body["pipelines_failed"];
  
  const char* body_alarms_list_0 = body["alarms_list"][0];
  const char* body_pipelines_running_list_0 = body["pipelines_running_list"][0];

  Serial.println(body_alarms_raised);
  Serial.println(body_pipelines_running);
  Serial.println(body_pipelines_failed);
  Serial.println(body_alarms_list_0);
  Serial.println(body_pipelines_running_list_0);

  return body;
}
