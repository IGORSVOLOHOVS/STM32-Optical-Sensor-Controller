/*
 * Arduino Uart Tester for OpticManager
 * Hardware: Arduino Uno
 * Connections:
 *   Arduino TX (Pin 11) -> STM32 RX (PC5/USART1_RX)
 *   Arduino RX (Pin 10) -> STM32 TX (PC4/USART1_TX)
 *   GND -> GND
 * 
 * Note: We use SoftwareSerial on Pins 10 (RX) and 11 (TX) to talk to STM32,
 * and keep Hardware Serial for PC debugging.
 */

#include <SoftwareSerial.h>

// RX, TX
SoftwareSerial stmSerial(10, 11); 

void setup() {
  // PC Console
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("OpticManager UART Tester Started");

  // STM32 Connection
  stmSerial.begin(115200);
}

void sendCommand(uint8_t cmd, bool isWrite, uint16_t data = 0) {
  uint8_t packet[8] = {0};
  
  // Byte 0: Bit 0 = R/W, Bits 1-7 = Command
  packet[0] = (cmd << 1) | (isWrite ? 1 : 0);
  
  // Little Endian Data
  packet[1] = data & 0xFF;
  packet[2] = (data >> 8) & 0xFF;
  
  // Send
  stmSerial.write(packet, 8);
  
  Serial.print("Sent Cmd: 0x");
  Serial.print(cmd, HEX);
  Serial.print(" Mode: ");
  Serial.println(isWrite ? "WRITE" : "READ");
}

void readResponse() {
  unsigned long startTime = millis();
  while (stmSerial.available() < 8 && millis() - startTime < 1000) {
    // Wait for 8 bytes or timeout
  }

  if (stmSerial.available() >= 8) {
    uint8_t buffer[8];
    stmSerial.readBytes(buffer, 8);
    
    Serial.print("Received: ");
    for (int i = 0; i < 8; i++) {
      if (buffer[i] < 0x10) Serial.print("0");
      Serial.print(buffer[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  } else {
    Serial.println("Timeout waiting for response");
  }
  Serial.println("-------------------");
}

void loop() {
  // 1. Read ADC Values (Cmd 0x00)
  sendCommand(0x00, false);
  readResponse();
  delay(1000);

  // 2. Read Sensor States (Cmd 0x04)
  sendCommand(0x04, false);
  readResponse();
  delay(1000);

  // 3. Set Transparent Threshold (Cmd 0x01, Write 2000)
  sendCommand(0x01, true, 2000);
  readResponse();
  delay(1000);

  // 4. Read Transparent Threshold (Cmd 0x01, Read)
  sendCommand(0x01, false);
  readResponse();
  delay(2000);
}
