#include <SPI.h>
#include <Ethernet.h>
#include <MFRC522.h>
#include <EEPROM2.h>
#include <EthernetUdp.h>

#define RST_PIN 2
#define SS_PIN 3
#define RelayPin 4  /*Пин замка*/
#define BeepPin 5   /*Пин Пищалки*/
#define ledPin 6    /*Пин Диодика*/
#define btnPin 7    /*Пин кнопки выхода*/

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance
EthernetUDP udp;
EthernetClient client;
IPAddress server;
unsigned long uidDec, uidDecTemp;  // для храниения номера метки в десятичном формате
unsigned long lastAttemptTime = 0;
const unsigned long updateInterval = 60*1000;
bool btnState = 0;                  // Состояние кнопки, 1 когда нажата
bool MasterUID = 0;
String incomingString;


void initializePins() {
  pinMode(RelayPin, OUTPUT);
  pinMode(BeepPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(btnPin, INPUT);
  digitalWrite(RelayPin, 1);
}

void initializeEthernet() {
  byte mac[] = { 0xDE, 0xAD, 0xBE, 0xFF, 0xEF, 0xED };
  while (!Ethernet.begin(mac));
  Serial.print("initializeEthernet - OK");
}

void openDoor(byte inside) {
  /*
     inside=0 открытие по ключу
     inside=1 открытие по кнопке изнутри
  */
  int opentime = 0;
  if (inside == 0) {
    if (client.connect(server, 7462)) {
      client.print("GATE,");
      client.print("INSIDE,");
      client.println(uidDec);
      if(!MasterUID)){           // ждем разрешения, если не мастер ключ
        while(client.available() > 0){
          char incomingChar = client.read();
          if(incomingChar == '\n'){
            if(incomingString == "GATE,INSIDE,ALLOW"){
               digitalWrite(RelayPin, 0); // размыкаем релешку
               opentime = millis();
               client.stop();
            } else if(incomingString == "GATE,INSIDE,DENIED"){
               // какое-нибудь действие если не разрешен вход
            }
            incomingString == "";
          } else {
               incomingString += incomingChar;
          }  
        }
      } else {
        digitalWrite(RelayPin, 0); // размыкаем релешку
        opentime = millis();
        client.stop();
      }
    } else {
      Serial.println("connection failed");
      if(MasterUID)){
        digitalWrite(RelayPin, 0); // размыкаем релешку
        opentime = millis();
      }
    }
  } else {
    // если открывают изнутри кнопкой
    opentime = millis();
    digitalWrite(RelayPin, 0); // замыкаем релешку
  }
  
  while ((millis() - opentime) < 5000);
  digitalWrite(RelayPin, 1); // замыкаем релешку
}

bool compareUID(unsigned long UID) {
  unsigned long countUIDs, EEPROMUID;
  EEPROM_read(0, countUIDs);
  for (int i = 1; i <= (countUIDs + 1); i++) {
    EEPROM_read(i, EEPROMUID);
    if (UID == EEPROMUID) {
      return 1;
    }
  }
  return 0;
}

void updUIDs() {
  // В EEPROM храним MASTER ключи
  unsigned long countUIDs, fromEEPROM;
  
    if (client.connect(server, 7364)) {
      client.print("GATE,");
      client.println("UPD");
    }   
    
    if(client.connected()){
       int i=1;
       while(client.available() > 0){
         char incomingChar = client.read();
       
         if(incomingChar == '\n'){
           EEPROM_read(i, fromEEPROM);
           if(fromEEPROM == incomingString.toInt()){
             incomingString == "";
           }else{
             EEPROM_write(i,incomingString.toInt());
             incomingString == "";
           }
         }else{
           incomingString += incomingChar;
         }  
         i++;
       }
       EEPROM_read(0, fromEEPROM);
       if(i != fromEEPROM){
         EEPROM_write(0,i);
       }
         
     }
      client.stop();
      lastAttemptTime = millis();
}

void sendNewUID(unsigned long UID){
    if (client.connect(server, 7462)) {
      client.print("GATE,");
      client.print("NEW,");
      client.println(uidDec);
      client.stop();
    } else {
      Serial.println("connection failed");
    }
}

bool chkBtn(){
  if (digitalRead(btnPin)){
    delay(50);
    if (digitalRead(btnPin)){
    //  do{
    //  }while(digitalRead(btnPin));
      return 1;
    }
  }
  return 0;
}

void setup() {
  Serial.begin(9600);
  initializePins();
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();   // Init MFRC522
  initializeEthernet();
  updUIDs();
}

void loop() {
  
  btnState = chkBtn();
  
  if (mfrc522.PICC_IsNewCardPresent() 
      && mfrc522.PICC_ReadCardSerial()) // Если найдена новая RFID метка и считан UID
  {
    uidDec = 0;
    for (byte i = 0; i < mfrc522.uid.size; i++)
    {
      uidDecTemp = mfrc522.uid.uidByte[i];
      uidDec = uidDec * 256 + uidDecTemp;
    }
    Serial.println("Card UID: ");
    Serial.println(uidDec); // Выводим UID метки в консоль.
    MasterUID = compareUID(uidDec);
    openDoor(0);
    
   if (!MasterUID && btnState){
      sendNewUID(uidDec);
    }
    mfrc522.PICC_HaltA();
  }


  if (btnState) {   // Если нажата кнопка выхода
    openDoor(1);
  }
  
  if(millis()-lastAttemptTime > updateInterval){
    updUIDs();
  }
  
  
}

