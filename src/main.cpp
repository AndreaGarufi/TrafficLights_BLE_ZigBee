#include <iot_board.h>
#include <BLEDevice.h>

#define SERVICE_UUID "70cb215d-b1a7-4963-90c4-5ad3a70caf0c"
#define TRAFFIC_LIGHT_UUID "41cf179f-f164-494b-9225-55daf6f574ad"

uint16_t channel = 15;
uint16_t PAN_ID = 0x6769;
uint16_t MY_ID = 0x1717;


volatile bool connected = false;
//Passare con BLE se è master,stato di partenza
volatile bool master = false, green = false, red = false;

BLECharacteristic *role;

class SetupCallbacks : public BLECharacteristicCallbacks {
  
  void onWrite(BLECharacteristic *c) {     // a central wrote

    if(green || red){
      return;
    }

    if(c->getValue() == "master"){  //set master
      master = true;
    }else if(c->getValue() == "rosso"){
      red = true;
    }else if(c->getValue() == "verde"){
      green = true;
    }

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
    display->println("ZIGBEE Buono");
    display->display();
    Serial.printf("ZigBee Buono");
  }

  zigbee->setPanId(channel);

  BLEDevice::init("Semaforino-arancione");  //blue,nero,bianco            
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  BLEService *svc = server->createService(SERVICE_UUID);
  role = svc->createCharacteristic(TRAFFIC_LIGHT_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY);

    role->setCallbacks(new SetupCallbacks());
    svc->start();
    server->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

}


void loop(){

  if(connected){
    display->clearDisplay();
    display->setCursor(0,0);
    display->printf("Connesso! PAN ID: %d \nMY_ID: %d \nCHANNEL %d\n",PAN_ID,MY_ID,channel);
    display->printf("Master: %s \n", master ? "true" : "false");
    display->printf("Verde: %s \n", green ? "true" : "false");
    display->printf("Rosso: %s \n", red ? "true" : "false");    
    display->display();

  }else{
    display->clearDisplay();
    display->setCursor(0,0);
    display->printf("Non connesso");
    display->display();    
  }


}

