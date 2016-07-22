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
int btnState = 0;                  // Состояние кнопки, 1 когда нажата

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
  digitalWrite(RelayPin, 0);
  int opentime = millis();
  if (inside == 0) {
    if (client.connect(server, 7462)) {
      client.print("GATE,");
      client.print("INSIDE,");
      client.println(uidDec);
      client.stop();
    } else {
      Serial.println("connection failed");
    }
  }
  while ((millis() - opentime) < 5000);
  digitalWrite(RelayPin, 1);
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
  /*
     Если в EEPROM пусто - подгрузить ключи из БД

  */
  unsigned long countUIDs;
  EEPROM_read(0, countUIDs);
  if (countUIDs == 0) {
    if (client.connect(server, 7364)) {
      client.print("GATE,");
      client.println("UPD");
      client.stop();
    } else {
      Serial.println("connection failed");
    }
  }

}

void setup() {
  Serial.begin(9600);
  initializePins();
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();   // Init MFRC522
  initializeEthernet();
  

}

void loop() {

  if (mfrc522.PICC_IsNewCardPresent() 
      && mfrc522.PICC_ReadCardSerial()) // Если найдена новая RFID метка и считан UID
  {
    uidDec = 0;
    // Выдача серийного номера метки.
    for (byte i = 0; i < mfrc522.uid.size; i++)
    {
      uidDecTemp = mfrc522.uid.uidByte[i];
      uidDec = uidDec * 256 + uidDecTemp;
    }
    Serial.println("Card UID: ");
    Serial.println(uidDec); // Выводим UID метки в консоль.
    if (compareUID(uidDec)) // Сравниваем Uid метки, если он равен заданному то открываем.
    {
      openDoor(0);
    }
    mfrc522.PICC_HaltA();
    // return;
  }

  btnState = digitalRead(btnPin);
  if (btnState) {   // Если нажата кнопка выхода
    openDoor(1);
    // return;
  }
  
  if
  
}
