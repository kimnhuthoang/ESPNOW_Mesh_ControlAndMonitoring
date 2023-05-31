#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <espnow.h>
#include <WiFiManager.h>
#include <Arduino_JSON.h>
#include <Ticker.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "secrets.h"

#define BOARD_ID 1

#define TRIGGER_PIN 0
#define BUTTON1 12  
#define BUTTON2 13
#define RELAY1 4
#define RELAY2 5

#define SDA 1
#define SCL 3
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
U8G2_FOR_ADAFRUIT_GFX u8g2_GFX;

WiFiClientSecure httpsClient;
 
BearSSL::X509List cert(rootCA);
BearSSL::X509List client_crt(client_certificate);
BearSSL::PrivateKey key(privatekey);
 
PubSubClient mqttClient(httpsClient);
 
time_t now;

WiFiEventHandler disconnectedEventHandler;

Ticker TimerLed;

//const char* Wifi_SSID = "ASUS_X00TD";
//const char* Wifi_PASSWORD = "NhutHoang0910";
// REPLACE WITH THE MAC Address of your receiver 
uint8_t ParentNode_Addr_1[] = {0xC8, 0xC9, 0xA3, 0x58, 0x0B, 0xED};
uint8_t ParentNode_Addr_2[] = {0xBC, 0xFF, 0x4D, 0xF9, 0x53, 0x21};


uint32_t Node2_TimeOut = 0;
uint32_t Node3_TimeOut = 0;
uint32_t current_time = 0;
uint32_t time_delayConnect = -2000;
uint32_t time_ReadSensor = 0;
uint32_t led_time = 0;
uint32_t time_SwitchDisplay = 0;
uint8_t node_display = 1;
uint16_t status_time = 200;

bool SendNode2_OK = 1;
bool SendNode3_OK = 1;
bool SendNode4_OK = 1;
uint32_t Node2_Wait_SendAgain = 0;
uint32_t Node3_Wait_SendAgain = 0;
uint32_t Node4_Wait_SendAgain = 0;
uint8_t Sending_Type = 1;

bool ESPNOW_Node2_Rcv_RL = 0;
bool ESPNOW_Node3_Rcv_RL = 0;
bool ESPNOW_Node4_Rcv_RL = 0;

bool ESPNOW_Node2_Rcv_SS = 0;
bool ESPNOW_Node3_Rcv_SS = 0;
bool ESPNOW_Node4_Rcv_SS = 0;

bool MQTT_Node1_rcv = 0;
bool MQTT_Node2_rcv = 0;
bool MQTT_Node3_rcv = 0;
bool MQTT_Node4_rcv = 0;

//Structure example to send data
//Must match the receiver structure
typedef struct struct_message_DataRelay{
    uint8_t type = '#';
    uint8_t fromID;
    uint8_t toID;
    bool RL1;
    bool RL2;
} struct_message_DataRelay;

typedef struct struct_message_DataSensor{
    uint8_t type = '%';
    uint8_t fromID;
    uint8_t toID;
    uint8_t temp;
    uint8_t humi;
} struct_message_DataSensor;

typedef struct struct_message_Status1{
    uint8_t type = '!';
    uint8_t fromID;
    uint8_t node_id1;
    uint8_t node_id2;
} struct_message_Status1;

typedef struct struct_message_Status2{
    uint8_t node_1 = 0;
    uint8_t node_2 = 0;
    uint8_t node_3 = 0;
    uint8_t node_4 = 0;
} struct_message_Status2;

uint8_t stt_4from2 = 0;
uint8_t stt_4from3 = 0;
struct_message_Status2  StatusToSend;

struct_message_DataRelay Node1_Data_Relay;
struct_message_DataRelay Node2_Data_Relay;
struct_message_DataRelay Node3_Data_Relay;
struct_message_DataRelay Node4_Data_Relay;

struct_message_DataSensor Node1_DataReading_Sensor;
struct_message_DataSensor Node2_DataReading_Sensor;
struct_message_DataSensor Node3_DataReading_Sensor;
struct_message_DataSensor Node4_DataReading_Sensor;

//void ESPNOW_SentCallback(uint8_t *mac_addr, uint8_t sendStatus);
void IRAM_ATTR ESPNOW_ReceivedCallback(uint8_t * mac, uint8_t *incomingData, uint8_t len);
//void IRAM_ATTR MQTT_Callback(char *topic, byte *payload, unsigned int length);
void WiFiEvent(WiFiEvent_t event);
bool ScanWiFiAvailable(String ssid);
void connection_wifi();
void Check_Wifi_Connection();

//--------------------------------------------------------------------------------------------------//

