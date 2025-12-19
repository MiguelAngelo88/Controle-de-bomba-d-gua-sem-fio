/*
  Controle de Bomba d'Água via LoRa
  Módulo TX - Transmissor
  MCU: Heltec WiFi LoRa 32 (V2)
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
#define BUTTON_PIN   13
#define ACK_TIMEOUT  1000  // ms

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

enum TxState {
  TX_IDLE,
  TX_SEND,
  TX_WAIT_ACK,
  TX_SUCCESS,
  TX_TIMEOUT
};

TxState txState = TX_IDLE;

uint8_t seq = 0;
unsigned long t0;

/* ================== FUNÇÕES ================== */

bool initLoRa() {
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("[TX] Falha ao iniciar LoRa");
    return false;
  }

  LoRa.setTxPower(LORA_TX_POWER);
  LoRa.enableCrc();
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);

  Serial.println("[TX] LoRa inicializado");
  return true;
}

void sendCommand(uint8_t cmd) {
  LoraPacket pkt = { STX, cmd, seq, ETX };

  LoRa.beginPacket();
  LoRa.write((uint8_t*)&pkt, sizeof(pkt));
  LoRa.endPacket();

  Serial.println("[TX] Comando enviado");
}

bool receiveAck() {
  LoraPacket pkt;
  if (LoRa.available() < sizeof(pkt)) return false;

  LoRa.readBytes((uint8_t*)&pkt, sizeof(pkt));

  if (pkt.stx != STX || pkt.etx != ETX) return false;
  if (pkt.seq != seq) return false;

  if (pkt.cmd == ACK_ON || pkt.cmd == ACK_OFF) {
    return true;
  }

  return false;
}

/* ================== SETUP ================== */

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT);

  while (!initLoRa());
}

/* ================== LOOP ================== */

void loop() {

  switch (txState) {

    case TX_IDLE:
      if (digitalRead(BUTTON_PIN) == HIGH) {
        seq++;
        sendCommand(CMD_ON);
        t0 = millis();
        txState = TX_WAIT_ACK;
      }
      break;

    case TX_WAIT_ACK:
      if (LoRa.parsePacket()) {
        if (receiveAck()) {
          txState = TX_SUCCESS;
        }
      } else if (millis() - t0 > ACK_TIMEOUT) {
        txState = TX_TIMEOUT;
      }
      break;

    case TX_SUCCESS:
      Serial.println("[TX] ACK recebido. Bomba acionada.");
      txState = TX_IDLE;
      break;

    case TX_TIMEOUT:
      Serial.println("[TX] Timeout. Sem ACK.");
      txState = TX_IDLE;
      break;
  }
}
