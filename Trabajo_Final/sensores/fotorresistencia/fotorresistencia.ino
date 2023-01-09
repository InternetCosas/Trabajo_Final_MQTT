/*
 * Resistencia de 470 ohm
 */

#include <string.h>
#include <SPI.h>             
#include <LoRa.h>
#include <Arduino_PMIC.h>

#define TX_LAPSE_MS          10000

// NOTA: Ajustar estas variables 
const uint8_t localAddress = 0xB1;     // Dirección de este dispositivo
uint8_t destination = 0xFF;            // Dirección de destino, 0xFF es la dirección de broadcast

volatile bool txDoneFlag = true;       // Flag para indicar cuando ha finalizado una transmisión
volatile bool transmitting = false;

// Estructura para almacenar la configuración de la radio
typedef struct {
  uint8_t bandwidth_index;
  uint8_t spreadingFactor;
  uint8_t codingRate;
  uint8_t txPower; 
} LoRaConfig_t;

double bandwidth_kHz[10] = {7.8E3, 10.4E3, 15.6E3, 20.8E3, 31.25E3,
                            41.7E3, 62.5E3, 125E3, 250E3, 500E3 };

LoRaConfig_t thisNodeConf   = { 9, 7, 5, 2};
int remoteRSSI = 0;
float remoteSNR = 0;
static uint16_t msgCount = 0;

const int LDR_Apin = A1;
const int LDR_Dpin = 2;
int wait = 10000;
uint8_t d_read = -1;
uint16_t a_read = -1;


void setup() {
  pinMode(LDR_Apin, INPUT); // Pin por el que leeremos el nivel de luminosidad
  pinMode(LDR_Dpin, INPUT); // Pin por el que leeremos si la luz da al sensor directamente o no

  Serial.begin(115200);  
  while (!Serial); 

  Serial.println("LoRa Duplex with TxDone and Receive callbacks");
  Serial.println("Using binary packets");
  
  // Es posible indicar los pines para CS, reset e IRQ pins (opcional)
  // LoRa.setPins(csPin, resetPin, irqPin);// set CS, reset, IRQ pin

  
  if (!init_PMIC()) {
    Serial.println("Initilization of BQ24195L failed!");
  }
  else {
    Serial.println("Initilization of BQ24195L succeeded!");
  }

  if (!LoRa.begin(868E6)) {      // Initicializa LoRa a 868 MHz
    Serial.println("LoRa init failed. Check your connections.");
    while (true);                
  }

  // Configuramos algunos parámetros de la radio
  LoRa.setSignalBandwidth(long(bandwidth_kHz[thisNodeConf.bandwidth_index])); 
                                  // 7.8E3, 10.4E3, 15.6E3, 20.8E3, 31.25E3
                                  // 41.7E3, 62.5E3, 125E3, 250E3, 500E3 
                                  // Multiplicar por dos el ancho de banda
                                  // supone dividir a la mitad el tiempo de Tx
                                  
  LoRa.setSpreadingFactor(thisNodeConf.spreadingFactor);     
                                  // [6, 12] Aumentar el spreading factor incrementa 
                                  // de forma significativa el tiempo de Tx
                                  // SPF = 6 es un valor especial
                                  // Ver tabla 12 del manual del SEMTECH SX1276
  
  LoRa.setCodingRate4(thisNodeConf.codingRate);         
                                  // [5, 8] 5 da un tiempo de Tx menor
                                  
  LoRa.setTxPower(thisNodeConf.txPower, PA_OUTPUT_PA_BOOST_PIN); 
                                  // Rango [2, 20] en dBm
                                  // Importante seleccionar un valor bajo para pruebas
                                  // a corta distancia y evitar saturar al receptor
  LoRa.setSyncWord(0x12);         // Palabra de sincronización privada por defecto para SX127X 
                                  // Usaremos la palabra de sincronización para crear diferentes
                                  // redes privadas por equipos
  LoRa.setPreambleLength(8);      // Número de símbolos a usar como preámbulo

  
  // Indicamos el callback para cuando se reciba un paquete
  LoRa.onReceive(onReceive);
  
  // Activamos el callback que nos indicará cuando ha finalizado la 
  // transmisión de un mensaje
  LoRa.onTxDone(TxFinished);

  // Nótese que la recepción está activada a partir de este punto
  LoRa.receive();

  Serial.println("LoRa init succeeded.\n");
}