void setup() {
  // Init Serial Monitor
  delay(1000);
  TimerLed.attach_ms(status_time, led_status);
  
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(RELAY1, OUTPUT); digitalWrite(RELAY1,HIGH);
  pinMode(RELAY2, OUTPUT); digitalWrite(RELAY2,HIGH);
  pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN,HIGH);
  
  disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event)
  {
    //Serial.println("Station disconnected");
    WiFi.disconnect();
  });

  Wire.begin(SDA,SCL);
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  display.setTextSize(1);
  display.cp437(true);
  u8g2_GFX.begin(display);                  
  u8g2_GFX.setFontMode(0);                 
  u8g2_GFX.setFontDirection(0); 
  u8g2_GFX.setFont(u8g2_font_7x13_mr); 

  display.clearDisplay(); 
  u8g2_GFX.drawStr(2, 9, "IOT SYSTEM ESP8266");
  u8g2_GFX.drawStr(16, 25, "KIM NHUT HOANG");
  u8g2_GFX.drawStr(1, 41, "2002190238-10DHDT1");
  u8g2_GFX.drawStr(1, 57, "CONNECTING TO WIFI");
  display.display();
  
  delay(200);
  connection_wifi();
  WiFi.setAutoReconnect(false);
  
  // Init ESP-NOW
  if(esp_now_init() != 0){
    //Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(ESPNOW_SentCallback);
  esp_now_add_peer(ParentNode_Addr_1, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);
  esp_now_add_peer(ParentNode_Addr_2, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);
  delay(2000);
  
  mqttClient.setServer(MQTT_HOST, 8883);
  mqttClient.setCallback(MQTT_Callback);
  if(WiFi.status() == WL_CONNECTED) {
    u8g2_GFX.drawStr(0, 57, "CONNECTING TO MQTT"); display.display();
    if(!mqttClient.connected()) Reconnect_MQTT();
  }
  esp_now_register_recv_cb(ESPNOW_ReceivedCallback);
  delay(1000);
}

//--------------------------------------------------------------------------------------------------//
 
void loop() {

  if(WiFi.status() == WL_CONNECTED) {
    if(!mqttClient.connected()) {
      display.clearDisplay();
      u8g2_GFX.setFont(u8g2_font_7x13_mr);
      u8g2_GFX.drawStr(0, 64, "CONNECTING TO MQTT"); display.display();
      Reconnect_MQTT();
    } else {
      mqttClient.loop();
    }
  }

  Check_Wifi_Connection();
  //led_status();

  Check_Button();
  ESPNOW_Rcv_Handle();
  MQTT_Rcv_Handle();
  
  if(long_press()) {
    digitalWrite(LED_BUILTIN,LOW);
    WiFiManager wm;    
    wm.setConfigPortalTimeout(120);
    display.clearDisplay();
    u8g2_GFX.setFont(u8g2_font_7x13_mr);
    u8g2_GFX.drawStr(2, 9, "IOT SYSTEM ESP8266");
    u8g2_GFX.drawStr(12, 25, "CONFIG NEW WIFI");
    u8g2_GFX.drawStr(8, 41, "SSID:ESP8266_IOT");
    u8g2_GFX.drawStr(12, 57, "IP: 192.168.4.1");
    display.display();
    if(!wm.startConfigPortal("ESP8266_IOT")){
      delay(3000);
      ESP.restart();
    }
  }

//  if(millis() - time_ReadSensor >= 10000) {
//    JSONVar JSON_DataSensor;
//    JSON_DataSensor["node1"]["temp"] = (int)Node1_DataReading_Sensor.temp; 
//    JSON_DataSensor["node1"]["humi"] = (int)Node1_DataReading_Sensor.humi;
//    String jsonString = JSON.stringify(JSON_DataSensor);
//    //if(mqttClient.connected()) mqttClient.publish("esp8266hufi/node1/sensor",jsonString.c_str());
//  }

  SendToNode2_Again();
  SendToNode3_Again();
  SendToNode4_Again();

  if(millis() - time_SwitchDisplay >= 4000) {
    switch(node_display) {
      case 1:
        showDisplay_detail(node_display, Node1_DataReading_Sensor.temp, Node1_DataReading_Sensor.humi, Node1_Data_Relay.RL1, Node1_Data_Relay.RL2);
        break;
      case 2:
        showDisplay_detail(node_display, Node2_DataReading_Sensor.temp, Node2_DataReading_Sensor.humi, Node2_Data_Relay.RL1, Node2_Data_Relay.RL2);
        break;
      case 3:
        showDisplay_detail(node_display, Node3_DataReading_Sensor.temp, Node3_DataReading_Sensor.humi, Node3_Data_Relay.RL1, Node3_Data_Relay.RL2);
        break;
      case 4:
        showDisplay_detail(node_display, Node4_DataReading_Sensor.temp, Node4_DataReading_Sensor.humi, Node4_Data_Relay.RL1, Node4_Data_Relay.RL2);
        break;
    }
    node_display += 1;
    if(node_display == 5) node_display = 1;
    time_SwitchDisplay = millis();
  }
}

//----------------------------------------------ESP-NOW-----------------------------------------------------------//

// Callback when data is sent
void ESPNOW_SentCallback(uint8_t *mac_addr, uint8_t sendStatus) {
  //Serial.print("Last Packet Send Status: ");
  if (sendStatus == 0){
//    Serial.println("Delivery success");
//    Serial.println("----------------------------------------------------");
    if(memcmp(mac_addr,ParentNode_Addr_1,sizeof(mac_addr)) == 0) {
      if(Sending_Type == 1) SendNode2_OK = 1;
      if(Sending_Type == 3) SendNode4_OK = 1;
    }
    if(memcmp(mac_addr,ParentNode_Addr_2,sizeof(mac_addr)) == 0) {
      if(Sending_Type == 2) SendNode3_OK = 1;
      if(Sending_Type == 3) SendNode4_OK = 1;
    }
  }
  else{
//    Serial.println("Delivery fail");
//    Serial.println("----------------------------------------------------");
    if(memcmp(mac_addr,ParentNode_Addr_1,sizeof(mac_addr)) == 0) {
      if(Sending_Type == 1) {SendNode2_OK = 0; Node2_Wait_SendAgain = millis();}
      if(Sending_Type == 3) {SendNode4_OK = 0; Node4_Wait_SendAgain = millis();}
    }
    if(memcmp(mac_addr,ParentNode_Addr_2,sizeof(mac_addr)) == 0) {
      if(Sending_Type == 2) {SendNode3_OK = 0; Node3_Wait_SendAgain = millis();}
      if(Sending_Type == 3) {SendNode4_OK = 0; Node4_Wait_SendAgain = millis();}
    }
  }
}

