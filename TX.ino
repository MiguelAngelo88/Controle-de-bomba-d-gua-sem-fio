/*
  Controle de Bomba d'Água via LoRa - LÓGICA PERSISTENTE
  Garante que o estado da bomba coincida com a chave física.
*/

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_task_wdt.h>

/* ================== CONFIGURAÇÕES ================== */
#define WDT_TIMEOUT    10
#define SWITCH_PIN     13
#define LORA_BAND      915E6
#define ACK_TIMEOUT    1500  // Tempo esperando resposta
#define RETRY_INTERVAL 3000  // Tempo entre tentativas após falha

/* OLED PINS (Heltec V2) */
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RST);

/* LoRa PINS (Heltec V2) */
#define LORA_SS    18
#define LORA_RST   14
#define LORA_DIO0  26

/* PROTOCOLO */
#define STX 0xAA
#define ETX 0x55
enum Cmd : uint8_t { CMD_ON = 0x01, CMD_OFF = 0x02, ACK_ON = 0x81, ACK_OFF = 0x82 };

typedef struct __attribute__((packed)) {
  uint8_t stx;
  uint8_t cmd;
  uint8_t seq;
  uint8_t etx;
} LoraPacket;

/* ESTADO DO SISTEMA */
bool desiredState = HIGH; // HIGH = OFF (Pullup), LOW = ON
bool actualStateConfirmed = false; 
uint8_t globalSeq = 0;
unsigned long lastTxTime = 0;
bool waitingAck = false;

/* ================== FUNÇÕES AUXILIARES ================== */

void updateDisplay(const char* status, bool error = false) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("SISTEMA BOMBA LORA");
  
  display.setCursor(0, 15);
  display.print("CHAVE: ");
  display.println(desiredState == LOW ? "LIGAR (ON)" : "DESLIGAR (OFF)");

  display.setCursor(0, 30);
  display.print("BOMBA: ");
  if (actualStateConfirmed) {
    display.setTextColor(SSD1306_WHITE);
    display.println(desiredState == LOW ? "LIGADA [OK]" : "DESLIGADA [OK]");
  } else {
    display.println("SINCRONIZANDO...");
  }

  display.setCursor(0, 50);
  display.setTextSize(1);
  if (error) display.print("(!) ERRO DE SINAL");
  else if (waitingAck) display.print(">> Enviando comando...");
  
  display.display();
}

void sendCommand() {
  uint8_t cmd = (desiredState == LOW) ? CMD_ON : CMD_OFF;
  LoraPacket pkt = { STX, cmd, globalSeq, ETX };
  
  LoRa.beginPacket();
  LoRa.write((uint8_t*)&pkt, sizeof(pkt));
  LoRa.endPacket();
  
  lastTxTime = millis();
  waitingAck = true;
}

/* ================== SETUP ================== */

void setup() {
  Serial.begin(115200);
  pinMode(SWITCH_PIN, INPUT);
  
  // Inicializa OLED
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW); delay(20); digitalWrite(OLED_RST, HIGH);
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) while(1);
  display.setTextColor(SSD1306_WHITE);

  // Inicializa LoRa
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_BAND)) {
    updateDisplay("ERRO LORA");
    while(1);
  }
  LoRa.setSpreadingFactor(10); // Máxima robustez
  LoRa.enableCrc();

  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);
  
  desiredState = digitalRead(SWITCH_PIN);
  updateDisplay("INICIANDO...");
}

/* ================== LOOP ================== */

void loop() {
  esp_task_wdt_reset();
  
  bool currentSwitch = digitalRead(SWITCH_PIN);

  // Se a chave mudou, resetamos a confirmação e forçamos novo envio
  if (currentSwitch != desiredState) {
    desiredState = currentSwitch;
    actualStateConfirmed = false;
    globalSeq++; // Novo ID de transação
    sendCommand();
  }

  // Se não está confirmado, tenta enviar periodicamente
  if (!actualStateConfirmed) {
    if (!waitingAck) {
      if (millis() - lastTxTime > RETRY_INTERVAL) {
        sendCommand();
      }
    } else {
      // Esperando ACK
      if (LoRa.parsePacket()) {
        LoraPacket ackPkt;
        if (LoRa.available() >= sizeof(ackPkt)) {
          LoRa.readBytes((uint8_t*)&ackPkt, sizeof(ackPkt));
          
          // Valida se o ACK corresponde ao comando atual
          bool matchOn = (desiredState == LOW && ackPkt.cmd == ACK_ON);
          bool matchOff = (desiredState == HIGH && ackPkt.cmd == ACK_OFF);
          
          if (ackPkt.stx == STX && ackPkt.etx == ETX && ackPkt.seq == globalSeq && (matchOn || matchOff)) {
            actualStateConfirmed = true;
            waitingAck = false;
            updateDisplay("CONFIRMADO");
          }
        }
      } else if (millis() - lastTxTime > ACK_TIMEOUT) {
        waitingAck = false; // Timeout, tentará novamente no próximo RETRY_INTERVAL
        updateDisplay("FALHA ACK", true);
      }
    }
  }

  // Atualização periódica do display (opcional, para manter feedback)
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate > 500) {
    updateDisplay(actualStateConfirmed ? "OK" : "SYNCING");
    lastDisplayUpdate = millis();
  }
}
