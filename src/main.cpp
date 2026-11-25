#include <iot_board.h>
#include <BLEDevice.h>


#define SERVICE_UUID "70cb215d-b1a7-4963-90c4-5ad3a70caf0c"

// One characteristic for each configuration parameter
#define MASTER_UUID       "41cf179f-f164-494b-9225-55daf6f574a4"
#define addOffset_UUID    "41cf179f-f164-494b-9225-55daf6f574a5"

/**
 * Configuration variables:
 * channel   : 802.15.4 channel, possible values 11-16
 * PAN_ID    : id of the private network, necessary for communication
 * MY_ID     : id of the device inside the PAN
 * connected : whether or not the device is connected to the PAN
 * master    : whether or not the device is the master
 * addOffset : whether or not the traffic light has an offset 
 *             in relation to the other traffic lights
 */

uint16_t channel = 15;
uint16_t PAN_ID = 0x6769;
uint16_t MY_ID = 0x1717;
volatile bool connected = false;
volatile bool master = false;
volatile bool addOffset = false;

static void applyZigbeeConfig() {
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
    display->printf("addOffset: %s\n", addOffset ? "true" : "false");
    display->display();

  }else{
    display->clearDisplay();
    display->setCursor(0,0);
    display->printf("Not connected");
    display->display();    
  }
}

class MasterCallbacks : public BLECharacteristicCallbacks {

  void onWrite(BLECharacteristic *c) {
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
    applyZigbeeConfig();
  }
};

class addOffsetCallbacks : public BLECharacteristicCallbacks {

  void onWrite(BLECharacteristic *c) {
    String v = c->getValue();
    v.trim();
    v.toLowerCase();

    if (v == "true" || v == "1" || v == "on") {
      addOffset = true;
    } else if (v == "false" || v == "0" || v == "off") {
      addOffset = false;
    } else {
      return;
    }

    c->setValue(addOffset ? "true" : "false");
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

  BLEDevice::init("TF-black");  //blue, black, white
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  BLEService *svc = server->createService(SERVICE_UUID);

  BLECharacteristic *masterCharacteristic = svc->createCharacteristic(MASTER_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE);
  masterCharacteristic->setValue(master ? "master" : "slave");
  masterCharacteristic->setCallbacks(new MasterCallbacks());

  BLECharacteristic *addOffsetCharacteristic = svc->createCharacteristic(addOffset_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE);
  addOffsetCharacteristic->setValue(addOffset ? "true" : "false");
  addOffsetCharacteristic->setCallbacks(new addOffsetCallbacks());

  svc->start();
  server->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

}


void loop(){
  // Update the display only every 500 ms
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 500) {
    return;
  }
  lastDraw = millis();

  drawDisplay();
}