// Callback when data is received
void IRAM_ATTR ESPNOW_ReceivedCallback(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  if((char)incomingData[0] == '#')
  {
    struct_message_DataRelay DataReading_Relay;
    memcpy(&DataReading_Relay, incomingData, sizeof(DataReading_Relay));
    if(DataReading_Relay.fromID == 2 && DataReading_Relay.toID == 1) { 
      Node2_Data_Relay.RL1 = DataReading_Relay.RL1; Node2_Data_Relay.RL2 = DataReading_Relay.RL2;
      ESPNOW_Node2_Rcv_RL = 1;
    }
    else if(DataReading_Relay.fromID == 3 && DataReading_Relay.toID == 1){ 
      Node3_Data_Relay.RL1 = DataReading_Relay.RL1; Node3_Data_Relay.RL2 = DataReading_Relay.RL2;
      ESPNOW_Node3_Rcv_RL = 1;
    }
    else {
      Node4_Data_Relay.RL1 = DataReading_Relay.RL1; Node4_Data_Relay.RL2 = DataReading_Relay.RL2;
      ESPNOW_Node4_Rcv_RL = 1;
    }
  }
  if((char)incomingData[0] == '%')
  {
    struct_message_DataSensor DataReading_Sensor;
    memcpy(&DataReading_Sensor, incomingData, sizeof(DataReading_Sensor));
    if(DataReading_Sensor.fromID == 2 && DataReading_Sensor.toID == 1) { 
      Node2_DataReading_Sensor.temp = DataReading_Sensor.temp; Node2_DataReading_Sensor.humi = DataReading_Sensor.humi;
      ESPNOW_Node2_Rcv_SS = 1;
    }
    else if(DataReading_Sensor.fromID == 3 && DataReading_Sensor.toID == 1) { 
      Node3_DataReading_Sensor.temp = DataReading_Sensor.temp; Node3_DataReading_Sensor.humi = DataReading_Sensor.humi;
      ESPNOW_Node3_Rcv_SS = 1; 
    }
    else {
      Node4_DataReading_Sensor.temp = DataReading_Sensor.temp; Node4_DataReading_Sensor.humi = DataReading_Sensor.humi;
      ESPNOW_Node4_Rcv_SS = 1;
    }
  }
  if((char)incomingData[0] == '!')
  {
    struct_message_Status1 DataReading_Status;
    memcpy(&DataReading_Status, incomingData, sizeof(DataReading_Status));
    if(DataReading_Status.fromID == 2) { 
      StatusToSend.node_2 = DataReading_Status.node_id1;
      stt_4from2 = DataReading_Status.node_id2;
      Node2_TimeOut = 0;
    }
    if(DataReading_Status.fromID == 3) {
      StatusToSend.node_3 = DataReading_Status.node_id1;
      stt_4from3 = DataReading_Status.node_id2;
      Node3_TimeOut = 0;
    }
  }
}

//----------------------------------------------MQTT--------------------------------------------------------------//

