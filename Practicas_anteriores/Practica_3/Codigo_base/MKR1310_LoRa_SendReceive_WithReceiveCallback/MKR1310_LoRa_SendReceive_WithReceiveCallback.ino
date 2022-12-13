/* ---------------------------------------------------------------------
 *  Ejemplo MKR1310_LoRa_SendReceive_WithCallbacks
 *  Práctica 3
 *  Asignatura (GII-IoT)
 *  
 *  Basado en el ejemplo LoRaDuplexCallback de la librería 
 *  LoRa demuestra cómo es posible resolver la recepción  
 *  de mensajes de forma asíncrona.
 *  
 *  Este ejemplo requiere de una versión modificada
 *  de la librería Arduino LoRa (descargable desde 
 *  CV de la asignatura.
 *  
 *  También usa la librería Arduino_BQ24195 
 *  https://github.com/arduino-libraries/Arduino_BQ24195
 * ---------------------------------------------------------------------
 */

#include <SPI.h>             
#include <LoRa.h>
#include <Arduino_PMIC.h>

#define TX_LAPSE_MS          10000

// NOTA: Ajustar estas variables 
const uint8_t localAddress = 0xB0;     // Dirección de este dispositivo
uint8_t destination = 0xFF;            // Dirección de destino, 0xFF es la dirección de broadcast


// --------------------------------------------------------------------
// Setup function
// --------------------------------------------------------------------
void setup() 
{
  Serial.begin(9600);  
  while (!Serial); 

  Serial.println("LoRa Duplex with callback");

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
  LoRa.setSignalBandwidth(125E3); // 7.8E3, 10.4E3, 15.6E3, 20.8E3, 31.25E3
                                  // 41.7E3, 62.5E3, 125E3, 250E3, 500E3 
                                  // Multiplicar por dos el ancho de banda
                                  // supone dividir a la mitad el tiempo de Tx
  LoRa.setSpreadingFactor(7);     // [6, 12] Aumentar el spreading factor incrementa 
                                  // de forma significativa el tiempo de Tx
                                  // SPF = 6 es un valor especial
                                  // Ver tabla 12 del manual del SEMTECH SX1276
  LoRa.setSyncWord(0x12);         // Palabra de sincronización privada por defecto para SX127X 
                                  // Usaremos la palabra de sincronización para crear diferentes
                                  // redes privadas por equipos
  LoRa.setCodingRate4(5);         // [5, 8] 5 da un tiempo de Tx menor
  LoRa.setPreambleLength(8);      // Número de símbolos a usar como preámbulo

  LoRa.setTxPower(3, PA_OUTPUT_PA_BOOST_PIN); // Rango [2, 20] en dBm
                                  // Importante seleccionar un valor bajo para pruebas
                                  // a corta distancia y evitar saturar al receptor

  // Indicamos el callback para cuando se reciba un paquete
  LoRa.onReceive(onReceive);

  // Nótese que la recepción está activada a partir de este punto
  LoRa.receive();
  
  Serial.println("LoRa init succeeded\n");
}

// --------------------------------------------------------------------
// Loop function
// --------------------------------------------------------------------
void loop() 
{
  static uint32_t lastSendTime_ms = 0;
  static uint16_t msgCount = 0;
  static uint32_t txInterval_ms = TX_LAPSE_MS;
  
  if ((millis() - lastSendTime_ms) > txInterval_ms) {
    char message[50];
    

    snprintf(message, sizeof(message),"Message no. %03d from 0x%02X", 
             msgCount, localAddress);
    
    uint32_t beginTX_ms = millis();         
    sendMessage(message, uint8_t(strlen(message)), msgCount);
    uint32_t endTX_ms = millis();
    Serial.print("Sent message ' ");
    Serial.print(message);
    Serial.print("' in ");
    Serial.print(endTX_ms - beginTX_ms);
    Serial.println(" msecs\n");
    
    lastSendTime_ms = millis();            
    txInterval_ms = random(TX_LAPSE_MS) + 1000; 

    // Reactivamos la recepción de mensajes, que se desactiva
    // en segundo plano mientras se transmite
    LoRa.receive();                     
  }
}

// --------------------------------------------------------------------
// Sending message function
// --------------------------------------------------------------------
void sendMessage(char* outgoing, uint8_t msgLength, uint16_t &msgCount) 
{
  LoRa.beginPacket();                     // Comenzamos el empaquetado del mensaje
  LoRa.write(destination);                // Añadimos el ID del destinatario
  LoRa.write(localAddress);               // Añadimos el ID del remitente
  LoRa.write((uint8_t)(msgCount >> 7));   // Añadimos el Id del mensaje (MSB primero)
  LoRa.write((uint8_t)(msgCount & 0xFF)); 
  LoRa.write(msgLength);                  // Añadimos la longitud en bytes del mensaje
  LoRa.print(outgoing);                   // Añadimos el mensaje/payload
  
  // OJO: endPacket(), equivalente a endPacket(false) no vuelve hasta que 
  // la transmisión ha concluido. Por contra, endPacket(true) retorna inmediatamente
  LoRa.endPacket();                       // Finalizamos el paquete y lo enviamos
  msgCount++;                             // Incrementamos el contador de mensajes
}

// --------------------------------------------------------------------
// Receiving message function
// --------------------------------------------------------------------
void onReceive(int packetSize) 
{
  if (packetSize == 0) return;          // Si no hay mensajes, retornamos

  // Leemos los primeros bytes del mensaje
  char buffer[50];                      // Buffer para almacenar el mensaje
  int recipient = LoRa.read();          // Dirección del destinatario
  uint8_t sender = LoRa.read();         // Dirección del remitente
                                        // msg ID (High Byte first)
  uint16_t incomingMsgId = ((uint16_t)LoRa.read() << 7) | 
                            (uint16_t)LoRa.read();
  
  uint8_t incomingLength = LoRa.read(); // Longitud en bytes del mensaje
  
  uint8_t receivedBytes = 0;            // Leemos el mensaje byte a byte
  while (LoRa.available() && (receivedBytes < uint8_t(sizeof(buffer)-1))) {            
    buffer[receivedBytes++] = (char)LoRa.read();
  }
  buffer[receivedBytes] = '\0';         // Terminamos la cadena

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
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + String(buffer));
  Serial.print("RSSI: " + String(LoRa.packetRssi()));
  Serial.println(" dBm\nSNR: " + String(LoRa.packetSnr()));
  Serial.println();
}
