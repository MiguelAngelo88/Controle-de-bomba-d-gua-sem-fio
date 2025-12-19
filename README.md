# Controle de Bomba d’Água via LoRa

Sistema embarcado para acionamento remoto de uma bomba d’água utilizando comunicação **LoRa ponto-a-ponto**, composto por dois módulos independentes: **Transmissor (TX)** e **Receptor (RX)**.

---

## Visão Geral do Sistema

- **TX (Transmissor)**  
  Operado pelo usuário via chave física.  
  Responsável por enviar comandos de acionamento/desacionamento da bomba.

- **RX (Receptor)**  
  Recebe comandos via LoRa, valida o pacote, aciona a bomba através de relé e retorna confirmação (ACK) ao transmissor.

- **Comunicação**  
  - LoRa ponto-a-ponto  
  - Protocolo próprio  
  - Confirmação por ACK  
  - Timeout e retransmissão  

---

## Hardware Utilizado (Protótipo)

- **Placa:** Heltec LoRa32 V2  
  - MCU: ESP32  
  - Rádio: SX1276  
  - Frequência: 915 MHz (Brasil)  

- **RX**
  - Módulo de relé (isolado com optoacoplador)

- **TX**
  - Botão ou chave de comando
  - LED de status (opcional)

> ⚠️ Protótipo **não possui pareamento** entre TX/RX nesta versão.

---

## Arquitetura

[ Usuário ]
↓
[ TX - ESP32 + LoRa ]
↓ (CMD)

Comunicação
Copiar código
   ↑   (ACK)
[ RX - ESP32 + LoRa ]
   ↓
[ Relé ]
   ↓
[ Bomba d'água ]
```

---

## Máquina de Estados (Resumo)

### TX
- `IDLE`
- `SEND_CMD`
- `WAIT_ACK`
- `SUCCESS`
- `TIMEOUT`
- `ERROR`

### RX
- `IDLE`
- `RX_PACKET`
- `VALIDATE`
- `ACTUATE`
- `SEND_ACK`

Toda a lógica é **não bloqueante** (`millis()`), sem uso de `delay()`.

---

## Protocolo de Comunicação (Resumo)

Pacote simples e determinístico:

```
[STX][CMD][SEQ][CRC][ETX]
```

- **CMD:** ON / OFF  
- **SEQ:** Número de sequência (evita ACK falso)  
- **CRC:** Validação de integridade  
- **ACK obrigatório** para confirmação de acionamento

---

## Como Compilar e Gravar

1. Instalar **Arduino IDE**
2. Adicionar suporte ao **ESP32**
3. Instalar biblioteca LoRa compatível com SX1276
4. Abrir:
  - `firmware/tx/src/main_tx.ino` para o transmissor
  - `firmware/rx/src/main_rx.ino` para o receptor
5. Selecionar placa **Heltec WiFi LoRa 32 (V2)**
6. Compilar e gravar

---

## Status do Projeto

- [x] Comunicação LoRa básica  
- [x] FSM TX/RX  
- [x] ACK de confirmação
- [ ] Watchdog
- [ ] Fail-safe
- [ ] Pareamento   
- [ ] Certificação / Produto final  

---

## Observações Importantes


- Uso em aplicações críticas exige:
 - Watchdog
 - Fail-safe de relé
 - Proteções elétricas
 - Testes de campo extensivos

---
