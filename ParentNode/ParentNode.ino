#include <ESP8266WiFi.h>
#include <espnow.h>
#include <DHT.h>

#define BOARD_ID 3

#define BUTTON1 12  
#define BUTTON2 13
#define RELAY1 4
#define RELAY2 5

#define DHT_TYPE DHT11
#define DHT_PIN  14
DHT _DHT11(DHT_PIN, DHT_TYPE);

uint32_t current_time = 0;
uint32_t wait_readSensor = 0;

uint32_t RL_1to4_Wait = 0;
uint32_t RL_2to1_Wait = 0;
uint32_t RL_4to1_Wait = 0;
uint32_t SS_2to1_Wait = 0;
uint32_t SS_4to1_Wait = 0;

bool RL_1to4_Send_OK = 1;
bool RL_2to1_Send_OK = 1;
bool RL_4to1_Send_OK = 1;
bool SS_2to1_Send_OK = 1;
bool SS_4to1_Send_OK = 1;

uint8_t Sending_Type = 0;

uint32_t led_time = 0;
uint16_t status_time = 200;
bool CONNECT_OK = 0;
uint8_t Wait_Connect = 42;
uint32_t LeafNode_TimeOut = 0;
uint8_t Fail = 0;

bool RL_1to2_Rcv = 0;
bool RL_1to4_Rcv = 0;
bool RL_4to1_Rcv = 0;
bool SS_4to1_Rcv = 0;

uint8_t RootNode_Addr[6] = {0xC8, 0xC9, 0xA3, 0x58, 0x0B, 0x91};
uint8_t LeafNode_Addr[6] = {0xC8, 0xC9, 0xA3, 0x58, 0x0B, 0xEA};


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

typedef struct struct_message_Status{
    uint8_t type = '!';
    uint8_t fromID;
    uint8_t node_id1 = 0;
    uint8_t node_id2 = 0;
} struct_message_Status;

// Create a struct_message called DHTReadings to hold sensor readings
struct_message_DataSensor ParentNode_Sensor;
struct_message_DataRelay ParentNode_Relay;
struct_message_Status StatusToSend;

struct_message_DataRelay RelayNode1toNode4;
struct_message_DataRelay RelayNode4toNode1;
struct_message_DataSensor SensorNode4toNode1;
// Create a struct_message to hold incoming sensor readings

// Callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  if (sendStatus == 0){
//    Serial.println("Delivery success");
    if(memcmp(mac_addr,RootNode_Addr,sizeof(mac_addr)) == 0) {
      Fail = 0; CONNECT_OK = 1; status_time = 1000;
      if(Sending_Type == 1) RL_2to1_Send_OK = 1;
      else if(Sending_Type == 2) RL_4to1_Send_OK = 1;
      else if(Sending_Type == 3) SS_2to1_Send_OK = 1;
      else SS_4to1_Send_OK = 1;
    }
    if(memcmp(mac_addr,LeafNode_Addr,sizeof(mac_addr)) == 0) RL_1to4_Send_OK = 1;
  }
  else{
//    Serial.println("Delivery fail");
    if(memcmp(mac_addr,RootNode_Addr,sizeof(mac_addr)) == 0) {
      if(Fail == 3) {ESPNOW_Scan_Channel(RootNode_Addr); status_time = 200; Fail = 0;}
      Fail += 1;
      if(Sending_Type == 1) {RL_2to1_Send_OK = 0; RL_2to1_Wait = millis();}
      else if(Sending_Type == 2) {RL_4to1_Send_OK = 0; RL_4to1_Wait = millis();}
      else if(Sending_Type == 3) {SS_2to1_Send_OK = 0; SS_2to1_Wait = millis();}
      else {SS_4to1_Send_OK = 0; SS_4to1_Wait = millis();}
    }
    if(memcmp(mac_addr,LeafNode_Addr,sizeof(mac_addr)) == 0) {RL_1to4_Send_OK = 0; RL_1to4_Wait = millis();}
  }
}

