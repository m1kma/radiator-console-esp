/*
 * (c) Mika Mäkelä - 2019
 * 
 * The program for the electronic AWS Dashboard Console.
 * Show AWS alerts and code pipeline status by LEDs and LCD.
 * The program is intended to run on the ESP8266 based boards.
 */

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include "arduino_secrets.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);

const char* ssid_home = SECRET_SSID_HOME;
const char* password_home = SECRET_PASS_HOME;
const char* ssid_work = SECRET_SSID_WORK;
const char* password_work = SECRET_PASS_WORK;
const char* ssid_work2 = SECRET_SSID_WORK2;
const char* password_work2 = SECRET_PASS_WORK2;

const char* host_dev = SECRET_AWS_HOST_DEV;
const char* host_test = SECRET_AWS_HOST_TEST;
const char* host_prod = SECRET_AWS_HOST_PROD;
const char* api_key_dev = SECRET_AWS_KEY_DEV;
const char* api_key_test = SECRET_AWS_KEY_TEST;
const char* api_key_prod = SECRET_AWS_KEY_PROD;

const int httpsPort = 443;

// SHA1 fingerprint of the certificate
const char* fingerprint = "13 67 AF FD 46 8F ED 6F B3 28 29 94 6E 02 B1 E7 57 55 01 17";

#define ALARMS_LED D0
#define PIPE_RUNNING_LED_DEV D8
#define PIPE_FAILED_LED_DEV D7
#define PIPE_RUNNING_LED_TEST D6
#define PIPE_FAILED_LED_TEST D5
#define PIPE_RUNNING_LED_PROD D4
#define PIPE_FAILED_LED_PROD D3

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
  connectWifi();
  
  delay(2000);
  lcd.setCursor(0, 0);
  lcd.print("Call AWS...     ");
  lcd.setCursor(0, 1);
  lcd.print("                ");
}


void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    // Call the AWS Lambda endpoints
    String data_dev = callAWS(host_dev, "D", api_key_dev);
    String data_test = callAWS(host_test, "T", api_key_test);
    String data_prod = callAWS(host_prod, "P", api_key_prod);
    
    // Set the console
    setConsole(data_dev, data_test, data_prod);        
  } else {
    lcd.print("Wifi Conn lost   ");
  }
}

/*
 * Connect Wifi
 */
void connectWifi() {
  
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid_work);

  // Try WORK network
  lcd.setCursor(0, 0);
  lcd.print("Connecting...");
  lcd.setCursor(0, 1);
  lcd.print(ssid_work);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid_work, password_work);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    if (WiFi.status() == WL_NO_SSID_AVAIL) {
      WiFi.disconnect();
      break;
    }
  }

  // If no success try WORK2 network 
  if (WiFi.status() != WL_CONNECTED) {
    lcd.clear();
    delay(100);
    lcd.setCursor(0, 0);
    lcd.print("Connecting...");
    lcd.setCursor(0, 1);
    lcd.print(ssid_work2);

    WiFi.begin(ssid_work2, password_work2);
    
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");

      if (WiFi.status() == WL_NO_SSID_AVAIL) {
        WiFi.disconnect();
        break;
      }
    }
  }

  // If no success try HOME network 
  if (WiFi.status() != WL_CONNECTED) {
    lcd.clear();
    delay(100);
    lcd.setCursor(0, 0);
    lcd.print("Connecting...");
    lcd.setCursor(0, 1);
    lcd.print(ssid_home);

    WiFi.begin(ssid_home, password_home);
    
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");

      if (WiFi.status() == WL_NO_SSID_AVAIL) {
        WiFi.disconnect();
        break;
      }
    }
  }

  
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  lcd.setCursor(0, 0);
  lcd.print("Wifi connected  ");
}


/*
 * Call the AWS endpoint
 */
String callAWS(const char* host, String env, String api_key) {
  
  // Print status character to the LCD
  lcd.setCursor(15,0);
  lcd.print(env);
  
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

  /*if (!client.verify(fingerprint, host)) {
    lcd.setCursor(0, 0);
    Serial.println("certificate doesn't match");
    lcd.print("Cert not match");
    delay(10000);
    return "";
  }*/

  String url = "/dev/status";

  // ###### Send GET Request ######

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ArduinoRadiatorConsole\r\n" +
               "x-api-key: " + api_key + "\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("request sent");

  while (client.connected()) {
    String line = client.readStringUntil('\n');

    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }

  //client.setTimeout(1000);
  //String line = client.readStringUntil('\n');

  String line = client.readStringUntil('}');
  line = line + "}";

  client.stop();
  
  Serial.println("reply was:");
  Serial.println(line);

  lcd.setCursor(15,0);
  lcd.print(" ");

  return line;
}


/*
 * Parse the JSON response and set console status
 */