void loop() {

  static uint32_t lastSendTime_ms = 0;
  static uint32_t txInterval_ms = TX_LAPSE_MS;
  static uint32_t tx_begin_ms = 0;

  uint8_t payload[2];
  uint8_t payloadLength = 2;

  d_read = -1;
  a_read = -1;
  if(!transmitting) {
    brightnesMesure();
  }
  memcpy(payload, &a_read, payloadLength);  // Guardamos el valor de luminosidad en un array para mandarlo por LoRa posteriormente al concentrador
  if (d_read != -1 && a_read != -1) {
    //sendPayload(lastSendTime_ms, msgCount, txInterval_ms, tx_begin_ms, payload, payloadLength, d_read);
    if (!transmitting && ((millis() - lastSendTime_ms) > txInterval_ms)) {
        transmitting = true;
        txDoneFlag = false;
        tx_begin_ms = millis();
    
        sendMessage(payload, payloadLength, msgCount, d_read);
        Serial.print("Sending new brightness measurements (");
        Serial.print(msgCount++);
        Serial.print("): ");
        printBinaryPayload(payload, payloadLength);
    }                  

    if (transmitting && txDoneFlag) {
        uint32_t TxTime_ms = millis() - tx_begin_ms;
        Serial.print("----> TX completed in ");
        Serial.print(TxTime_ms);
        Serial.println(" msecs");
        
        // Ajustamos txInterval_ms para respetar un duty cycle del 1% 
        uint32_t lapse_ms = tx_begin_ms - lastSendTime_ms;
        lastSendTime_ms = tx_begin_ms; 
        float duty_cycle = (100.0f * TxTime_ms) / lapse_ms;
        
        Serial.print("Duty cycle: ");
        Serial.print(duty_cycle, 1);
        Serial.println(" %\n");

        // Solo si el ciclo de trabajo es superior al 1% lo ajustamos
        if (duty_cycle > 1.0f) {
        txInterval_ms = TxTime_ms * 100;
        }
        
        transmitting = false;
        
        // Reactivamos la recepción de mensajes, que se desactiva
        // en segundo plano mientras se transmite
        LoRa.receive();   
    }
  }
}

// --------------------------------------------------------------------
// Sending message function
// --------------------------------------------------------------------
void sendMessage(uint8_t* payload, uint8_t payloadLength, uint16_t msgCount, uint8_t d_read) {
  while(!LoRa.beginPacket()) {            // Comenzamos el empaquetado del mensaje
    delay(10);                            // 
  }
  LoRa.write(destination);                // Añadimos el ID del destinatario
  LoRa.write(localAddress);               // Añadimos el ID del remitente
  LoRa.write((uint8_t)(msgCount >> 7));   // Añadimos el Id del mensaje (MSB primero)
  LoRa.write((uint8_t)(msgCount & 0xFF));
  LoRa.write(d_read);                     // Añadimos la información de si hay luz directa (1) o no (0)
  LoRa.write(payloadLength);              // Añadimos la longitud en bytes del mensaje
  LoRa.write(payload, (size_t)payloadLength); // Añadimos el mensaje/payload 
  LoRa.endPacket(true);                   // Finalizamos el paquete, pero no esperamos a
                                          // finalice su transmisión
}