void MQTT_Callback(char *topic, byte *payload, unsigned int length)
{
  String MessageMQTT,Topic_name;
  Topic_name = String(topic);
  //Serial.print("Message arrived ["); Serial.print(Topic_name); Serial.print("] ");
  for (int i = 0; i < length; i++) 
  {
    MessageMQTT += (char)payload[i];
  }
  //Serial.println(MessageMQTT);
  JSONVar JSON_data = JSON.parse(MessageMQTT);
  //-------------------------------------------------------------------------------------------------------------//
  if(Topic_name == "node1/update/delta") {
    if(JSON_data["state"]["RL1"] != null) {
      if(String(JSON_data["state"]["RL1"]) == "1") {digitalWrite(RELAY1,LOW); Node1_Data_Relay.RL1 = 1;} 
      else {digitalWrite(RELAY1,HIGH); Node1_Data_Relay.RL1 = 0;}
    }
    if(JSON_data["state"]["RL2"] != null) {
      if(String(JSON_data["state"]["RL2"]) == "1") {digitalWrite(RELAY2,LOW); Node1_Data_Relay.RL2 = 1;} 
      else {digitalWrite(RELAY2,HIGH); Node1_Data_Relay.RL2 = 0;}
    }
    MQTT_Node1_rcv = 1;
  }
  if(Topic_name == "node2/update/delta") {
    Node2_Data_Relay.fromID = BOARD_ID; Node2_Data_Relay.toID = 2;
    if(JSON_data["state"]["RL1"] != null) {
      if(String(JSON_data["state"]["RL1"]) == "1") Node2_Data_Relay.RL1 = 1;
      else Node2_Data_Relay.RL1 = 0;
    }
    if(JSON_data["state"]["RL2"] != null) {
      if(String(JSON_data["state"]["RL2"]) == "1") Node2_Data_Relay.RL2 = 1;
      else Node2_Data_Relay.RL2 = 0;
    }
    MQTT_Node2_rcv = 1;
  }
  if(Topic_name == "node3/update/delta") {
    Node3_Data_Relay.fromID = BOARD_ID; Node3_Data_Relay.toID = 3;
    if(JSON_data["state"]["RL1"] != null) {
      if(String(JSON_data["state"]["RL1"]) == "1") Node3_Data_Relay.RL1 = 1;
      else Node3_Data_Relay.RL1 = 0;
    }
    if(JSON_data["state"]["RL2"] != null) {
      if(String(JSON_data["state"]["RL2"]) == "1") Node3_Data_Relay.RL2 = 1;
      else Node3_Data_Relay.RL2 = 0;
    }
    MQTT_Node3_rcv = 1;
  }
  if(Topic_name == "node4/update/delta") {
    Node4_Data_Relay.fromID = BOARD_ID; Node4_Data_Relay.toID = 4;
    if(JSON_data["state"]["RL1"] != null) {
      if(String(JSON_data["state"]["RL1"]) == "1") Node4_Data_Relay.RL1 = 1;
      else Node4_Data_Relay.RL1 = 0;
    }
    if(JSON_data["state"]["RL2"] != null) {
      if(String(JSON_data["state"]["RL2"]) == "1") Node4_Data_Relay.RL2 = 1;
      else Node4_Data_Relay.RL2 = 0;
    }
    MQTT_Node4_rcv = 1;
  }
  //-------------------------------------------------------------------------------------------------------------//
  if(Topic_name == "node1/get/accepted") {
    if(String(JSON_data["state"]["desired"]["RL1"]) == "1") {digitalWrite(RELAY1,LOW); Node1_Data_Relay.RL1 = 1;}
    else {digitalWrite(RELAY1,HIGH); Node1_Data_Relay.RL1 = 0;}
    if(String(JSON_data["state"]["desired"]["RL2"]) == "1") {digitalWrite(RELAY2,LOW); Node1_Data_Relay.RL2 = 1;}
    else {digitalWrite(RELAY2,HIGH); Node1_Data_Relay.RL2 = 0;}
    MQTT_Node1_rcv = 1;
  }
  
  if(Topic_name == "node2/get/accepted") {
    if(String(JSON_data["state"]["desired"]["RL1"]) == "1") Node2_Data_Relay.RL1 = 1;
    else Node2_Data_Relay.RL1 = 0;
    if(String(JSON_data["state"]["desired"]["RL2"]) == "1") Node2_Data_Relay.RL2 = 1;
    else Node2_Data_Relay.RL2 = 0;
    
    MQTT_Node2_rcv = 1;
    //Sending_Type = 1; esp_now_send(ParentNode_Addr_1, (uint8_t *) &Node2_Data_Relay, sizeof(Node2_Data_Relay));
  }
  
  if(Topic_name == "node3/get/accepted") {
    if(String(JSON_data["state"]["desired"]["RL1"]) == "1") Node3_Data_Relay.RL1 = 1;
    else Node3_Data_Relay.RL1 = 0;
    if(String(JSON_data["state"]["desired"]["RL2"]) == "1") Node3_Data_Relay.RL2 = 1;
    else Node3_Data_Relay.RL2 = 0;

    MQTT_Node3_rcv = 1;
    //Sending_Type = 2; esp_now_send(ParentNode_Addr_2, (uint8_t *) &Node3_Data_Relay, sizeof(Node3_Data_Relay));
  }
  
  if(Topic_name == "node4/get/accepted") {
    if(String(JSON_data["state"]["desired"]["RL1"]) == "1") Node4_Data_Relay.RL1 = 1;
    else Node4_Data_Relay.RL1 = 0;
    if(String(JSON_data["state"]["desired"]["RL2"]) == "1") Node4_Data_Relay.RL2 = 1;
    else Node4_Data_Relay.RL2 = 0;

    MQTT_Node4_rcv = 1;
    //Sending_Type = 3; 
    //esp_now_send(ParentNode_Addr_1, (uint8_t *) &Node4_Data_Relay, sizeof(Node4_Data_Relay));
    //esp_now_send(ParentNode_Addr_2, (uint8_t *) &Node4_Data_Relay, sizeof(Node4_Data_Relay));
  }
  
}

