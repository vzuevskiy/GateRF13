#include <SPI.h>
#include <Ethernet.h>
#include <MFRC522.h>
#include <EEPROM2.h>
#include <EthernetUdp.h>

#define RST_PIN 2
#define SS_PIN 3
#define RelayPin 5  /*Пин замка*/
#define BeepPin 4   /*Пин Пищалки*/
#define ledPin 6    /*Пин Диодика*/
#define btnPin 7    /*Пин кнопки выхода*/

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance
EthernetUDP udp;
EthernetClient client;
IPAddress server(192, 168, 0, 155);
unsigned long uidDec, uidDecTemp;  // для храниения номера метки в десятичном формате
unsigned long lastAttemptTime = 0;
const unsigned long updateInterval = 60000; // интервал обновления мастер клюей мс
bool btnState = 0;                  // Состояние кнопки, 1 когда нажата
bool MasterUID = 0;
String incomingString;


void initializePins() {
  pinMode(RelayPin, OUTPUT);
  pinMode(BeepPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(btnPin, INPUT);
  pinMode(SS_PIN, OUTPUT); // рфид
  pinMode(10, OUTPUT);     // езернет
  digitalWrite(RelayPin, 1);
}

void initializeEthernet() {
  byte mac[] = { 0xDE, 0xAD, 0xBE, 0xFF, 0xEF, 0xED };
  digitalWrite(SS_PIN, HIGH); // откл рфид
  while (!Ethernet.begin(mac));
  Serial.println(Ethernet.localIP());

}

void openDoor(byte inside) {
  /*
     inside=0 открытие по ключу
     inside=1 открытие по кнопке изнутри
  */
  unsigned long opentime = 0;
  if (inside == 0) {
    if (client.connect(server, 7364)) {
      client.print("GATE,");
      client.print("INSIDE,");
      client.println(uidDec);
      if (!MasterUID) {         // ждем разрешения, если не мастер ключ
        while (1) {
          if (client.available()) {
            char incomingChar = client.read();
            if (incomingChar == '\n') {
              Serial.print("openDoor:");
              Serial.println(incomingString);
              if (incomingString == "GATE,INSIDE,ALLOW") {
                digitalWrite(RelayPin, 0); // размыкаем релешку
                opentime = millis();
              } else if (incomingString == "GATE,INSIDE,DENIED") {
                // какое-нибудь действие если не разрешен вход
              }
              incomingString = "";
              break;
            } else {
              incomingString += incomingChar;
            }
          }
        }
      } else {
        digitalWrite(RelayPin, 0); // размыкаем релешку
        opentime = millis();
      }
      client.stop();
    } else {
      Serial.println("connection failed");
      if (MasterUID) {
        digitalWrite(RelayPin, 0); // размыкаем релешку
        opentime = millis();
      }
    }
  } else {
    // если открывают изнутри кнопкой
    opentime = millis();
    digitalWrite(RelayPin, LOW); // замыкаем релешку
  }
  // delay(5000);
  while ((millis() - opentime) < 5000);
  digitalWrite(RelayPin, HIGH); // замыкаем релешку
}

bool compareUID(unsigned long UID) {
  unsigned long countUIDs, EEPROMUID;
  EEPROM_read(0, countUIDs);
  for (int i = 4; i <= (countUIDs); i += 4) {
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
      int i = 0;
      while (1) {
        if (client.available()) {
          char incomingChar = client.read();
          if (incomingChar == '\n') {
            if (incomingString.endsWith("<END>")) {
              incomingString = "";
              break;
            }
            i += 4;
            EEPROM_read(i, fromEEPROM);
            Serial.print("From buffer:\t");
            Serial.println(incomingString.toInt());
            Serial.print("From EEPROM:\t");
            Serial.println(fromEEPROM);
            if (fromEEPROM == incomingString.toInt()) {
              incomingString = "";
            } else {
              EEPROM_write(i, incomingString.toInt());
              Serial.println("new UID written");
              incomingString = "";
            }
            // i += 4;
          } else {
            incomingString += incomingChar;
          }
        }
      }
      client.stop();
      EEPROM_read(0, fromEEPROM);
      Serial.print("UIDs  num:\t");
      Serial.println(i);
      Serial.print("UIDs in EEPROM:\t");
      Serial.println(fromEEPROM);
      if (i != fromEEPROM) {
        EEPROM_write(0, i);
      }
      Serial.println("stop");
    } else {
      Serial.println("UPD err: connection failed.");
    }
    lastAttemptTime = millis();
  }

void sendNewUID(unsigned long UID) {
  if (client.connect(server, 7364)) {
    client.print("GATE,");
    client.print("NEW,");
    client.println(uidDec);
    client.stop();
  } else {
    Serial.println("connection failed");
  }
}

bool chkBtn() {
  if (digitalRead(btnPin)) {
    delay(50);
    if (digitalRead(btnPin)) {
      do {
      } while (digitalRead(btnPin));
      Serial.println("btn 1");
      return 1;
    }
  }
  return 0;
}

void setup() {
  Serial.begin(9600);
  initializePins();
  SPI.begin();      // Init SPI bus
  initializeEthernet();
  digitalWrite(10, HIGH); // откл езернет
  digitalWrite(SS_PIN, LOW); // вкл рфид
  mfrc522.PCD_Init();   // Init MFRC522
  updUIDs();
}

void loop() {

  btnState = chkBtn();

  digitalWrite(10, HIGH); // откл езернет
  digitalWrite(SS_PIN, LOW); // вкл рфид

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
    digitalWrite(10, LOW); // ВКЛ езернет
    digitalWrite(SS_PIN, HIGH); // ОТКЛ рфид
    if (!btnState) {
      MasterUID = compareUID(uidDec);
      openDoor(0);
    }
    if (!MasterUID && btnState) {
      sendNewUID(uidDec);
    }
    digitalWrite(10, HIGH); // откл езернет
    digitalWrite(SS_PIN, LOW); // вкл рфид
    mfrc522.PICC_HaltA();
    return;
  }

  if (btnState) {   // Если нажата кнопка выхода
    openDoor(1);
    return;
  }

  if (millis() - lastAttemptTime > updateInterval) {
    updUIDs();
  }
}

