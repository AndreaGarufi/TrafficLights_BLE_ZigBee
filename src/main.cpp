#include <iot_board.h>
#include <BLEDevice.h>


#define SERVICE_UUID "70cb215d-b1a7-4963-90c4-5ad3a70caf0c"

// One characteristic for each configuration parameter
#define MASTER_UUID       "41cf179f-f164-494b-9225-55daf6f574a4"
#define ColorOffset_UUID    "98ac8717-565f-4356-bb76-82fbc390f509"

#define MAX_NODES 4
#define HEARTBEAT_INTERVAL 500  // Ogni scheda manda HELLO ogni 500 ms
#define NODE_TIMEOUT       2000  // Se non si sente per 2 secondi, viene rimossa

struct PeerNode {
  uint16_t addr;
  unsigned long lastSeen; // Timestamp millis() dell'ultimo HELLO
  bool active;
};

PeerNode nodes[MAX_NODES];  //by default all is set at 0 by the compiler

static enum{
  HELLO_PKT = 0,
  START_PKT = 1
}PacketType;

/**
 * Configuration variables:
 * channel   : 802.15.4 channel, possible values 11-16
 * PAN_ID    : id of the private network, necessary for communication
 * MY_ID     : id of the device inside the PAN
 * connected : whether or not the device is connected to the PAN
 * master    : whether or not the device is the master
 * ColorOffset : whether or not the traffic light has an offset 
 *             in relation to the other traffic lights
 */

uint16_t channel = 15;
uint16_t PAN_ID = 0x6769;
uint16_t MY_ID = 0x1717;
volatile bool connected = false;
volatile bool master = false;
volatile bool ColorOffset = false;  //decides with what colour a node have to start
volatile bool isIdle = true;

static void applyZigbeeConfig() {            //ZigBee setup
  if (!zigbee) {
    return;
  }

  zigbee->setAddress(MY_ID);
  zigbee->setPanId(PAN_ID);
  zigbee->setChannel(channel);
  zigbee->setCoordinator(master);

  Serial.printf("ZigBee: CH=%d PAN_ID=0x%04X MY_ID=0x%04X coordinator=%s\n",
                channel, PAN_ID, MY_ID, master ? "true" : "false");
}

static void drawDisplay() {
  if(connected){
    display->clearDisplay();
    display->setCursor(0,0);
    display->printf("Connected!\n");
    display->printf("CH: %d\n", channel);
    display->printf("PAN: 0x%04X\n", PAN_ID);
    display->printf("MY_ID: 0x%04X\n", MY_ID);
    display->printf("Master: %s\n", master ? "true" : "false");
    display->printf("ColorOffset_UUID: %s\n", ColorOffset ? "true" : "false");
    display->display();

  }else{
    display->clearDisplay();
    display->setCursor(0,0);
    display->printf("Not connected");
    display->display();    
  }
}

class MasterCallbacks : public BLECharacteristicCallbacks { 

  void onWrite(BLECharacteristic *c) {  //function for deciding who is the master and who is the slave
    String v = c->getValue();
    v.trim();
    v.toLowerCase();

    if (v == "master" || v == "true" || v == "1" || v == "on") {
      master = true;
    } else if (v == "slave" || v == "false" || v == "0" || v == "off") {
      master = false;
    } else {
      return;
    }

    c->setValue(master ? "master" : "slave");
    applyZigbeeConfig();  //appling master to the ZigBee setup
  }
};

class ColorOffsetCallbacks : public BLECharacteristicCallbacks {

  void onWrite(BLECharacteristic *c) {  //funciont for deciding who have to start green or red
    String v = c->getValue();
    v.trim();
    v.toLowerCase();

    if (v == "true" || v == "1" || v == "on" || v == "green") {
      ColorOffset = true;
    } else if (v == "false" || v == "0" || v == "off" || v == "red") {
      ColorOffset = false;
    } else {
      return;
    }

    c->setValue(ColorOffset ? "true" : "false");
  }
};

class ServerCallbacks : public BLEServerCallbacks{

  void onConnect(BLEServer *s){
    connected = true;
  }

  void onDisconnect(BLEServer *s){
    connected = false;
    BLEDevice::startAdvertising();
  }


};

void start_packet(){


}

