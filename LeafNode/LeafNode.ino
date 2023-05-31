#include <ESP8266WiFi.h>
#include <espnow.h>
#include <DHT.h>

#define BOARD_ID 4
#define Node2 0
#define Node3 1

#define BUTTON1 12  
#define BUTTON2 13
#define RELAY1 4
#define RELAY2 5

#define DHT_TYPE DHT11
#define DHT_PIN  14
DHT _DHT11(DHT_PIN, DHT_TYPE);

uint32_t current_time = 0;
uint32_t Wait_readSensor = 0;
uint8_t CONNECT_TIMEOUT = 0;
uint32_t led_time = 0;
uint16_t status_time = 200;

uint32_t RL_4to1_Wait = 0;
uint32_t SS_4to1_Wait = 0;
bool RL_Send_Ok = 1;
bool SS_Send_Ok = 1;
uint8_t Sending_Type = 0;
uint8_t Node2_Fail = 0;
uint8_t Node3_Fail = 0;

bool CONNECT_TO = 0;
bool CONNECT_OK = 0;
uint8_t Wait_Connect = 10;
bool ESPNOW_Rcv_Flag = 0;

uint8_t ParentNode_Addr_1[] = {0xC8, 0xC9, 0xA3, 0x58, 0x0B, 0xED};
uint8_t ParentNode_Addr_2[] = {0xBC, 0xFF, 0x4D, 0xF9, 0x53, 0x21};

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

// Create a struct_message called DHTReadings to hold sensor readings
struct_message_DataSensor DataToSend_Sensor;
struct_message_DataRelay DataToSend_Relay;

// Callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  if (sendStatus == 0){
//    Serial.println("Delivery success");
    CONNECT_OK = 1; status_time = 1000; 
  
    if(Sending_Type == 1) RL_Send_Ok = 1;
    if(Sending_Type == 2) SS_Send_Ok = 1;
    
    if(memcmp(mac_addr,ParentNode_Addr_1,sizeof(mac_addr)) == 0) {CONNECT_TO = Node2; Node2_Fail = 0;}
    if(memcmp(mac_addr,ParentNode_Addr_2,sizeof(mac_addr)) == 0) {CONNECT_TO = Node3; Node3_Fail = 0;}
  }
  else{
//    Serial.println("Delivery fail");
    status_time = 200; 

    if(Sending_Type == 1) {RL_Send_Ok = 0; RL_4to1_Wait = millis();}
    if(Sending_Type == 2) {SS_Send_Ok = 0; SS_4to1_Wait = millis();}

    if(memcmp(mac_addr,ParentNode_Addr_1,sizeof(mac_addr)) == 0) {
      if(Node2_Fail == 3) {
        ESPNOW_Scan_Channel(ParentNode_Addr_1);
        Node2_Fail = 0;
      }
      Node2_Fail += 1;
    }
    if(memcmp(mac_addr,ParentNode_Addr_2,sizeof(mac_addr)) == 0) {
      if(Node3_Fail == 3) {
        ESPNOW_Scan_Channel(ParentNode_Addr_2);
        Node3_Fail = 0;
      }
      Node3_Fail += 1;
    }
    
    if(CONNECT_TIMEOUT == 9 && CONNECT_TO == Node2){CONNECT_TO = Node3; CONNECT_TIMEOUT = 0;} 
    if(CONNECT_TIMEOUT == 9 && CONNECT_TO == Node3){CONNECT_TO = Node2; CONNECT_TIMEOUT = 0;}
    CONNECT_TIMEOUT += 1; 
  }
}

// Callback when data is received
void IRAM_ATTR OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len){
  if((char)incomingData[0] == '#')
  {
    struct_message_DataRelay DataReading_Relay;
    memcpy(&DataReading_Relay, incomingData, sizeof(DataReading_Relay));
    if(DataReading_Relay.fromID == 1 && DataReading_Relay.toID == 4) {
      DataToSend_Relay.RL1 = DataReading_Relay.RL1; DataToSend_Relay.RL2 = DataReading_Relay.RL2;
      ESPNOW_Rcv_Flag = 1;
    }
  }
}
 