//--------------------------------------------------------------------------------------------------//
void ESPNOW_Rcv_Handle() {
  if(ESPNOW_Node2_Rcv_RL == 1) {
    //Serial.println("Send Data Relay Node 2 to MQTT");
    ESPNOW_Node2_Rcv_RL = 0;
    JSONVar JSON_DataRelay;
    //JSON_DataRelay["state"]["desired"] = null;
    JSON_DataRelay["state"]["desired"]["RL1"]  = String(Node2_Data_Relay.RL1);
    JSON_DataRelay["state"]["desired"]["RL2"]  = String(Node2_Data_Relay.RL2);
    JSON_DataRelay["state"]["reported"]["RL1"] = String(Node2_Data_Relay.RL1);
    JSON_DataRelay["state"]["reported"]["RL2"] = String(Node2_Data_Relay.RL2);
    String jsonString = JSON.stringify(JSON_DataRelay);
    if(mqttClient.connected()) mqttClient.publish("$aws/things/esp8266hufi/shadow/name/node2/update",jsonString.c_str());
    showDisplay_detail(2, Node2_DataReading_Sensor.temp, Node2_DataReading_Sensor.humi, Node2_Data_Relay.RL1, Node2_Data_Relay.RL2);
    time_SwitchDisplay = millis(); node_display = 2;
  }
  if(ESPNOW_Node3_Rcv_RL == 1) {
    //Serial.println("Send Data Relay Node 3 to MQTT");
    ESPNOW_Node3_Rcv_RL = 0;
    JSONVar JSON_DataRelay;
    //JSON_DataRelay["state"]["desired"] = null;
    JSON_DataRelay["state"]["desired"]["RL1"]  = String(Node3_Data_Relay.RL1);
    JSON_DataRelay["state"]["desired"]["RL2"]  = String(Node3_Data_Relay.RL2);
    JSON_DataRelay["state"]["reported"]["RL1"] = String(Node3_Data_Relay.RL1);
    JSON_DataRelay["state"]["reported"]["RL2"] = String(Node3_Data_Relay.RL2);
    String jsonString = JSON.stringify(JSON_DataRelay);
    if(mqttClient.connected()) mqttClient.publish("$aws/things/esp8266hufi/shadow/name/node3/update",jsonString.c_str());
    showDisplay_detail(3, Node3_DataReading_Sensor.temp, Node3_DataReading_Sensor.humi, Node3_Data_Relay.RL1, Node3_Data_Relay.RL2);
    time_SwitchDisplay = millis(); node_display = 3;
  }
  if(ESPNOW_Node4_Rcv_RL == 1) {
    //Serial.println("Send Data Relay Node 4 to MQTT");
    ESPNOW_Node4_Rcv_RL = 0;
    JSONVar JSON_DataRelay;
    //JSON_DataRelay["state"]["desired"] = null;
    JSON_DataRelay["state"]["desired"]["RL1"]  = String(Node4_Data_Relay.RL1);
    JSON_DataRelay["state"]["desired"]["RL2"]  = String(Node4_Data_Relay.RL2);
    JSON_DataRelay["state"]["reported"]["RL1"] = String(Node4_Data_Relay.RL1);
    JSON_DataRelay["state"]["reported"]["RL2"] = String(Node4_Data_Relay.RL2);
    String jsonString = JSON.stringify(JSON_DataRelay);
    if(mqttClient.connected()) mqttClient.publish("$aws/things/esp8266hufi/shadow/name/node4/update",jsonString.c_str());
    showDisplay_detail(4, Node4_DataReading_Sensor.temp, Node4_DataReading_Sensor.humi, Node4_Data_Relay.RL1, Node4_Data_Relay.RL2);
    time_SwitchDisplay = millis(); node_display = 4;
  }
  if(ESPNOW_Node2_Rcv_SS == 1) {
    JSONVar JSON_DataSensor;
    JSON_DataSensor["temp"] = (int)Node2_DataReading_Sensor.temp; 
    JSON_DataSensor["humi"] = (int)Node2_DataReading_Sensor.humi;
    String jsonString = JSON.stringify(JSON_DataSensor);
    if(mqttClient.connected()) mqttClient.publish("esp8266hufi/node2/sensor",jsonString.c_str());
    ESPNOW_Node2_Rcv_SS = 0;
  }
  if(ESPNOW_Node3_Rcv_SS == 1) {
    JSONVar JSON_DataSensor;
    JSON_DataSensor["temp"] = (int)Node3_DataReading_Sensor.temp; 
    JSON_DataSensor["humi"] = (int)Node3_DataReading_Sensor.humi;
    String jsonString = JSON.stringify(JSON_DataSensor);
    if(mqttClient.connected()) mqttClient.publish("esp8266hufi/node3/sensor",jsonString.c_str());
    ESPNOW_Node3_Rcv_SS = 0;
  }
  if(ESPNOW_Node4_Rcv_SS == 1) {
    JSONVar JSON_DataSensor;
    JSON_DataSensor["temp"] = (int)Node4_DataReading_Sensor.temp; 
    JSON_DataSensor["humi"] = (int)Node4_DataReading_Sensor.humi;
    String jsonString = JSON.stringify(JSON_DataSensor);
    if(mqttClient.connected()) mqttClient.publish("esp8266hufi/node4/sensor",jsonString.c_str());
    ESPNOW_Node4_Rcv_SS = 0;
  }
}
//--------------------------------------------------------------------------------------------------//
void MQTT_Rcv_Handle() {
  if(MQTT_Node1_rcv == 1) {
    MQTT_Node1_rcv = 0;
    JSONVar JSON_DataRelay;
    JSON_DataRelay["state"]["reported"]["RL1"] = String(Node1_Data_Relay.RL1);
    JSON_DataRelay["state"]["reported"]["RL2"] = String(Node1_Data_Relay.RL2);
    String jsonString = JSON.stringify(JSON_DataRelay);
    if(mqttClient.connected()) mqttClient.publish("$aws/things/esp8266hufi/shadow/name/node1/update",jsonString.c_str());
    showDisplay_detail(1, Node1_DataReading_Sensor.temp, Node1_DataReading_Sensor.humi, Node1_Data_Relay.RL1, Node1_Data_Relay.RL2);
    time_SwitchDisplay = millis(); node_display = 1;
  }
  if(MQTT_Node2_rcv == 1) {
    MQTT_Node2_rcv = 0;
    Sending_Type = 1;
    esp_now_send(ParentNode_Addr_1, (uint8_t *) &Node2_Data_Relay, sizeof(Node2_Data_Relay));
  }
  if(MQTT_Node3_rcv == 1) {
    MQTT_Node3_rcv = 0;
    Sending_Type = 2;
    esp_now_send(ParentNode_Addr_2, (uint8_t *) &Node3_Data_Relay, sizeof(Node3_Data_Relay));
  }
  if(MQTT_Node4_rcv == 1) {
    MQTT_Node4_rcv = 0;
    Sending_Type = 3;
    if(StatusToSend.node_2 == 1) esp_now_send(ParentNode_Addr_1, (uint8_t *) &Node4_Data_Relay, sizeof(Node4_Data_Relay));
    else esp_now_send(ParentNode_Addr_2, (uint8_t *) &Node4_Data_Relay, sizeof(Node4_Data_Relay));
  }
}
//--------------------------------------------------------------------------------------------------//