void updateNodeHeartbeat(uint16_t addr) {
  if (addr == 0 || addr == 0xFFFF || addr == MY_ID) return;

  unsigned long now = millis();
  int freeSlot = -1;

  for (int i = 0; i < MAX_NODES; i++) {
    if (nodes[i].active) {
      if (nodes[i].addr == addr) {
        nodes[i].lastSeen = now; // Nodo già noto: rinnova il timer
        return;
      }
    } else if (freeSlot == -1) {
      freeSlot = i; // Memorizza il primo posto libero
    }
  }

  // Se non era in lista e c'è spazio, aggiungilo
  if (freeSlot != -1) {
    nodes[freeSlot].addr = addr;
    nodes[freeSlot].lastSeen = now;
    nodes[freeSlot].active = true;
  }

  uint8_t activeCounter = 0;
  for (int i = 0; i < MAX_NODES; i++) {
    if (nodes[i].active) {
      activeCounter++;
    }
  }
  if(activeCounter == 4){
    isIdle = false;
  }
  
}


void pruneDeadNodes() {
  unsigned long now = millis();
  // Partiamo da 1 perché nodes[0] siamo noi stessi
  for (int i = 1; i < MAX_NODES; i++) {
    if (nodes[i].active && (now - nodes[i].lastSeen > NODE_TIMEOUT)) {
      //Serial.printf("Nodo 0x%04X scaduto, rimosso!\n", nodes[i].addr);
      nodes[i].active = false;
      nodes[i].addr = 0;
      isIdle = true;
    } 
  }
}


                            //0                     1
bool received = false, isHelloPacket = false, isStartPacket = false;
//uint16_t lastAddrRecv;
uint8_t whatPacket = 0;

void onZigbeeReceive(rx_info_t *packet) {
  uint16_t senderAddr = packet->src_addr; // Legge direttamente l'indirizzo MAC[cite: 1, 2]
  uint8_t whatPacket = zigbee->read();

  if (whatPacket == HELLO_PKT) {
    updateNodeHeartbeat(senderAddr);
  } else if (whatPacket == START_PKT) {
    isStartPacket = true;
  }else{
    return;
  }
}

void sendHelloPacket(){

  zigbee->startPacket();
  zigbee->write((uint8_t)HELLO_PKT);  //writes uint8_t 0
  zigbee->sendPacket(0xFFFF, false);  //false means that we not want ACK 

}


void setup(){

  IoTBoard::init_serial();
  IoTBoard::init_buttons();
  IoTBoard::init_display();
  IoTBoard::init_leds();

  IoTBoard::init_spi();
  if(IoTBoard::init_zigbee()){
    display->clearDisplay();
    display->setCursor(0,0);
    display->println("ZIGBEE OK");
    display->display();
    Serial.printf("ZigBee OK\n");
  }

  applyZigbeeConfig();

  BLEDevice::init("TF-black");  //blue, black, white, orange
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  BLEService *svc = server->createService(SERVICE_UUID);

  BLECharacteristic *masterCharacteristic = svc->createCharacteristic(MASTER_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE);
  masterCharacteristic->setValue(master ? "master" : "slave");
  masterCharacteristic->setCallbacks(new MasterCallbacks());

  BLECharacteristic *ColorOffsetCharacteristic = svc->createCharacteristic(ColorOffset_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE);
  ColorOffsetCharacteristic->setValue(ColorOffset ? "true" : "false");
  ColorOffsetCharacteristic->setCallbacks(new ColorOffsetCallbacks());

  svc->start();
  server->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  zigbee->onReceiveCallback(onZigbeeReceive);

  nodes[0].addr = MY_ID;
  nodes[0].lastSeen = millis();
  nodes[0].active = true;
}


void loop(){

  if(isStartPacket){
    start_packet();
  }

  if(IoTBoard::runEvery(1000,1)){
    pruneDeadNodes();
  }

  if(isIdle){
    if(IoTBoard::runEvery(500,2)){
      digitalWrite(LED_YELLOW, !digitalRead(LED_YELLOW));  
    }
  }

  if(IoTBoard::runEvery(500,3)){
    sendHelloPacket();  
  }



  // Update the display only every 500 ms
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 500) {
    return;
  }
  lastDraw = millis();

  drawDisplay();



}