// Callback when data is received
void IRAM_ATTR OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len){
  if((char)incomingData[0] == '#')
  {
    struct_message_DataRelay DataReading_Relay;
    memcpy(&DataReading_Relay, incomingData, sizeof(DataReading_Relay));
    if(DataReading_Relay.fromID == 1 && DataReading_Relay.toID == 4){ 
      //Serial.println("Send Data Node1 to Node4");
      RelayNode1toNode4.RL1 = DataReading_Relay.RL1; RelayNode1toNode4.RL2 = DataReading_Relay.RL2;
      RL_1to4_Rcv = 1;
    }
    else if(DataReading_Relay.fromID == 4 && DataReading_Relay.toID == 1){ 
      //Serial.println("Send Data Node4 to Node1");
      RelayNode4toNode1.RL1 = DataReading_Relay.RL1; RelayNode4toNode1.RL2 = DataReading_Relay.RL2;
      RL_4to1_Rcv = 1;
    }
    else{
      //Serial.println("Data node1 to node2");
      ParentNode_Relay.RL1 = DataReading_Relay.RL1; ParentNode_Relay.RL2 = DataReading_Relay.RL2;
      RL_1to2_Rcv = 1;
    }
  }
  if((char)incomingData[0] == '%')
  {
    struct_message_DataSensor DataReading_Sensor;
    memcpy(&DataReading_Sensor, incomingData, sizeof(DataReading_Sensor));
    if(DataReading_Sensor.fromID == 4 && DataReading_Sensor.toID == 1) {
      SensorNode4toNode1.temp = DataReading_Sensor.temp; SensorNode4toNode1.humi = DataReading_Sensor.humi;
      SS_4to1_Rcv = 1;
    }
  }
  if((char)incomingData[0] == 'O' && (char)incomingData[1] == 'K')
  {
    LeafNode_TimeOut = 0; StatusToSend.node_id2 = 1;
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
  esp_now_add_peer(RootNode_Addr, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  esp_now_add_peer(LeafNode_Addr, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  esp_now_register_recv_cb(OnDataRecv);

  //Wait connected to Gateway
  while(Wait_Connect--){
    esp_now_send(RootNode_Addr, (uint8_t *) "OK", 2);
    if(CONNECT_OK) break;
    delay(300);
  }
  
  //Set values to send
  ParentNode_Relay.fromID = BOARD_ID; ParentNode_Relay.toID = 1;
  ParentNode_Sensor.fromID = BOARD_ID; ParentNode_Sensor.toID = 1;
  StatusToSend.fromID = BOARD_ID; StatusToSend.node_id1 = 1; 

  RelayNode1toNode4.fromID = 1; RelayNode1toNode4.toID = 4;
  RelayNode4toNode1.fromID = 4; RelayNode4toNode1.toID = 1;
  SensorNode4toNode1.fromID = 4; SensorNode4toNode1.toID = 1;
}
 
void loop() {

  if(millis() - current_time >= 1000)
  {
    current_time = millis();
    LeafNode_TimeOut += 1;
    if(LeafNode_TimeOut >= 2) StatusToSend.node_id2 = 0;
    Sending_Type = 0; esp_now_send(RootNode_Addr, (uint8_t *) &StatusToSend, sizeof(StatusToSend));
  }
  
  Check_Button();
  ESPNOW_Rcv_Handle();
  led_status();
  
  RL_1to4_Againt();
  RL_2to1_Againt();
  RL_4to1_Againt();
  SS_2to1_Againt();
  SS_4to1_Againt();

  if(millis() - wait_readSensor >= 10000) {
    wait_readSensor = millis();
    uint8_t t = _DHT11.readTemperature();
    uint8_t h = _DHT11.readHumidity();
    if(t > 0 && t <= 100 && h > 0 && h <= 100) {
      ParentNode_Sensor.temp = t; ParentNode_Sensor.humi = h;
      Sending_Type = 3; esp_now_send(RootNode_Addr, (uint8_t *) &ParentNode_Sensor, sizeof(ParentNode_Sensor));
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
  channel_scan += 1; if(channel_scan == 12) channel_scan = 1;
}

void Check_Button() {
  if(digitalRead(BUTTON1) == 0) {
    delay(20);
    while(digitalRead(BUTTON1) == 0);
    digitalWrite(RELAY1,!digitalRead(RELAY1));
    if(digitalRead(RELAY1)) ParentNode_Relay.RL1 = 0;
    else ParentNode_Relay.RL1 = 1;
    Sending_Type = 1; esp_now_send(RootNode_Addr, (uint8_t *) &ParentNode_Relay, sizeof(ParentNode_Relay));
  }
  if(digitalRead(BUTTON2) == 0) {
    delay(20);
    while(digitalRead(BUTTON2) == 0);
    digitalWrite(RELAY2,!digitalRead(RELAY2));
    if(digitalRead(RELAY2)) ParentNode_Relay.RL2 = 0;
    else ParentNode_Relay.RL2 = 1;
    Sending_Type = 1; esp_now_send(RootNode_Addr, (uint8_t *) &ParentNode_Relay, sizeof(ParentNode_Relay));
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

void RL_1to4_Againt() {
  if(millis() - RL_1to4_Wait >= 1000 && RL_1to4_Send_OK == 0) {
    RL_1to4_Wait = millis();
    esp_now_send(LeafNode_Addr, (uint8_t *) &RelayNode1toNode4, sizeof(RelayNode1toNode4));
  }
}

void RL_2to1_Againt() {
  if(millis() - RL_2to1_Wait >= 1000 && RL_2to1_Send_OK == 0) {
    RL_2to1_Wait = millis();
    Sending_Type = 1; esp_now_send(RootNode_Addr, (uint8_t *) &ParentNode_Relay, sizeof(ParentNode_Relay));
  }
}

void RL_4to1_Againt() {
  if(millis() - RL_4to1_Wait >= 1000 && RL_4to1_Send_OK == 0) {
    RL_4to1_Wait = millis();
    Sending_Type = 2; esp_now_send(RootNode_Addr, (uint8_t *) &RelayNode4toNode1, sizeof(RelayNode4toNode1));
  }
}


void SS_2to1_Againt() {
  if(millis() - SS_2to1_Wait >= 1000 && SS_2to1_Send_OK == 0) {
    SS_2to1_Wait = millis();
    Sending_Type = 3; esp_now_send(RootNode_Addr, (uint8_t *) &ParentNode_Sensor, sizeof(ParentNode_Sensor));
  }
}

void SS_4to1_Againt() {
  if(millis() - SS_4to1_Wait >= 1000 && SS_4to1_Send_OK == 0) {
    SS_4to1_Wait = millis();
    Sending_Type = 4; esp_now_send(RootNode_Addr, (uint8_t *) &SensorNode4toNode1, sizeof(SensorNode4toNode1));
  }
}


void ESPNOW_Rcv_Handle() {
  if(RL_1to4_Rcv == 1) {
    esp_now_send(LeafNode_Addr, (uint8_t *) &RelayNode1toNode4, sizeof(RelayNode1toNode4));
    RL_1to4_Rcv = 0;
  }
  if(RL_1to2_Rcv == 1) {
    if(ParentNode_Relay.RL1 == 1) digitalWrite(RELAY1,LOW); 
    else digitalWrite(RELAY1,HIGH);
    if(ParentNode_Relay.RL2 == 1) digitalWrite(RELAY2,LOW);
    else digitalWrite(RELAY2,HIGH);
    Sending_Type = 1; esp_now_send(RootNode_Addr, (uint8_t *) &ParentNode_Relay, sizeof(ParentNode_Relay));
    RL_1to2_Rcv = 0;
  }
  if(RL_4to1_Rcv == 1) {
    Sending_Type = 2; esp_now_send(RootNode_Addr, (uint8_t *) &RelayNode4toNode1, sizeof(RelayNode4toNode1));
    RL_4to1_Rcv = 0;
  }
  if(SS_4to1_Rcv == 1) {
    Sending_Type = 4; esp_now_send(RootNode_Addr, (uint8_t *) &SensorNode4toNode1, sizeof(SensorNode4toNode1));
    SS_4to1_Rcv = 0;
  }
}
