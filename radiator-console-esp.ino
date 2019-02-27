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

LiquidCrystal_I2C lcd(0x27, 16, 2);

const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
const char* host_dev = SECRET_AWS_HOST_DEV;
const char* host_test = SECRET_AWS_HOST_TEST;
const char* host_prod = SECRET_AWS_HOST_PROD;
const char* key_prod = SECRET_AWS_KEY_PROD;
const int httpsPort = 443;

// SHA1 fingerprint of the certificate
const char* fingerprint = "A3 7E 7A F5 3F F0 91 A3 48 45 9E 94 76 AA 18 1A ED B1 FA 96";

#define ALARMS_LED D3
#define PIPE_RUNNING_LED_DEV D5
#define PIPE_FAILED_LED_DEV D4
#define PIPE_RUNNING_LED_TEST D0
#define PIPE_FAILED_LED_TEST D0
#define PIPE_RUNNING_LED_PROD D7
#define PIPE_FAILED_LED_PROD D6

void setup() {
  Serial.begin(115200);
  
  pinMode(ALARMS_LED, OUTPUT);
  pinMode(PIPE_RUNNING_LED_DEV, OUTPUT);
  pinMode(PIPE_FAILED_LED_DEV, OUTPUT);
  pinMode(PIPE_RUNNING_LED_TEST, OUTPUT);
  pinMode(PIPE_FAILED_LED_TEST, OUTPUT);
  pinMode(PIPE_RUNNING_LED_PROD, OUTPUT);
  pinMode(PIPE_FAILED_LED_PROD, OUTPUT);

  Serial.print("START");

  lcd.init();
  lcd.backlight();

  initConsole();
  
  Serial.println();
  Serial.print("Connecting to ");
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
  lcd.print("Wifi connected  ");
  lcd.setCursor(0, 1);
  lcd.print("Call AWS...     ");
}


void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    
    String data_dev = callAWS(host_dev, "D");
    String data_test = callAWS(host_test, "T");
    String data_prod = callAWS(host_prod, "P");
    
    setConsole(data_dev, data_test, data_prod);
        
  } else {
    lcd.print("Wifi Conn lost   ");
  }
}


String callAWS(const char* host, String env) {
  
  lcd.setCursor(15,0);
  lcd.print(env);
  
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  Serial.print("connecting to ");
  Serial.println(host);

  // ###### Open HTTP connection #######
  
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    lcd.print("Connection fail");
    delay(10000);
    return "";
  }

  if (!client.verify(fingerprint, host)) {
    Serial.println("certificate doesn't match");
    lcd.print("Cert not match");
    delay(10000);
    return "";
  }

  String url = "/dev";

  // ###### Send GET Request ######

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: RadiatorConsoleESP\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("request sent");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  
  String line = client.readStringUntil('\n');

  client.stop();
  
  Serial.println("reply was:");
  Serial.println(line);

  lcd.setCursor(15,0);
  lcd.print(" ");

  return line;
}



