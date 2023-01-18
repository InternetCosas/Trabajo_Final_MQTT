/*
 * Código del concentrador
 */

#include <SPI.h>             
#include <LoRa.h>
#include <Arduino_PMIC.h>
#include <Regexp.h>

#define TX_LAPSE_MS          10000

// NOTA: Ajustar estas variables 
const uint8_t localAddress = 0xB0;     // Dirección de este dispositivo
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
uint8_t measurement = 0;

uint16_t bright_wait = 10000;
uint16_t bright_measurement = 0;

uint16_t ultrasound_wait = 10000;
uint16_t ultrasound_unit = 1;

// --------------------------------------------------------------------
// Setup function
// --------------------------------------------------------------------
void setup() {
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

// --------------------------------------------------------------------
// Loop function
// --------------------------------------------------------------------
void loop() {

    static uint32_t lastSendTime_ms = 0;
    static uint16_t msgCount = 0;
    static uint32_t txInterval_ms = TX_LAPSE_MS;
    static uint32_t tx_begin_ms = 0;

    if(Serial.available() > 0) {  // Si se encuentra algo que leer 
      String input = Serial.readStringUntil('\n');

      char Buf[50];
      input.toCharArray(Buf, 50);

      MatchState ms;
      ms.Target(Buf);

      char bright_result = ms.Match("^bright [0-9]+"); // Orden para modificar el tiempo entre una medida de luz y la siguiente
      char ultrasound_delay_result = ms.Match("^ultrasound delay [0-9]+"); // Orden para modificar el tiempo entre una medida de ultrasonido y la siguiente
      char ultrasound_unit_result = ms.Match("^ultrasound unit [1-3]"); // Orden para modificar el tiempo entre una medida de ultrasonido y la siguiente

      if (input.equalsIgnoreCase("help") || bright_result != REGEXP_MATCHED || ultrasound_delay_result != REGEXP_MATCHED || ultrasound_unit_result != REGEXP_MATCHED) {
        SerialUSB.println("Se printea la ayuda");
      }
      if (bright_result == REGEXP_MATCHED) {
        destination = 0xB1; // Cambiamos la direccion de destino al sensor de luminosidad
        int spacePositon = input.indexOf(" ");
        bright_wait = (uint8_t)(input.substring(spacePositon).toInt());   // Cambiamos el tiempo entre una medida y otra por el monitor serie y lo pasamos a ms
        SerialUSB.println("\n=============================================================");
        SerialUSB.print("The delay between brightness measurements has been changed to: ");
        SerialUSB.print((int)(bright_wait * 1000));
        SerialUSB.println("ms");
        SerialUSB.println("=============================================================");
        uint8_t payload = bright_wait;
        //sendPayload(lastSendTime_ms, msgCount, txInterval_ms, tx_begin_ms, payload);

        if (!transmitting && ((millis() - lastSendTime_ms) > txInterval_ms)) {
          transmitting = true;
          //txDoneFlag = false;
          txDoneFlag = true;
          tx_begin_ms = millis();
      
          sendMessage(payload, msgCount);
          Serial.print("Sending new brightness measurements delay (");
          Serial.print(msgCount++);
          Serial.print("): ");
          Serial.println(payload);
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
      if (ultrasound_delay_result == REGEXP_MATCHED) {
        destination = 0xB2;  // Cambiamos la direccion de destino al sensor de ultrasonido
        int spacePositon = input.indexOf(" ");
        String aux = input.substring(spacePositon+1);
        int secondSpacePositon = aux.indexOf(" ");
        ultrasound_wait = (uint8_t)(aux.substring(secondSpacePositon).toInt());   // Cambiamos el tiempo entre una medida y otra por el monitor serie y lo pasamos a ms
        SerialUSB.println("\n=============================================================");
        SerialUSB.print("The delay between brightness measurements has been changed to: ");
        SerialUSB.print((int)(ultrasound_wait * 1000));
        SerialUSB.println("ms");
        SerialUSB.println("=============================================================");
        uint8_t payload = ultrasound_wait;
        //sendPayload(lastSendTime_ms, msgCount, txInterval_ms, tx_begin_ms, payload);

        if (!transmitting && ((millis() - lastSendTime_ms) > txInterval_ms)) {
          transmitting = true;
          //txDoneFlag = false;
          txDoneFlag = true;
          tx_begin_ms = millis();
      
          sendMessage(payload, msgCount);
          Serial.print("Sending new ultrasound measurements delay (");
          Serial.print(msgCount++);
          Serial.print("): ");
          Serial.println(payload);
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
      if (ultrasound_unit_result == REGEXP_MATCHED) {
        destination = 0xB2;  // Cambiamos la direccion de destino al sensor de ultrasonido
        int spacePositon = input.indexOf(" ");
        String aux = input.substring(spacePositon+1);
        int secondSpacePositon = aux.indexOf(" ");
        ultrasound_unit = (uint8_t)(aux.substring(secondSpacePositon).toInt());   // Cambiamos el tiempo entre una medida y otra por el monitor serie y lo pasamos a ms
        SerialUSB.println("\n=============================================================");
        SerialUSB.print("The delay between brightness measurements has been changed to: ");
        SerialUSB.print((int)(ultrasound_unit));
        SerialUSB.println("ms");
        SerialUSB.println("=============================================================");
        uint8_t payload = ultrasound_unit;
        //sendPayload(lastSendTime_ms, msgCount, txInterval_ms, tx_begin_ms, payload);

        if (!transmitting && ((millis() - lastSendTime_ms) > txInterval_ms)) {
          transmitting = true;
          //txDoneFlag = false;
          txDoneFlag = true;
          tx_begin_ms = millis();
      
          sendUnitMessage(payload, msgCount, 27);
          Serial.print("Sending new ultrasound measurements delay (");
          Serial.print(msgCount++);
          Serial.print("): ");
          Serial.println(payload);
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
}

// --------------------------------------------------------------------
// Sending message function
// --------------------------------------------------------------------
void sendMessage(uint8_t payload, uint16_t msgCount) {
  while(!LoRa.beginPacket()) {            // Comenzamos el empaquetado del mensaje
    delay(10);                            // 
  }
  LoRa.write(destination);                // Añadimos el ID del destinatario
  LoRa.write(localAddress);               // Añadimos el ID del remitente
  LoRa.write((uint8_t)(msgCount >> 7));   // Añadimos el Id del mensaje (MSB primero)
  LoRa.write((uint8_t)(msgCount & 0xFF)); 
  LoRa.write(payload); // Añadimos el mensaje/payload 
  LoRa.endPacket(true);                   // Finalizamos el paquete, pero no esperamos a
                                          // finalice su transmisión
}

void sendUnitMessage(uint8_t payload, uint16_t msgCount, uint8_t unit_flag) {
  while(!LoRa.beginPacket()) {            // Comenzamos el empaquetado del mensaje
    delay(10);                            // 
  }
  LoRa.write(destination);                // Añadimos el ID del destinatario
  LoRa.write(localAddress);               // Añadimos el ID del remitente
  LoRa.write((uint8_t)(msgCount >> 7));   // Añadimos el Id del mensaje (MSB primero)
  LoRa.write((uint8_t)(msgCount & 0xFF));
  LoRa.write(payload); // Añadimos el mensaje/payload
  LoRa.write(unit_flag); // Añadimos el mensaje/payload 
  LoRa.endPacket(true);                   // Finalizamos el paquete, pero no esperamos a
                                          // finalice su transmisión
}

void sendPayload(uint32_t lastSendTime_ms, uint16_t msgCount, uint32_t txInterval_ms, uint32_t tx_begin_ms, uint8_t payload) {
    
    if (!transmitting && ((millis() - lastSendTime_ms) > txInterval_ms)) {
      transmitting = true;
      txDoneFlag = false;
      tx_begin_ms = millis();
  
      sendMessage(payload, msgCount);
      Serial.print("Sending new brightness measurements delay (");
      Serial.print(msgCount++);
      Serial.print("): ");
      Serial.print(payload);
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
  measurement = LoRa.read(); // En caso de que la luz sea directa (1) se guarda
  uint8_t incomingLength = LoRa.read(); // Longitud en bytes del mensaje
  
  uint8_t receivedBytes = 0;            // Leemos el mensaje byte a byte
  while (LoRa.available() && (receivedBytes < uint8_t(sizeof(buffer)-1))) {            
    buffer[receivedBytes++] = (char)LoRa.read();
  }
  
  if (incomingLength != receivedBytes) {// Verificamos la longitud del mensaje
    Serial.print("Receiving error: declared message length " + String(incomingLength));
    Serial.println(" does not match length " + String(receivedBytes));
    return;                             
  }

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
  Serial.println("Payload length: " + String(incomingLength));
  Serial.print("Payload: ");
  printBinaryPayload(buffer, receivedBytes);
  Serial.print("\nRSSI: " + String(LoRa.packetRssi()));
  Serial.print(" dBm\nSNR: " + String(LoRa.packetSnr()));
  Serial.println(" dB");

  // Mostramos las medidas de cada sensor según sus direcciones
  if (String(sender, HEX).equals("b1")) {
    bright_measurement = *((uint16_t*)buffer);
    Serial.print("Remote brightness measurement: ");
    Serial.println(bright_measurement);
    Serial.print("Remote direct light measurement: ");
    Serial.print(measurement);
    if (measurement == 1) {
      Serial.println(" (direct)");
    } else {
      Serial.println(" (indirect)");
    }
  } else if (String(sender, HEX).equals("b2")) {
    Serial.print("Remote ultrasound measurement: ");
    Serial.print(bright_measurement);
    ultrasound_unit = measurement;
    if (ultrasound_unit == 3) {
      Serial.println(" ms");
    } else if (ultrasound_unit == 2) {
      Serial.println(" inc");
    } else {
      Serial.println(" cm");
    }
  } else {
    Serial.println("Unexpected sender direction: 0x" + String(sender, HEX));
  }
}

void TxFinished() {
  txDoneFlag = true;
}

void printBinaryPayload(uint8_t * payload, uint8_t payloadLength) {
  for (int i = 0; i < payloadLength-1; i++) {
    Serial.print(payload[i], HEX);
    Serial.print(" ");
  }
}
