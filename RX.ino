/*
  Controle de Bomba d'Água via LoRa
  Módulo RX - Receptor
  Hardware: Heltec WiFi LoRa 32 (V2)
*/

#include <SPI.h>
#include <LoRa.h>

/* ================== PINOS HELTEC LORA32 V2 ================== */
#define LORA_SCK   5
#define LORA_MISO  19
#define LORA_MOSI  27
#define LORA_SS    18
#define LORA_RST   14
#define LORA_DIO0  26

#define LORA_BAND      915E6
#define LORA_TX_POWER  15

/* ================== APLICAÇÃO ================== */
#define RELAY_PIN 13

#define STX 0xAA
#define ETX 0x55

enum Cmd : uint8_t {
  CMD_ON  = 0x01,
  CMD_OFF = 0x02,
  ACK_ON  = 0x81,
  ACK_OFF = 0x82
};

typedef struct __attribute__((packed)) {
  uint8_t stx;
  uint8_t cmd;
  uint8_t seq;
  uint8_t etx;
} LoraPacket;

enum RxState {
  RX_IDLE,
  RX_VALIDATE,
  RX_ACTUATE,
  RX_SEND_ACK
};

RxState rxState = RX_IDLE;
LoraPacket rxPkt;

/* ================== FUNÇÕES ================== */

bool initLoRa() {
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("[RX] Falha ao iniciar LoRa");
    return false;
  }

  LoRa.setTxPower(LORA_TX_POWER);
  LoRa.enableCrc();
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);

  Serial.println("[RX] LoRa inicializado");
  return true;
}

void sendAck(uint8_t cmd, uint8_t seq) {
  LoraPacket ack = {
    STX,
    (cmd == CMD_ON) ? ACK_ON : ACK_OFF,
    seq,
    ETX
  };

  LoRa.beginPacket();
  LoRa.write((uint8_t*)&ack, sizeof(ack));
  LoRa.endPacket();

  Serial.println("[RX] ACK enviado");
}

/* ================== SETUP ================== */

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  while (!initLoRa());
}

/* ================== LOOP ================== */

void loop() {

  switch (rxState) {

    case RX_IDLE:
      if (LoRa.parsePacket()) {
        if (LoRa.available() >= sizeof(rxPkt)) {
          LoRa.readBytes((uint8_t*)&rxPkt, sizeof(rxPkt));
          rxState = RX_VALIDATE;
        }
      }
      break;

    case RX_VALIDATE:
      if (rxPkt.stx == STX && rxPkt.etx == ETX) {
        rxState = RX_ACTUATE;
      } else {
        rxState = RX_IDLE;
      }
      break;

    case RX_ACTUATE:
      if (rxPkt.cmd == CMD_ON) {
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("[RX] Bomba LIGADA");
      } 
      else if (rxPkt.cmd == CMD_OFF) {
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("[RX] Bomba DESLIGADA");
      }
      rxState = RX_SEND_ACK;
      break;

    case RX_SEND_ACK:
      sendAck(rxPkt.cmd, rxPkt.seq);
      rxState = RX_IDLE;
      break;
  }
}
