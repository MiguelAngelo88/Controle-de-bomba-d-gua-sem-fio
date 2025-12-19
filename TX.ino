#include <SPI.h>
#include <LoRa.h>

/* ================= CONFIG ================= */
#define LORA_FREQ      915E6
#define DEVICE_ID      0x01

#define CMD_OFF        0
#define CMD_ON         1

#define PIN_KEY        12
#define LED_GREEN      25
#define LED_RED        26

#define TX_PERIOD_MS   200
#define ACK_TIMEOUT_MS 800
/* ========================================== */

/* ================= CRC-8 ================= */
uint8_t crc8(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0x00;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x07;
      else
        crc <<= 1;
    }
  }
  return crc;
}
/* ========================================= */

struct CmdPacket {
  uint8_t device_id;
  uint8_t cmd;
  uint16_t seq;
  uint8_t crc;
};

struct StatusPacket {
  uint8_t device_id;
  uint8_t estado_bomba;
  uint8_t falha;
  uint16_t seq;
  uint8_t crc;
};

/* ================= FSM TX ================= */
typedef enum {
  TX_INIT,
  TX_IDLE_OFF,
  TX_IDLE_ON,
  TX_SEND_CMD,
  TX_WAIT_ACK,
  TX_CONFIRMED,
  TX_ERROR
} tx_state_t;

tx_state_t tx_state = TX_INIT;

/* ================= VAR ================= */
uint8_t cmd_desejado = CMD_OFF;
uint16_t seq_tx = 0;

unsigned long last_tx_time = 0;
unsigned long ack_start_time = 0;
/* ======================================== */

void setup() {
  pinMode(PIN_KEY, INPUT_PULLUP);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, HIGH);

  LoRa.begin(LORA_FREQ);
}

void tx_fsm_run() {

  cmd_desejado = (digitalRead(PIN_KEY) == LOW) ? CMD_ON : CMD_OFF;

  switch (tx_state) {

    case TX_INIT:
      tx_state = (cmd_desejado == CMD_ON) ? TX_IDLE_ON : TX_IDLE_OFF;
      break;

    case TX_IDLE_OFF:
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_RED, HIGH);

      if (cmd_desejado == CMD_ON)
        tx_state = TX_IDLE_ON;
      else if (millis() - last_tx_time > TX_PERIOD_MS)
        tx_state = TX_SEND_CMD;
      break;

    case TX_IDLE_ON:
      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(LED_RED, LOW);

      if (cmd_desejado == CMD_OFF)
        tx_state = TX_IDLE_OFF;
      else if (millis() - last_tx_time > TX_PERIOD_MS)
        tx_state = TX_SEND_CMD;
      break;

    case TX_SEND_CMD: {
      CmdPacket pkt;
      pkt.device_id = DEVICE_ID;
      pkt.cmd = cmd_desejado;
      pkt.seq = ++seq_tx;
      pkt.crc = crc8((uint8_t*)&pkt, sizeof(CmdPacket) - 1);

      LoRa.beginPacket();
      LoRa.write((uint8_t*)&pkt, sizeof(pkt));
      LoRa.endPacket();

      ack_start_time = millis();
      last_tx_time = millis();
      tx_state = TX_WAIT_ACK;
    } break;

    case TX_WAIT_ACK:
      if (LoRa.parsePacket() == sizeof(StatusPacket)) {
        StatusPacket st;
        LoRa.readBytes((uint8_t*)&st, sizeof(st));

        uint8_t crc_calc = crc8((uint8_t*)&st, sizeof(StatusPacket) - 1);

        if (st.crc == crc_calc &&
            st.device_id == DEVICE_ID &&
            st.seq == seq_tx &&
            st.falha == 0 &&
            st.estado_bomba == cmd_desejado) {
          tx_state = TX_CONFIRMED;
        } else {
          tx_state = TX_ERROR;
        }
      }

      if (millis() - ack_start_time > ACK_TIMEOUT_MS)
        tx_state = TX_ERROR;
      break;

    case TX_CONFIRMED:
      if (millis() - last_tx_time > TX_PERIOD_MS)
        tx_state = TX_SEND_CMD;

      if (cmd_desejado == CMD_ON && digitalRead(PIN_KEY) != LOW)
        tx_state = TX_IDLE_OFF;
      if (cmd_desejado == CMD_OFF && digitalRead(PIN_KEY) == LOW)
        tx_state = TX_IDLE_ON;
      break;

    case TX_ERROR:
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_RED, millis() % 300 < 150);
      tx_state = TX_IDLE_OFF;
      break;
  }
}

void loop() {
  tx_fsm_run();
}