void NTPConnect(void)
{
  uint16_t i = 2000;
  //Serial.print("Setting time using SNTP");
  configTime(TIME_ZONE * 3600, 0, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while(i--) {
    now = time(nullptr);
    delay(1);
  }
  delay(1000);
}

void Reconnect_MQTT() 
{
  if(millis() - time_delayConnect >= 5000) {
    NTPConnect();
    httpsClient.setTrustAnchors(&cert);
    httpsClient.setClientRSACert(&client_crt, &key);
    //Serial.print("Attempting MQTT connection...");
    if(mqttClient.connect(THINGNAME)) {
      time_delayConnect = 0;
      //Serial.println("connected");
      mqttClient.subscribe("node1/update/delta"); mqttClient.subscribe("node1/get/accepted");
      mqttClient.subscribe("node2/update/delta"); mqttClient.subscribe("node2/get/accepted");
      mqttClient.subscribe("node3/update/delta"); mqttClient.subscribe("node3/get/accepted");
      mqttClient.subscribe("node4/update/delta"); mqttClient.subscribe("node4/get/accepted");
      delay(500);
      mqttClient.publish("$aws/things/esp8266hufi/shadow/name/node1/get",""); delay(500);
      mqttClient.publish("$aws/things/esp8266hufi/shadow/name/node2/get",""); delay(500);
      mqttClient.publish("$aws/things/esp8266hufi/shadow/name/node3/get",""); delay(500);
      mqttClient.publish("$aws/things/esp8266hufi/shadow/name/node4/get",""); delay(500);
      u8g2_GFX.drawStr(0, 57, "CONNECTED TO MQTT "); display.display();
    } else {
      time_delayConnect = millis();
      //Serial.print("failed, rc=");  // in ra màn hình trạng thái của client khi không kết nối được với MQTT broker
      //Serial.print(mqttClient.state());
      //Serial.println(" try again in 5 seconds");
    }
  }
}

//--------------------------------------------------------------------------------------------------------------------//

void connection_wifi() {
  uint8_t Connect_TimeOut = 0;
  //WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin();
  while(Connect_TimeOut < 100)
  {
    if(WiFi.status() == WL_CONNECTED)
    {
      u8g2_GFX.drawStr(0, 57, "     CONNECTED     "); display.display();
      return;
    }
    delay(200);
    Connect_TimeOut++;
  }
  delay(500);
  u8g2_GFX.drawStr(0, 57, "CAN'T CONNECT WIFI"); display.display();
}

void Check_Wifi_Connection() {
  if(millis() - current_time >= 3000) {
    current_time = millis();
    //Check wifi status
    if(WiFi.status() == WL_CONNECTED) {
      //Serial.println("Wifi OK");
      status_time = 1000;
    }
    else {
      status_time = 200;
      WiFiManager wm;  
      //Serial.println("Wifi loss");
      if(ScanWiFiAvailable(wm.getWiFiSSID())) {
        //Serial.println("Ket noi lai WIFI");
        ESP.restart();
      }
      //Serial.println(wm.getWiFiSSID());
    }

    if(Node2_TimeOut >= 3) {StatusToSend.node_2 = 0; stt_4from2 = 0;}
    else {Node2_TimeOut += 1;}
    
    if(Node3_TimeOut >= 3) {StatusToSend.node_3 = 0; stt_4from3 = 0;}
    else {Node3_TimeOut += 1;}

    StatusToSend.node_4 = stt_4from2 | stt_4from3;
    //if(StatusToSend.node_2 == 0 && StatusToSend.node_3 == 0)  StatusToSend.node_4 = 0;

    JSONVar JSON_DataStatus;
    StatusToSend.node_1 = 1;
    JSON_DataStatus["node1"] = StatusToSend.node_1; JSON_DataStatus["node2"] = StatusToSend.node_2;
    JSON_DataStatus["node3"] = StatusToSend.node_3; JSON_DataStatus["node4"] = StatusToSend.node_4;
    String jsonString = JSON.stringify(JSON_DataStatus);
    if(mqttClient.connected()) mqttClient.publish("esp8266hufi/status",jsonString.c_str());
  }
}

bool ScanWiFiAvailable(String ssid) {
  if(int32_t n = WiFi.scanNetworks()) {
    for(uint8_t i=0; i<n; i++) {
      if(ssid == WiFi.SSID(i)){
        return true;
      }
    }
  }
  return false;
}
//--------------------------------------------------------------------------------------------------//

bool long_press()
{
  static uint16_t lastpress = 0;
  if(millis() - lastpress >= 5000 && digitalRead(TRIGGER_PIN) == 0)
  {
    lastpress = 0;
    return true;
  }
  else if(digitalRead(TRIGGER_PIN) == 1) lastpress = millis();
  return false;
}

void Check_Button() {
  if(digitalRead(BUTTON1) == 0) {
    delay(20);
    while(digitalRead(BUTTON1) == 0);
    digitalWrite(RELAY1,!digitalRead(RELAY1));
    if(digitalRead(RELAY1)) Node1_Data_Relay.RL1 = 0;
    else Node1_Data_Relay.RL1 = 1;
    JSONVar JSON_DataRelay;
    JSON_DataRelay["state"]["desired"]["RL1"] = String(Node1_Data_Relay.RL1);
    JSON_DataRelay["state"]["desired"]["RL2"] = String(Node1_Data_Relay.RL2);
    JSON_DataRelay["state"]["reported"]["RL1"] = String(Node1_Data_Relay.RL1);
    JSON_DataRelay["state"]["reported"]["RL2"] = String(Node1_Data_Relay.RL2);
    String jsonString = JSON.stringify(JSON_DataRelay);
    if(mqttClient.connected()) mqttClient.publish("$aws/things/esp8266hufi/shadow/name/node1/update",jsonString.c_str());
  }
  if(digitalRead(BUTTON2) == 0) {
    delay(20);
    while(digitalRead(BUTTON2) == 0);
    digitalWrite(RELAY2,!digitalRead(RELAY2));
    if(digitalRead(RELAY2)) Node1_Data_Relay.RL2 = 0;
    else Node1_Data_Relay.RL2 = 1;
    JSONVar JSON_DataRelay;
    JSON_DataRelay["state"]["desired"]["RL1"] = String(Node1_Data_Relay.RL1);
    JSON_DataRelay["state"]["desired"]["RL2"] = String(Node1_Data_Relay.RL2);
    JSON_DataRelay["state"]["reported"]["RL1"] = String(Node1_Data_Relay.RL1);
    JSON_DataRelay["state"]["reported"]["RL2"] = String(Node1_Data_Relay.RL2);
    String jsonString = JSON.stringify(JSON_DataRelay);
    if(mqttClient.connected()) mqttClient.publish("$aws/things/esp8266hufi/shadow/name/node1/update",jsonString.c_str());
  }
}

//--------------------------------------------------------------------------------------------------//

void led_status()
{
//  if(millis() - led_time >= status_time)
//  {
//    digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));
//    led_time = millis(); 
//  }
  digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));
  TimerLed.attach_ms(status_time, led_status);
}

