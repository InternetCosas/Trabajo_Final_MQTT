/*
 * Resistencia de 470 ohm
 */

#include <string.h>

const int LDR_Apin = A1;
const int LDR_Dpin = 2;
int wait = 10000;


void setup() {
  pinMode(LDR_Apin, INPUT); // Pin por el que leeremos el nivel de luminosidad
  pinMode(LDR_Dpin, INPUT); // Pin por el que leeremos si la luz da al sensor directamente o no
}


void loop() {

  if(Serial.available() > 0) {  // Si se encuentra algo que leer 
    wait = (Serial.readStringUntil('\n').toInt()) * 1000;   // Cambiamos el tiempo entre una medida y otra por el monitor serie y lo pasamos a ms
    SerialUSB.println("\n=============================================================");
    SerialUSB.print("The delay between measurements has been changed to: ");
    SerialUSB.print(wait);
    SerialUSB.println("ms");
    SerialUSB.println("=============================================================");
  }

  uint8_t payload[2];
  uint8_t payloadLength = 2;

  uint8_t d_read = digitalRead(LDR_Dpin);  // Si la luz da directamente al sensor da 1, en caso contrario 0
  uint16_t a_read = analogRead(LDR_Apin);  // Nivel de luminosidad de 0 a 1023
  SerialUSB.print("\nDirect light: ");
  SerialUSB.println(d_read);
  SerialUSB.print("Brightness level: ");
  SerialUSB.println(a_read);
  
  memcpy(payload, &a_read, payloadLength);  // Guardamos el valor de luminosidad en un array para mandarlo por LoRa posteriormente al concentrador
  Serial.print("Sended measurements: ");
  printBinaryPayload(payload, payloadLength+1);

  delay(wait);
}

void printBinaryPayload(uint8_t * payload, uint8_t payloadLength) {
  for (int i = 0; i < payloadLength-1; i++) {
    Serial.print(payload[i], HEX);
    Serial.print(" ");
  }
  Serial.print("\n");
}