void setConsole(String data_dev, String data_test, String data_prod) {

// ###### Parse JSON #######
  DynamicJsonDocument json_dev = parseJSON(data_dev);
  
  bool pipelines_running_dev = json_dev["pipelines_running"];
  bool pipelines_failed_dev = json_dev["pipelines_failed"];  
  const String pipes_failed_0_dev = json_dev["pipelines_failed_list"][0]; 
  const String pipes_running_0_dev = json_dev["pipelines_running_list"][0]; 

  DynamicJsonDocument json_test = parseJSON(data_test);
  
  bool pipelines_running_test = json_test["pipelines_running"];
  bool pipelines_failed_test = json_test["pipelines_failed"];  
  const String pipes_failed_0_test = json_test["pipelines_failed_list"][0]; 
  const String pipes_running_0_test = json_test["pipelines_running_list"][0]; 

  DynamicJsonDocument json_prod = parseJSON(data_prod);
  
  bool alarms_raised_prod = json_prod["alarms_raised"];  
  bool alarms_raised_history_prod = json_prod["alarms_raised_history"];  
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

    bool all_ok = true;

    // Blink the alarm led if history alarms exists
    if (alarms_raised_history_prod == true && alarms_raised_prod == false) {      
      digitalWrite(ALARMS_LED, HIGH);
      delay(50);
      digitalWrite(ALARMS_LED, LOW);
    } 
  
    if (pipelines_failed_dev == true) {
      lcd.setCursor(0,0);
      lcd.print("DEV Pipe fail:  ");
      lcd.setCursor(0,1);
      lcd.print(pipes_failed_0_dev);

      all_ok = false;
      delay(5000);
    } 
    
    if (pipelines_running_dev == true) {  
      lcd.setCursor(0,0);
      lcd.print("DEV Pipe run:   ");
      lcd.setCursor(0,1);
      lcd.print(pipes_running_0_dev);

      all_ok = false;
      delay(5000);
    }  
    
    if (pipelines_failed_test == true) {
      lcd.setCursor(0,0);
      lcd.print("TEST Pipe fail: ");
      lcd.setCursor(0,1);
      lcd.print(pipes_failed_0_test);

      all_ok = false;
      delay(5000);
    }  
    
    if (pipelines_running_test == true) {  
      lcd.setCursor(0,0);
      lcd.print("TEST Pipe run:  ");
      lcd.setCursor(0,1);
      lcd.print(pipes_running_0_test);

      all_ok = false;
      delay(5000);
    }  
    
    if (alarms_raised_prod == true) {  
      lcd.setCursor(0,0);
      lcd.print("PROD Alarms:    ");
      lcd.setCursor(0,1);
      lcd.print(alarms_0_prod);

      all_ok = false;
      delay(5000);
    }

    if (pipelines_failed_prod == true) {
      lcd.setCursor(0,0);
      lcd.print("PROD Pipe fail: ");
      lcd.setCursor(0,1);
      lcd.print(pipes_failed_0_prod);
      
      all_ok = false;
      delay(5000);
    }  
    
    if (pipelines_running_prod == true) {  
      lcd.setCursor(0,0);
      lcd.print("PROD Pipe run:  ");
      lcd.setCursor(0,1);
      lcd.print(pipes_running_0_prod);
      
      all_ok = false;
      delay(5000);
    } 
    
    if (all_ok == true) {  
      lcd.clear();
      lcd.print("     All OK     ");

      delay(60000);

      break;
    }
  }
}

/*
 * Initialize the console leds and LCD
 */
void initConsole() {

  lcd.setCursor(0, 0);
  lcd.print("  AWS Radiator  ");
  lcd.setCursor(0, 1);
  lcd.print(" ver 07.04.2019 ");

  digitalWrite(PIPE_RUNNING_LED_DEV, HIGH);
  digitalWrite(PIPE_FAILED_LED_DEV, HIGH);
  digitalWrite(PIPE_RUNNING_LED_TEST, HIGH);
  digitalWrite(PIPE_FAILED_LED_TEST, HIGH);
  digitalWrite(PIPE_RUNNING_LED_PROD, HIGH);
  digitalWrite(PIPE_FAILED_LED_PROD, HIGH);
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

/*
 * JSON parser
 */
DynamicJsonDocument parseJSON(String payload) {

  const size_t capacity = 2*JSON_ARRAY_SIZE(0) + JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(7) + 800;

  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, payload);
  
  bool body_alarms_raised = doc["alarms_raised"];
  bool body_pipelines_running = doc["pipelines_running"];
  bool body_pipelines_failed = doc["pipelines_failed"];
  bool body_pipelines_raised_history = doc["pipelines_raised_history"];
  
  const String body_alarms_list_0 = doc["alarms_list"][0];
  const String body_pipelines_running_list_0 = doc["pipelines_running_list"][0];
  const String body_pipelines_failed_list_0 = doc["pipelines_failed_list"][0];

  Serial.println(body_alarms_raised);
  Serial.println(body_pipelines_raised_history);
  Serial.println(body_pipelines_running);
  Serial.println(body_pipelines_failed);
  Serial.println(body_alarms_list_0);
  Serial.println(body_pipelines_running_list_0);
  Serial.println(body_pipelines_failed_list_0);


  return doc;
}