void SendToNode2_Again() {
  if(millis() - Node2_Wait_SendAgain >= 1000 && SendNode2_OK == 0) {
    Node2_Wait_SendAgain = millis();
    Sending_Type = 1;
    esp_now_send(ParentNode_Addr_1, (uint8_t *) &Node2_Data_Relay, sizeof(Node2_Data_Relay));
  }
}

void SendToNode3_Again() {
  if(millis() - Node3_Wait_SendAgain >= 1000 && SendNode3_OK == 0) {
    Node3_Wait_SendAgain = millis();
    Sending_Type = 2;
    esp_now_send(ParentNode_Addr_2, (uint8_t *) &Node3_Data_Relay, sizeof(Node3_Data_Relay));
  }
}

void SendToNode4_Again() {
  if(millis() - Node4_Wait_SendAgain >= 1000 && SendNode4_OK == 0) {
    Node4_Wait_SendAgain = millis();
    Sending_Type = 3;
    if(StatusToSend.node_2 == 1) esp_now_send(ParentNode_Addr_1, (uint8_t *) &Node4_Data_Relay, sizeof(Node4_Data_Relay));
    else esp_now_send(ParentNode_Addr_2, (uint8_t *) &Node4_Data_Relay, sizeof(Node4_Data_Relay));
  }
}


//void SendToNode_Again() {
//  if(millis() - Wait_SendAgain >= 1000) {
//    Wait_SendAgain = millis();
//    if(SendNode2_OK == 0) {
//      Sending_Type = 1; esp_now_send(ParentNode_Addr_1, (uint8_t *) &Node2_Data_Relay, sizeof(Node2_Data_Relay));
//    }
//    if(SendNode3_OK == 0) {
//      Sending_Type = 2; esp_now_send(ParentNode_Addr_2, (uint8_t *) &Node3_Data_Relay, sizeof(Node3_Data_Relay));
//    }
//    if(SendNode4_OK == 0) {
//      Sending_Type = 3;
//      if(StatusToSend.node_2 == 1) esp_now_send(ParentNode_Addr_1, (uint8_t *) &Node4_Data_Relay, sizeof(Node4_Data_Relay));
//      else esp_now_send(ParentNode_Addr_2, (uint8_t *) &Node4_Data_Relay, sizeof(Node4_Data_Relay));
//    }
//  }
//}

