/*
  Controle de Bomba d'Água via LoRa
  Módulo TX - Heltec WiFi LoRa 32 V2
*/

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* ================== OLED ================== */
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

/* ================== LORA ================== */
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
#define ACK_TIMEOUT  1000

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
  TX_WAIT_ACK,
  TX_SUCCESS,
  TX_TIMEOUT
};

TxState txState = TX_IDLE;
uint8_t seq = 0;
unsigned long t0;

/* ================== FUNÇÕES ================== */

void oledPrint(const char* msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(msg);
  display.display();
}

bool initOLED() {
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    return false;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  oledPrint("OLED OK");
  return true;
}

bool initLoRa() {
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_BAND)) {
    oledPrint("Erro LoRa");
    return false;
  }

  LoRa.setTxPower(LORA_TX_POWER);
  LoRa.enableCrc();
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);

  oledPrint("LoRa OK");
  return true;
}

void sendCommand(uint8_t cmd) {
  LoraPacket pkt = { STX, cmd, seq, ETX };

  LoRa.beginPacket();
  LoRa.write((uint8_t*)&pkt, sizeof(pkt));
  LoRa.endPacket();

  oledPrint("Comando enviado");
}

bool receiveAck() {
  LoraPacket pkt;
  if (LoRa.available() < sizeof(pkt)) return false;

  LoRa.readBytes((uint8_t*)&pkt, sizeof(pkt));

  if (pkt.stx != STX || pkt.etx != ETX) return false;
  if (pkt.seq != seq) return false;

  return (pkt.cmd == ACK_ON || pkt.cmd == ACK_OFF);
}

/* ================== SETUP ================== */

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT);

  if (!initOLED()) {
    Serial.println("Falha OLED");
    while (1);
  }

  while (!initLoRa());
}

/* ================== LOOP ================== */

void loop() {
  switch (txState) {

    case TX_IDLE:
      oledPrint("Aguardando Comando");
      if (digitalRead(BUTTON_PIN)) {
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
      oledPrint("Mensagem recebida");
      delay(500);//remover dps
      txState = TX_IDLE;
      break;

    case TX_TIMEOUT:
      oledPrint("Mensagem nao recebida");
      delay(500);//remover dps
      txState = TX_IDLE;
      break;
  }
}
