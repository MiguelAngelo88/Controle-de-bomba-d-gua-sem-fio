#include <SPI.h>
#include <LoRa.h>

/* ================= CONFIG ================= */
#define LORA_FREQ     915E6
#define DEVICE_ID     0x01

#define CMD_OFF       0
#define CMD_ON        1

#define PIN_RELAY     27
#define RX_TIMEOUT_MS 3000
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

/* ================= FSM RX ================= */
typedef enum {
  RX_INIT,
  RX_WAIT_CMD,
  RX_EXEC_CMD,
  RX_SEND_STATUS
} rx_state_t;

rx_state_t rx_state = RX_INIT;

/* ================= VAR ================= */
CmdPacket last_cmd;
uint16_t last_seq_rx = 0;
unsigned long last_rx_time = 0;
/* ======================================== */

void setup() {
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);

  LoRa.begin(LORA_FREQ);
}

void rx_fsm_run() {

  switch (rx_state) {

    case RX_INIT:
      digitalWrite(PIN_RELAY, LOW);
      rx_state = RX_WAIT_CMD;
      break;

    case RX_WAIT_CMD:
      if (LoRa.parsePacket() == sizeof(CmdPacket)) {
        CmdPacket pkt;
        LoRa.readBytes((uint8_t*)&pkt, sizeof(pkt));

        uint8_t crc_calc = crc8((uint8_t*)&pkt, sizeof(CmdPacket) - 1);

        if (pkt.crc == crc_calc &&
            pkt.device_id == DEVICE_ID &&
            pkt.seq > last_seq_rx) {

          last_seq_rx = pkt.seq;
          last_cmd = pkt;
          last_rx_time = millis();
          rx_state = RX_EXEC_CMD;
        }
      }

      if (millis() - last_rx_time > RX_TIMEOUT_MS)
        digitalWrite(PIN_RELAY, LOW);
      break;

    case RX_EXEC_CMD:
      digitalWrite(PIN_RELAY, (last_cmd.cmd == CMD_ON));
      rx_state = RX_SEND_STATUS;
      break;

    case RX_SEND_STATUS: {
      StatusPacket st;
      st.device_id = DEVICE_ID;
      st.estado_bomba = last_cmd.cmd;
      st.falha = 0;
      st.seq = last_cmd.seq;
      st.crc = crc8((uint8_t*)&st, sizeof(StatusPacket) - 1);

      LoRa.beginPacket();
      LoRa.write((uint8_t*)&st, sizeof(st));
      LoRa.endPacket();

      rx_state = RX_WAIT_CMD;
    } break;
  }
}

void loop() {
  rx_fsm_run();
}