void setConsole(String data_dev, String data_test, String data_prod) {

// ###### Parse JSON #######
  JsonObject& json_dev = parseJSON(data_dev);
  
  bool pipelines_running_dev = json_dev["pipelines_running"];
  bool pipelines_failed_dev = json_dev["pipelines_failed"];  
  const String pipes_failed_0_dev = json_dev["pipelines_failed_list"][0]; 
  const String pipes_running_0_dev = json_dev["pipelines_running_list"][0]; 

  JsonObject& json_test = parseJSON(data_test);
  
  bool pipelines_running_test = json_test["pipelines_running"];
  bool pipelines_failed_test = json_test["pipelines_failed"];  
  const String pipes_failed_0_test = json_test["pipelines_failed_list"][0]; 
  const String pipes_running_0_test = json_test["pipelines_running_list"][0]; 

  JsonObject& json_prod = parseJSON(data_prod);
  
  bool alarms_raised_prod = json_prod["alarms_raised"];  
  bool pipelines_running_prod = json_prod["pipelines_running"];
  bool pipelines_failed_prod = json_prod["pipelines_failed"];  
  const String alarms_0_prod = json_prod["alarms_list"][0];
  const String pipes_failed_0_prod = json_prod["pipelines_failed_list"][0]; 
  const String pipes_running_0_prod = json_prod["pipelines_running_list"][0]; 


// ###### Set LEDs ######

  digitalWrite(PIPE_RUNNING_LED_DEV, LOW);
  digitalWrite(PIPE_FAILED_LED_DEV, LOW);
  digitalWrite(PIPE_RUNNING_LED_TEST, LOW);
  digitalWrite(PIPE_FAILED_LED_TEST, LOW);
  digitalWrite(ALARMS_LED, LOW);
  digitalWrite(PIPE_RUNNING_LED_PROD, LOW);
  digitalWrite(PIPE_FAILED_LED_PROD, LOW);

  if (pipelines_running_dev == true) { digitalWrite(PIPE_RUNNING_LED_DEV, HIGH); }
  if (pipelines_failed_dev == true) { digitalWrite(PIPE_FAILED_LED_DEV, HIGH); }
  if (pipelines_running_test == true) { digitalWrite(PIPE_RUNNING_LED_TEST, HIGH); }
  if (pipelines_failed_test == true) { digitalWrite(PIPE_FAILED_LED_TEST, HIGH); }
  if (alarms_raised_prod == true) { digitalWrite(ALARMS_LED, HIGH); }
  if (pipelines_running_prod == true) { digitalWrite(PIPE_RUNNING_LED_PROD, HIGH); }
  if (pipelines_failed_prod == true) { digitalWrite(PIPE_FAILED_LED_PROD, HIGH); }  

// ###### Set LCD #######

  for (int i=0; i < 5; i++) {

    if (pipelines_failed_dev == true) {
      lcd.setCursor(0,0);
      lcd.print("Pipe fail DEV:  ");
      lcd.setCursor(0,1);
      lcd.print(pipes_failed_0_dev);

      delay(5000);
    } 
    else if (pipelines_running_dev == true) {  
      lcd.setCursor(0,0);
      lcd.print("Pipe run DEV:   ");
      lcd.setCursor(0,1);
      lcd.print(pipes_running_0_dev);

      delay(5000);
    }  
    else if (pipelines_failed_test == true) {
      lcd.setCursor(0,0);
      lcd.print("Pipe fail TEST: ");
      lcd.setCursor(0,1);
      lcd.print(pipes_failed_0_test);

      delay(5000);
    }  
    else if (pipelines_running_test == true) {  
      lcd.setCursor(0,0);
      lcd.print("Pipe run TEST:  ");
      lcd.setCursor(0,1);
      lcd.print(pipes_running_0_test);

      delay(5000);
    }  
    else if (alarms_raised_prod == true) {  
      lcd.setCursor(0,0);
      lcd.print("Alarms PROD:    ");
      lcd.setCursor(0,1);
      lcd.print(alarms_0_prod);

      delay(5000);
    }
    else if (pipelines_failed_prod == true) {
      lcd.setCursor(0,0);
      lcd.print("Pipe fail PROD: ");
      lcd.setCursor(0,1);
      lcd.print(pipes_failed_0_prod);

      delay(5000);
    }  
    else if (pipelines_running_prod == true) {  
      lcd.setCursor(0,0);
      lcd.print("Pipe run PROD:  ");
      lcd.setCursor(0,1);
      lcd.print(pipes_running_0_prod);

      delay(5000);
    } 
    else {  
      lcd.clear();
      lcd.print("     All OK     ");

      delay(60000);

      break;
    }
  }
}

void initConsole() {

  lcd.setCursor(0, 0);
  lcd.print("  AWS Radiator  ");
  delay(500);
  lcd.setCursor(0, 1);
  lcd.print(" ver 28.02.2019 ");

  digitalWrite(PIPE_RUNNING_LED_DEV, HIGH);
  digitalWrite(PIPE_FAILED_LED_DEV, HIGH);
  delay(250);
  digitalWrite(PIPE_RUNNING_LED_TEST, HIGH);
  digitalWrite(PIPE_FAILED_LED_TEST, HIGH);
  delay(250);
  digitalWrite(PIPE_RUNNING_LED_PROD, HIGH);
  digitalWrite(PIPE_FAILED_LED_PROD, HIGH);
  delay(250);
  digitalWrite(ALARMS_LED, HIGH);

  delay(2000);

  digitalWrite(PIPE_RUNNING_LED_DEV, LOW);
  digitalWrite(PIPE_FAILED_LED_DEV, LOW);
  digitalWrite(PIPE_RUNNING_LED_TEST, LOW);
  digitalWrite(PIPE_FAILED_LED_TEST, LOW);
  digitalWrite(PIPE_RUNNING_LED_PROD, LOW);
  digitalWrite(PIPE_FAILED_LED_PROD, LOW);
  digitalWrite(ALARMS_LED, LOW);

  lcd.clear();
}

JsonObject& parseJSON(String payload) {

  // Length (with one extra character for the null terminator)
  int str_len = payload.length() + 1; 
  
  // Prepare the character array (the buffer) 
  char json[str_len];
  
  // Copy it over 
  payload.toCharArray(json, str_len);

  const size_t capacity = 3*JSON_ARRAY_SIZE(0) + JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(8) + 800;

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
