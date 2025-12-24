/*
  Controle de Bomba d'Água via LoRa - MÓDULO RECEPTOR (RX)
  HARDWARE: Heltec WiFi LoRa 32 (V3) - Chip SX1262
*/

#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"

/* ================== CONFIGURAÇÕES ================== */
#define RELAY_PIN           48
#define RF_FREQUENCY        915000000
#define TX_OUTPUT_POWER     14
#define LORA_BANDWIDTH      0
#define LORA_SPREADING_FACTOR 10
#define LORA_CODINGRATE     1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define RX_TIMEOUT_VALUE    1000

/* PROTOCOLO */
#define STX 0xAA
#define ETX 0x55

enum Cmd : uint8_t {
  CMD_ON   = 0x01,
  CMD_OFF  = 0x02,
  ACK_ON  = 0x81,
  ACK_OFF = 0x82
};

typedef struct __attribute__((packed)) {
  uint8_t stx;
  uint8_t cmd;
  uint8_t seq;
  uint8_t etx;
} LoraPacket;

/* ESTADO */
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
static RadioEvents_t RadioEvents;

bool lora_idle = true;
unsigned long lastCommandTime = 0;

#define FAILSAFE_TIMEOUT 1200000  // 20 minutos

/* ================== FUNÇÕES ================== */

void atualizaDisplay(const char* status, int rssi = 0) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "RECEPTOR BOMBA V3");
  display.drawString(0, 15, String("Status: ") + status);

  bool bombaLigada = (digitalRead(RELAY_PIN) == HIGH);
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 30, bombaLigada ? "BOMBA: LIGADA" : "BOMBA: DESLIGADA");

  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 50, String("RSSI: ") + rssi + " dBm");
  display.display();
}

void dumpPacket(LoraPacket* pkt) {
  Serial.print("[RX] STX: 0x"); Serial.print(pkt->stx, HEX);
  Serial.print(" CMD: 0x"); Serial.print(pkt->cmd, HEX);
  Serial.print(" SEQ: "); Serial.print(pkt->seq);
  Serial.print(" ETX: 0x"); Serial.println(pkt->etx, HEX);
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  Serial.println("\n[LoRa] RX DONE");
  Serial.print("[LoRa] Size: "); Serial.println(size);
  Serial.print("[LoRa] RSSI: "); Serial.print(rssi);
  Serial.print(" dBm | SNR: "); Serial.println(snr);

  if (size != sizeof(LoraPacket)) {
    Serial.println("[ERRO] Tamanho de pacote invalido");
    Radio.Sleep();
    lora_idle = true;
    return;
  }

  LoraPacket* pkt = (LoraPacket*)payload;
  dumpPacket(pkt);

  if (pkt->stx != STX || pkt->etx != ETX) {
    Serial.println("[ERRO] STX/ETX invalido");
    Radio.Sleep();
    lora_idle = true;
    return;
  }

  lastCommandTime = millis();
  uint8_t ackCmd = 0;

  if (pkt->cmd == CMD_ON) {
    Serial.println("[CMD] LIGAR BOMBA");
    digitalWrite(RELAY_PIN, HIGH);
    ackCmd = ACK_ON;

  } else if (pkt->cmd == CMD_OFF) {
    Serial.println("[CMD] DESLIGAR BOMBA");
    digitalWrite(RELAY_PIN, LOW);
    ackCmd = ACK_OFF;

  } else {
    Serial.println("[ERRO] CMD desconhecido");
  }

  if (ackCmd != 0) {
    delay(50);
    LoraPacket ackPkt = { STX, ackCmd, pkt->seq, ETX };
    Serial.print("[TX] Enviando ACK 0x");
    Serial.println(ackCmd, HEX);
    Radio.Send((uint8_t*)&ackPkt, sizeof(ackPkt));
  }

  atualizaDisplay("Comando OK", rssi);

  Radio.Sleep();
  lora_idle = true;
}

void OnTxDone() {
  Serial.println("[LoRa] TX DONE (ACK enviado)");
  Radio.Sleep();
  lora_idle = true;
}

void OnTxTimeout() {
  Serial.println("[ERRO] TX TIMEOUT");
  Radio.Sleep();
  lora_idle = true;
}

/* ================== SETUP ================== */

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==== RX BOMBA LoRa V3 ====");
  Serial.println("[BOOT] Inicializando MCU");

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  display.init();
  atualizaDisplay("Iniciando...");

  RadioEvents.RxDone = OnRxDone;
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;

  Serial.println("[LoRa] Init");
  Radio.Init(&RadioEvents);

  Serial.println("[LoRa] Configurando RX/TX");
  Radio.SetChannel(RF_FREQUENCY);

  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);

  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  lastCommandTime = millis();
  Serial.println("[LoRa] Pronto. Aguardando pacotes...");
  atualizaDisplay("Aguardando...");
}

/* ================== LOOP ================== */

void loop() {
  if (lora_idle) {
    lora_idle = false;
    Serial.println("[LoRa] Entrando em RX");
    Radio.Rx(0);
  }

  Radio.IrqProcess();

  if (digitalRead(RELAY_PIN) == HIGH &&
      (millis() - lastCommandTime > FAILSAFE_TIMEOUT)) {

    Serial.println("[FAIL-SAFE] Timeout sem comando. Desligando bomba.");
    digitalWrite(RELAY_PIN, LOW);
    atualizaDisplay("FAIL-SAFE!");
  }

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 2000) {
    Serial.println("[STATUS] Ouvindo...");
    atualizaDisplay("Ouvindo...");
    lastUpdate = millis();
  }
}