void sendPayload(uint32_t lastSendTime_ms, uint16_t msgCount, uint32_t txInterval_ms, uint32_t tx_begin_ms, uint8_t payload[2], uint8_t payloadLength, uint8_t d_read) {
    
    if (!transmitting && ((millis() - lastSendTime_ms) > txInterval_ms)) {
        transmitting = true;
        txDoneFlag = false;
        tx_begin_ms = millis();
    
        sendMessage(payload, payloadLength, msgCount, d_read);
        Serial.print("Sending new brightness measurements (");
        Serial.print(msgCount++);
        Serial.print("): ");
        printBinaryPayload(payload, payloadLength);
    }                  

    if (transmitting && txDoneFlag) {
        uint32_t TxTime_ms = millis() - tx_begin_ms;
        Serial.print("----> TX completed in ");
        Serial.print(TxTime_ms);
        Serial.println(" msecs");
        
        // Ajustamos txInterval_ms para respetar un duty cycle del 1% 
        uint32_t lapse_ms = tx_begin_ms - lastSendTime_ms;
        lastSendTime_ms = tx_begin_ms; 
        float duty_cycle = (100.0f * TxTime_ms) / lapse_ms;
        
        Serial.print("Duty cycle: ");
        Serial.print(duty_cycle, 1);
        Serial.println(" %\n");

        // Solo si el ciclo de trabajo es superior al 1% lo ajustamos
        if (duty_cycle > 1.0f) {
        txInterval_ms = TxTime_ms * 100;
        }
        
        transmitting = false;
        
        // Reactivamos la recepción de mensajes, que se desactiva
        // en segundo plano mientras se transmite
        LoRa.receive();   
    }
}


// --------------------------------------------------------------------
// Receiving message function
// --------------------------------------------------------------------
void onReceive(int packetSize) {
  if (transmitting && !txDoneFlag) txDoneFlag = true;
  
  if (packetSize == 0) return;          // Si no hay mensajes, retornamos

  // Leemos los primeros bytes del mensaje
  uint8_t buffer[10];                   // Buffer para almacenar el mensaje
  int recipient = LoRa.read();          // Dirección del destinatario
  uint8_t sender = LoRa.read();         // Dirección del remitente
                                        // msg ID (High Byte first)
  uint16_t incomingMsgId = ((uint16_t)LoRa.read() << 7) | 
                            (uint16_t)LoRa.read();
  
  uint8_t incomingConfig = LoRa.read(); // Longitud en bytes del mensaje
  
  uint8_t receivedBytes = 1;            // Leemos el mensaje byte a byte

  // Verificamos si se trata de un mensaje en broadcast o es un mensaje
  // dirigido específicamente a este dispositivo.
  // Nótese que este mecanismo es complementario al uso de la misma
  // SyncWord y solo tiene sentido si hay más de dos receptores activos
  // compartiendo la misma palabra de sincronización
  if ((recipient & localAddress) != localAddress ) {
    Serial.println("Receiving error: This message is not for me.");
    return;
  }

  // Imprimimos los detalles del mensaje recibido
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message ID: " + String(incomingMsgId));
  Serial.print("Payload: ");
  Serial.print(incomingConfig);
  Serial.print(", ");
  Serial.print(receivedBytes);
  Serial.print("\nRSSI: " + String(LoRa.packetRssi()));
  Serial.print(" dBm\nSNR: " + String(LoRa.packetSnr()));
  Serial.println(" dB");

  Serial.print("\n=============================================\n");
  wait = (int)incomingConfig;
  wait = wait * 1000;
  Serial.print("New delay configuration: ");
  Serial.print(wait);
  Serial.println(" ms.");
  Serial.print("=============================================\n");
}

void TxFinished() {
  txDoneFlag = true;
}

void brightnesMesure() {
  delay(wait);
  d_read = digitalRead(LDR_Dpin);  // Si la luz da directamente al sensor da 1, en caso contrario 0
  a_read = analogRead(LDR_Apin);  // Nivel de luminosidad de 0 a 1023
  SerialUSB.print("\nDirect light: ");
  SerialUSB.println(d_read);
  SerialUSB.print("Brightness level: ");
  SerialUSB.println(a_read);
}

void printBinaryPayload(uint8_t * payload, uint8_t payloadLength) {
  for (int i = 0; i < payloadLength-1; i++) {
    Serial.print(payload[i], HEX);
    Serial.print(" ");
  }
  Serial.print("\n");
}