//--------------------------------------------------------------------------------------------------//

void showDisplay_detail(uint8_t node, uint8_t temp, uint8_t hum, bool RL1, bool RL2) {
  display.clearDisplay(); 
  display.drawLine(0, 10, 127, 10, WHITE); display.drawLine(0, 54, 127, 54, WHITE);
  
  display.setCursor(1, 1); 
  switch(node) {
    case 1: 
      display.print("IoT GATEWAY"); 
      if(WiFi.status() == WL_CONNECTED) {
         u8g2_GFX.setFont(u8g2_font_open_iconic_www_1x_t);
         u8g2_GFX.setCursor(120,8); u8g2_GFX.write(72);
      } else {
        //display.fillRect(120, 0, 127, 8, SSD1306_BLACK);
        u8g2_GFX.setFont(u8g2_font_open_iconic_www_1x_t);
        u8g2_GFX.setCursor(120,8); u8g2_GFX.write(74);
      }
      break;
    case 2: 
      display.print("IoT NODE 2");
      if(StatusToSend.node_2 == 1) {
        u8g2_GFX.setFont(u8g2_font_open_iconic_www_1x_t);
        u8g2_GFX.setCursor(120,8); u8g2_GFX.write(83);
      } else {
        //display.fillRect(120, 0, 127, 8, SSD1306_BLACK);
        u8g2_GFX.setFont(u8g2_font_open_iconic_www_1x_t);
        u8g2_GFX.setCursor(120,8); u8g2_GFX.write(74);
      }
      break;
    case 3: 
      display.print("IoT NODE 3");
      if(StatusToSend.node_3 == 1) {
        u8g2_GFX.setFont(u8g2_font_open_iconic_www_1x_t);
        u8g2_GFX.setCursor(120,8); u8g2_GFX.write(83);
      } else {
        //display.fillRect(120, 0, 127, 8, SSD1306_BLACK);
        u8g2_GFX.setFont(u8g2_font_open_iconic_www_1x_t);
        u8g2_GFX.setCursor(120,8); u8g2_GFX.write(74);
      }
      break;
    case 4: 
      display.print("IoT NODE 4");
      if(StatusToSend.node_4 == 1) {
        u8g2_GFX.setFont(u8g2_font_open_iconic_www_1x_t);
        u8g2_GFX.setCursor(120,8); u8g2_GFX.write(83);
      } else {
        //display.fillRect(120, 0, 127, 8, SSD1306_BLACK);
        u8g2_GFX.setFont(u8g2_font_open_iconic_www_1x_t);
        u8g2_GFX.setCursor(120,8); u8g2_GFX.write(74);
      }
      break;
  }
  
  display.setCursor(1, 14); display.write(16); display.print(" TEMPERATURE: "); 
  if(temp <= 0 ) display.print("NA");
  else {display.print(temp); display.write(248); display.print("C");}
  
  display.setCursor(1, 24); display.write(16); display.print(" HUMIDITY: ");
  if(hum <= 0 ) display.print("NA");
  else {display.print(hum); display.print("%");}
  
  display.setCursor(1, 34); display.write(16); 
  switch(RL1) {
    case 0: display.print(" RELAY1: OFF"); break;
    case 1: display.print(" RELAY1: ON "); break;
  }
   
  display.setCursor(1, 44); display.write(16);
  switch(RL2) {
    case 0: display.print(" RELAY2: OFF"); break;
    case 1: display.print(" RELAY2: ON "); break;
  }

  //now = time(nullptr);
  if(WiFi.status() == WL_CONNECTED)
  {
    struct tm *time_info;
    char buffer_time[80];
    time(&now);
    time_info = localtime(&now);
    strftime(buffer_time,80,"%a %d/%m/%y, %I:%M%p", time_info);
    display.setCursor(1, 57); display.print(buffer_time);
  }
  
  display.display();
}
//--------------------------------------------------------------------------------------------------//

//  while (Serial.available()) 
//  {
//    char inChar = (char)Serial.read();
//    if (inChar == '\n') 
//    {
//      Serial.println(inputString);
//
//      if(inputString == "sendRelay_2")
//      {
//        struct_message_DataRelay Data_Relay;
//        Data_Relay.fromID = BOARD_ID;
//        Data_Relay.toID = 2;
//        Data_Relay.RL1 = 0;
//        Data_Relay.RL2 = 1;
//        esp_now_send(ParentNode_Addr_1, (uint8_t *) &Data_Relay, sizeof(Data_Relay));
//      }
//      else if(inputString == "sendRelay_4") 
//      {
//        struct_message_DataRelay Data_Relay;
//        Data_Relay.fromID = BOARD_ID;
//        Data_Relay.toID = 4;
//        Data_Relay.RL1 = 1;
//        Data_Relay.RL2 = 1;
//        esp_now_send(ParentNode_Addr_1, (uint8_t *) &Data_Relay, sizeof(Data_Relay));
//      }  
//      inputString = "";
//    }
//    else
//    {
//      inputString += inChar;
//    }
//  }