void setup() {
  // Init Serial Monitor
  //Serial.begin(115200);
  delay(5000);

  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(RELAY1, OUTPUT); digitalWrite(RELAY1,HIGH);
  pinMode(RELAY2, OUTPUT); digitalWrite(RELAY2,HIGH);
  pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN,HIGH);

  _DHT11.begin();
  delay(100);
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  // Init ESP-NOW
  if (esp_now_init() != 0) {
    //Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(OnDataSent);
  esp_now_add_peer(ParentNode_Addr_1, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  esp_now_add_peer(ParentNode_Addr_2, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  esp_now_register_recv_cb(OnDataRecv);

  //RL_4to1_Wait connected to RootNode
  while(Wait_Connect){
    if(CONNECT_TO == Node2) esp_now_send(ParentNode_Addr_1, (uint8_t *) "OK", 2);
    else esp_now_send(ParentNode_Addr_2, (uint8_t *) "OK", 2);
    if(CONNECT_OK) break;
    delay(500);
  }

  DataToSend_Relay.fromID = BOARD_ID; DataToSend_Relay.toID = 1;
  DataToSend_Sensor.fromID = BOARD_ID; DataToSend_Sensor.toID = 1;
  //Serial.println("Connect to ParentNode:[OK]");
}
 
void loop() {

  if(millis() - current_time >= 1000)
  {
    current_time = millis();
    Sending_Type = 0;
    if(CONNECT_TO == Node2) esp_now_send(ParentNode_Addr_1, (uint8_t *) "OK", 2);
    else esp_now_send(ParentNode_Addr_2, (uint8_t *) "OK", 2);
//    esp_now_send(ParentNode_Addr_1, (uint8_t *) "OK", 2);
//    esp_now_send(ParentNode_Addr_2, (uint8_t *) "OK", 2);
  }
  
  Check_Button();
  ESPNOW_Rcv_Handle();
  led_status();

  RL_4to1_Againt();
  SS_4to1_Againt();

  if(millis() - Wait_readSensor >= 10000) {
    Wait_readSensor = millis();
    uint8_t t = _DHT11.readTemperature();
    uint8_t h = _DHT11.readHumidity();
    if(t > 0 && t <= 100 && h > 0 && h <= 100) {
      DataToSend_Sensor.temp = t; DataToSend_Sensor.humi = h;
      Sending_Type = 2; 
      if(CONNECT_TO == Node2) esp_now_send(ParentNode_Addr_1, (uint8_t *) &DataToSend_Sensor, sizeof(DataToSend_Sensor));
      else esp_now_send(ParentNode_Addr_2, (uint8_t *) &DataToSend_Sensor, sizeof(DataToSend_Sensor));
    }
  }
}

void ESPNOW_Scan_Channel(u8 *macaddr)
{
  static uint8_t channel_scan = 1;
  wifi_promiscuous_enable(1); 
  wifi_set_channel(channel_scan); 
  wifi_promiscuous_enable(0); 
  esp_now_set_peer_channel(macaddr,channel_scan);
  channel_scan += 5; if(channel_scan == 16) channel_scan = 1;
}

void Check_Button() {
  if(digitalRead(BUTTON1) == 0) {
    delay(20);
    while(digitalRead(BUTTON1) == 0);
    digitalWrite(RELAY1,!digitalRead(RELAY1));
    if(digitalRead(RELAY1)) DataToSend_Relay.RL1 = 0;
    else DataToSend_Relay.RL1 = 1;
    Sending_Type = 1;
    if(CONNECT_TO == Node2) esp_now_send(ParentNode_Addr_1, (uint8_t *) &DataToSend_Relay, sizeof(DataToSend_Relay));
    else esp_now_send(ParentNode_Addr_2, (uint8_t *) &DataToSend_Relay, sizeof(DataToSend_Relay));
  }
  if(digitalRead(BUTTON2) == 0) {
    delay(20);
    while(digitalRead(BUTTON2) == 0);
    digitalWrite(RELAY2,!digitalRead(RELAY2));
    if(digitalRead(RELAY2)) DataToSend_Relay.RL2 = 0;
    else DataToSend_Relay.RL2 = 1;
    Sending_Type = 1;
    if(CONNECT_TO == Node2) esp_now_send(ParentNode_Addr_1, (uint8_t *) &DataToSend_Relay, sizeof(DataToSend_Relay));
    else esp_now_send(ParentNode_Addr_2, (uint8_t *) &DataToSend_Relay, sizeof(DataToSend_Relay));
  }
}

void led_status()
{
  if(millis() - led_time >= status_time)
  {
    digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));
    led_time = millis(); 
  }
}

void RL_4to1_Againt() {
  if(millis() - RL_4to1_Wait >= 1000 && RL_Send_Ok == 0) {
    RL_4to1_Wait = millis();
    Sending_Type = 1;
    if(CONNECT_TO == Node2) esp_now_send(ParentNode_Addr_1, (uint8_t *) &DataToSend_Relay, sizeof(DataToSend_Relay));
    else esp_now_send(ParentNode_Addr_2, (uint8_t *) &DataToSend_Relay, sizeof(DataToSend_Relay));
  }
}

void SS_4to1_Againt() {
  if(millis() - SS_4to1_Wait >= 1000 && SS_Send_Ok == 0) {
    SS_4to1_Wait = millis();
    Sending_Type = 2; 
    if(CONNECT_TO == Node2) esp_now_send(ParentNode_Addr_1, (uint8_t *) &DataToSend_Sensor, sizeof(DataToSend_Sensor));
    else esp_now_send(ParentNode_Addr_2, (uint8_t *) &DataToSend_Sensor, sizeof(DataToSend_Sensor));
  }
}

void ESPNOW_Rcv_Handle() {
  if(ESPNOW_Rcv_Flag == 1) {
    if(DataToSend_Relay.RL1 == 1) digitalWrite(RELAY1,LOW);
    else digitalWrite(RELAY1,HIGH);
    if(DataToSend_Relay.RL2 == 1) digitalWrite(RELAY2,LOW);
    else digitalWrite(RELAY2,HIGH);
    
    Sending_Type = 1;
    if(CONNECT_TO == Node2) esp_now_send(ParentNode_Addr_1, (uint8_t *) &DataToSend_Relay, sizeof(DataToSend_Relay));
    else esp_now_send(ParentNode_Addr_2, (uint8_t *) &DataToSend_Relay, sizeof(DataToSend_Relay));
    
    ESPNOW_Rcv_Flag = 0;
  }
}
