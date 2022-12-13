/* ---------------------------------------------------------------------
 *  
 *  Entrega Práctica 3
 *  Fernando Sanfiel Reyes
 *  Sebastián Fernández García
 *  Santiago Adrián Yánez Martín
 *  Tinizara María Rodríguez Delgado
 *  Asignatura (GII-IoT)
 *  
 *  Basado en el ejemplo MKR1310_LoRa_SendReceive_WithCallbacks,
 *  muestra cómo es posible comunicar los parámetros de 
 *  configuración del transceiver entre nodos LoRa en
 *  formato binario *  
 *  
 *  Este ejemplo requiere de una versión modificada
 *  de la librería Arduino LoRa (descargable desde 
 *  CV de la asignatura)
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
const uint8_t localAddress = 0x80;     // Dirección de este dispositivo
uint8_t destination = 0xFF;            // Dirección de destino, 0xFF es la dirección de broadcast

volatile bool txDoneFlag = true;       // Flag para indicar cuando ha finalizado una transmisión
volatile bool transmitting = false;
bool firstConf = true;
bool opt = false;

bool opt_band = false;    // Flag de control de optimizacion de Ancho de banda
bool opt_spread = false;  // Flag de control de optimizacion de Factor de dispersion 
bool opt_coding = false;  // Flag de control de optimizacion de Tasa de codificacion
bool opt_power = false;   // Flag de control de optimizacion de Potencia
bool ack = true; // Flag de control de comunicacion

static uint32_t lastSendTime_ms = 0;
static uint16_t msgCount = 0;
static uint32_t txInterval_ms = TX_LAPSE_MS;
static uint32_t tx_begin_ms = 0;

int actualRemoteRSSI = 0;
float actualRemoteSNR = 0;
uint32_t actualTxTime_ms = 0;
int pastRemoteRSSI = 0;
float pastRemoteSNR = 0;
uint32_t pastTxTime_ms = 0;

// Estructura para almacenar la configuración de la radio
// Prioridad de aumento de valores
// 1: Ancho de Banda
// 2: Factor de Dispersión
// 3: Tasa de códificación
// 4: Potencia
typedef struct {
  uint8_t bandwidth_index;  // Indice del array bandwidth_kHz [0..9], indica el ancho de banda usado
  uint8_t spreadingFactor;  // Factor de dispersion
  uint8_t codingRate;       // Tasa de codificación 
  uint8_t txPower;          // Potencia de transmición
} LoRaConfig_t;

double bandwidth_kHz[10] = {7.8E3, 10.4E3, 15.6E3, 20.8E3, 31.25E3,
                            41.7E3, 62.5E3, 125E3, 250E3, 500E3 };

LoRaConfig_t thisNodeConf   = { 6, 12, 5, 2};   // Configuración del maestro
LoRaConfig_t remoteNodeConf = { 0, 0, 0, 0};   // Configuración del esclavo
LoRaConfig_t saveNodeConf = { 6, 12, 5, 2};   // Configuración del esclavo
int remoteRSSI = 0;
float remoteSNR = 0;
int packetCount = 0;

// --------------------------------------------------------------------
// Setup function
// --------------------------------------------------------------------
void setup() 
{
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

  if (!LoRa.begin(868E6)) {      // Initicializa la Banda ICMN LoRa a 868 MHz (fijo)
    Serial.println("LoRa init failed. Check your connections.");
    while (true);                
  }

  // Configuramos algunos parámetros de la radio
  LoRa.setSignalBandwidth(long(bandwidth_kHz[thisNodeConf.bandwidth_index])); // Ancho de banda, mayor valor menos tiempo de enlace.
                                                                              // menor valor mayor distancia de enlace y menor impacto de interferencias
                                  // 7.8E3, 10.4E3, 15.6E3, 20.8E3, 31.25E3
                                  // 41.7E3, 62.5E3, 125E3, 250E3, 500E3 
                                  // Multiplicar por dos el ancho de banda
                                  // supone dividir a la mitad el tiempo de Tx
                                  
  LoRa.setSpreadingFactor(thisNodeConf.spreadingFactor); // Factor de disperción a mayor valor más inmunidad al ruido y mayor distancia de enlace (fijo)
                                  // [6, 12] Aumentar el spreading factor incrementa 
                                  // de forma significativa el tiempo de Tx
                                  // SPF = 6 es un valor especial
                                  // Ver tabla 12 del manual del SEMTECH SX1276
  
  LoRa.setCodingRate4(thisNodeConf.codingRate); // Tasa de códificación    
                                  // [5, 8] 5 da un tiempo de Tx menor
                                  
  LoRa.setTxPower(thisNodeConf.txPower, PA_OUTPUT_PA_BOOST_PIN); // A menor potencía menor consumo de energía pero menos distancia de enlace
                                  // Rango [2, 20] en dBm
                                  // Importante seleccionar un valor bajo para pruebas
                                  // a corta distancia y evitar saturar al receptor
  LoRa.setSyncWord(0x82);         // Palabra de sincronización privada por defecto para SX127X 
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
void loop() 
{
      
  if (!transmitting && ((millis() - lastSendTime_ms) > txInterval_ms)) {
    uint8_t payload[50];
    uint8_t payloadLength = 0;

    payload[payloadLength]    = (thisNodeConf.bandwidth_index << 4);
    payload[payloadLength++] |= ((thisNodeConf.spreadingFactor - 6) << 1);
    payload[payloadLength]    = ((thisNodeConf.codingRate - 5) << 6);
    payload[payloadLength++] |= ((thisNodeConf.txPower - 2) << 1);

    // Incluimos el RSSI y el SNR del último paquete recibido
    // RSSI puede estar en un rango de [0, -127] dBm. Optimo: entre -55 y -30
    payload[payloadLength++] = uint8_t(-LoRa.packetRssi() * 2);
    // SNR puede estar en un rango de [20, -148] dBm. Optimo: lo más proximo posible a 20, 10 ya es aceptable
    payload[payloadLength++] = uint8_t(148 + LoRa.packetSnr());
    
    transmitting = true;
    txDoneFlag = false;
    tx_begin_ms = millis();
  
    sendMessage(payload, payloadLength, msgCount);
    Serial.print("\nSending configuration ");
    Serial.print(msgCount++);
    Serial.print(": ");
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
    
    pastTxTime_ms = actualTxTime_ms;
    actualTxTime_ms = TxTime_ms;
    
    Serial.print("Duty cycle: ");
    Serial.print(duty_cycle, 1);
    Serial.println(" %");

    // Solo si el ciclo de trabajo es superior al 1% lo ajustamos
    if (duty_cycle > 1.0f) {
      txInterval_ms = TxTime_ms * 40;
    }
    
    transmitting = false;

    updateConf();

    if (ack) {
      packetCount = 0;
    } else {
      packetCount++;
    }
    if (packetCount == 10) {
      resetNodesConf();
      packetCount = 0;
    }
    
    // Reactivamos la recepción de mensajes, que se desactiva
    // en segundo plano mientras se transmite
    LoRa.receive();  
    SerialUSB.println("Message reception reactivated\n");   
  }

}

// --------------------------------------------------------------------
// Sending message function
// --------------------------------------------------------------------
void sendMessage(uint8_t* payload, uint8_t payloadLength, uint16_t msgCount) 
{
  while(!LoRa.beginPacket()) {            // Comenzamos el empaquetado del mensaje
    delay(10);                            // 
  }
  LoRa.write(destination);                // Añadimos el ID del destinatario
  LoRa.write(localAddress);               // Añadimos el ID del remitente
  LoRa.write((uint8_t)(msgCount >> 7));   // Añadimos el Id del mensaje (MSB primero)
  LoRa.write((uint8_t)(msgCount & 0xFF)); 
  LoRa.write(payloadLength);              // Añadimos la longitud en bytes del mensaje
  LoRa.write(payload, (size_t)payloadLength); // Añadimos el mensaje/payload 
  LoRa.endPacket(true);                   // Finalizamos el paquete, pero no esperamos a
                                          // finalice su transmisión
}

// --------------------------------------------------------------------
// Receiving message function
// --------------------------------------------------------------------
void onReceive(int packetSize) 
{
  saveNodeConf.bandwidth_index = thisNodeConf.bandwidth_index;
  saveNodeConf.spreadingFactor = thisNodeConf.spreadingFactor;
  saveNodeConf.codingRate = thisNodeConf.codingRate;
  saveNodeConf.txPower = thisNodeConf.txPower;
  SerialUSB.println("Receiving...");
  if (transmitting && !txDoneFlag) txDoneFlag = true;
  
  if (packetSize == 0) return;          // Si no hay mensajes, retornamos

  // Leemos los primeros bytes del mensaje
  uint8_t buffer[10];                   // Buffer para almacenar el mensaje
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
  
  if (incomingLength != receivedBytes) {// Verificamos la longitud del mensaje
    Serial.print("Receiving error: declared message length " + String(incomingLength));
    Serial.println(" does not match length " + String(receivedBytes));
    packetCount++;
    ack = false;
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
  pastRemoteRSSI = actualRemoteRSSI;
  actualRemoteRSSI = LoRa.packetRssi();
  Serial.print("\nRSSI: " + String(actualRemoteRSSI));
  pastRemoteSNR = actualRemoteSNR;
  actualRemoteSNR = LoRa.packetSnr();
  Serial.print(" dBm\nSNR: " + String(actualRemoteSNR));
  Serial.println(" dB");

  // Actualizamos remoteNodeConf y lo mostramos
  if (receivedBytes == 4) {
    remoteNodeConf.bandwidth_index = buffer[0] >> 4;
    remoteNodeConf.spreadingFactor = 6 + ((buffer[0] & 0x0F) >> 1);
    remoteNodeConf.codingRate = 5 + (buffer[1] >> 6);
    remoteNodeConf.txPower = 2 + ((buffer[1] & 0x3F) >> 1);
    remoteRSSI = -int(buffer[2]) / 2.0f;
    remoteSNR  =  int(buffer[3]) - 148;
    Serial.print("Remote config: BW: ");
    Serial.print(bandwidth_kHz[remoteNodeConf.bandwidth_index]);
    Serial.print(" kHz, SPF: ");
    Serial.print(remoteNodeConf.spreadingFactor);
    Serial.print(", CR: ");
    Serial.print(remoteNodeConf.codingRate);
    Serial.print(", TxPwr: ");
    Serial.print(remoteNodeConf.txPower);
    Serial.print(" dBm, RSSI: ");
    Serial.print(remoteRSSI);
    Serial.print(" dBm, SNR: ");
    Serial.print(remoteSNR,1);
    Serial.println(" dB\n");
    ack = true;
  }
  else {
    Serial.print("Unexpected payload size: ");
    Serial.print(receivedBytes);
    Serial.println(" bytes\n");
  }
  if (firstConf) {   // Si es la primera configuracion evitamos compararla con si misma
    optimizeConf();
    firstConf = false;
  } else {          // Si se ha comprobamos si la nueva configuracion es mejor que la anterior intentado optimizar al menos una vez 
    if (!checkNewConf()) { // Si la nueva es mejor continuamos optimizando
      optimizeConf();
    } else {      // En caso contrario mandamos la configuración anterior
      sendNewConf();
    }
  }
  delay(1000);
}

void TxFinished()
{
  txDoneFlag = true;
}

void printBinaryPayload(uint8_t * payload, uint8_t payloadLength)
{
  for (int i = 0; i < payloadLength; i++) {
    Serial.print((payload[i] & 0xF0) >> 4, HEX);
    Serial.print(payload[i] & 0x0F, HEX);
    Serial.print(" ");
  }
}

void optimizeBand(boolean decrement){
  SerialUSB.println("Optimising Band Width...");
  if (decrement && thisNodeConf.bandwidth_index == 0) {
    SerialUSB.println("The slave node is disconnected or there is a communication failure");
    return;
  }
  if (!decrement && thisNodeConf.bandwidth_index == 9) {
    opt_band = true;
    return;
  }
  if (decrement) {
    thisNodeConf.bandwidth_index -= 1;
    remoteNodeConf.bandwidth_index -= 1;
    opt_band = true;
  } else {
    thisNodeConf.bandwidth_index += 1;
    remoteNodeConf.bandwidth_index += 1;
  }
}

void optimizeSpreading(boolean increment){
  SerialUSB.println("Optimising Spreading Factor...");
  if (increment && thisNodeConf.spreadingFactor == 12) {
    SerialUSB.println("The slave node is disconnected or there is a communication failure");
    return;
  }
  if (!increment && thisNodeConf.spreadingFactor == 7) {
    opt_spread = true;
    return;
  }
  if (increment) {
    thisNodeConf.spreadingFactor += 1;
    remoteNodeConf.spreadingFactor += 1;
    opt_spread = true;
  } else {
    thisNodeConf.spreadingFactor -= 1;
    remoteNodeConf.spreadingFactor -= 1;
  }
}

void optimizeCoding(boolean increment){
  SerialUSB.println("Optimising Coding Rate ...");
  if (increment && thisNodeConf.codingRate == 8) {
    SerialUSB.println("The slave node is disconnected or there is a communication failure");
    return;
  }
  if (!increment && thisNodeConf.codingRate == 5) {
    opt_coding = true;
    return;
  }
  if (increment) {
    thisNodeConf.codingRate += 1;
    remoteNodeConf.codingRate += 1;
    opt_coding = true;
  } else {
    thisNodeConf.codingRate -= 1;
    remoteNodeConf.codingRate -= 1;
  }
}

void optimizePower(boolean increment){
  SerialUSB.println("Optimising Power...");
  if (increment && thisNodeConf.txPower == 20) {
    SerialUSB.println("The slave node is disconnected or there is a communication failure");
    return;
  }
  if (!increment && thisNodeConf.txPower == 2) {
    opt_power = true;
    return;
  }
  if (increment) {
    thisNodeConf.txPower += 1;
    remoteNodeConf.txPower += 1;
    opt_power = true;
  } else {
    thisNodeConf.txPower -= 1;
    remoteNodeConf.txPower -= 1;
  }
}

void optimizeConf() {
  opt = false;
  SerialUSB.println("\nOptimising configuration...");
  saveNodeConf.bandwidth_index = thisNodeConf.bandwidth_index;
  saveNodeConf.spreadingFactor = thisNodeConf.spreadingFactor;
  saveNodeConf.codingRate = thisNodeConf.codingRate;
  saveNodeConf.txPower = thisNodeConf.txPower;
  if (!opt_band && !opt_spread && !opt_coding && !opt_power && !opt) {
    if(!ack) {
      optimizeBand(true);
    } else {
      optimizeBand(false);
    }
    opt = true;
  }
  if (opt_band && !opt_spread && !opt_coding && !opt_power && !opt) {
    if(!ack) {
      optimizeSpreading(true);
    } else {
      optimizeSpreading(false);
    }
    opt = true;
  }
  if (opt_band && opt_spread && !opt_coding && !opt_power && !opt) {
    if(!ack) {
      optimizeCoding(true);
    } else {
      optimizeCoding(false);
    }
    opt = true;
  }
  if (opt_band && opt_spread && opt_coding && !opt_power && !opt) {
    if(!ack) {
      optimizePower(true);
    } else {
      optimizePower(false);
    }
    opt = true;
  }
  if (opt_band && opt_spread && opt_coding && opt_power) {
      SerialUSB.println("The optimum configuration has been found");
      delay(10000);
  }
  if (opt_band && opt_spread && opt_coding && opt_power && !ack) { 
    // Si se encuentra la configuración optima, pero falla la comunicacion,
    // volvemos a poner todos los valores al maximo para recuperar la conexion 
    // y se vuelve a buscar la configuracion optima para la nueva situacion
    opt_band = false;
    opt_spread = false;
    opt_coding = false;
    opt_power = false;
    resetNodesConf();
  }
  sendNewConf();  // Indicamos al escalvo la nueva configuracion
}

void resetNodesConf() { // Metodo para devolver la configuracion a los valores maximos
  thisNodeConf.bandwidth_index = 6;
  thisNodeConf.spreadingFactor = 12;
  thisNodeConf.codingRate = 5;
  thisNodeConf.txPower = 2;
  remoteNodeConf.bandwidth_index = thisNodeConf.bandwidth_index;
  remoteNodeConf.spreadingFactor = thisNodeConf.spreadingFactor;
  remoteNodeConf.codingRate = thisNodeConf.codingRate;
  remoteNodeConf.txPower = thisNodeConf.txPower;
  SerialUSB.println("Communication failure, restarting configuration...");
  updateConf();
  SerialUSB.println("Configuration restarted");
}

bool checkNewConf() { // Metodo para comprobar si la configuracion anterior es mejor que la actual para recuperarla
  SerialUSB.println("Cheking new configuration...");
  SerialUSB.print("ActualSNR: ");
  SerialUSB.print(actualRemoteSNR);
  SerialUSB.print(", PastSNR: ");
  SerialUSB.print(pastRemoteSNR);
  SerialUSB.print("\nActualRSSI: ");
  SerialUSB.print(actualRemoteRSSI);
  SerialUSB.print(", PastRSSI: ");
  SerialUSB.print(pastRemoteRSSI);
  SerialUSB.print("\nActualTxTime: ");
  SerialUSB.print(actualTxTime_ms);
  SerialUSB.print("ms, PastnTxTime: ");
  SerialUSB.print(pastTxTime_ms);
  SerialUSB.print("ms\n");
  if (actualRemoteSNR < pastRemoteSNR && actualRemoteRSSI < pastRemoteRSSI && actualTxTime_ms > pastTxTime_ms) {
    // Si todos los parametros de la configuracion anterior son mejores que los de la actual
    // volvemos a la configuracion anterior
    SerialUSB.println("Previous configuration is better"); 
    thisNodeConf.bandwidth_index = saveNodeConf.bandwidth_index;
    thisNodeConf.spreadingFactor = saveNodeConf.spreadingFactor;
    thisNodeConf.codingRate = saveNodeConf.codingRate;
    thisNodeConf.txPower = saveNodeConf.txPower;
    return true;
  } else {
    SerialUSB.println("New configuration is better"); 
    return false;
  }
}

void updateConf() { // Metodo para actualizar la configuracion en caso de que algun valor haya cambiado
  if (thisNodeConf.bandwidth_index != saveNodeConf.bandwidth_index || thisNodeConf.spreadingFactor != saveNodeConf.spreadingFactor || thisNodeConf.codingRate != saveNodeConf.codingRate || thisNodeConf.txPower != saveNodeConf.txPower) {
    //Si alguno de los valores de configuracion a cambiado actualizamos la configuracion
    SerialUSB.println("Setting new configuration...");
    LoRa.setSignalBandwidth(long(bandwidth_kHz[thisNodeConf.bandwidth_index])); // Actualizamos el ancho de banda
    LoRa.setSpreadingFactor(thisNodeConf.spreadingFactor);                      // Actualizamos el Factor de dispersión
    LoRa.setCodingRate4(thisNodeConf.codingRate);                               // Actualizamos la Tasa de codificación
    LoRa.setTxPower(thisNodeConf.txPower, PA_OUTPUT_PA_BOOST_PIN);              // Actualizamos la Potencia
    saveNodeConf.bandwidth_index = thisNodeConf.bandwidth_index;
    saveNodeConf.spreadingFactor = thisNodeConf.spreadingFactor;
    saveNodeConf.codingRate = thisNodeConf.codingRate;
    saveNodeConf.txPower = thisNodeConf.txPower;
    Serial.print("New configuration set: BW: ");
    Serial.print(bandwidth_kHz[thisNodeConf.bandwidth_index]);
    Serial.print(" kHz, SPF: ");
    Serial.print(thisNodeConf.spreadingFactor);
    Serial.print(", CR: ");
    Serial.print(thisNodeConf.codingRate);
    Serial.print(", TxPwr: ");
    Serial.print(thisNodeConf.txPower);
    Serial.print(" dBm. \n");
  } else {
    Serial.print("Current configuration: BW: ");
    Serial.print(bandwidth_kHz[thisNodeConf.bandwidth_index]);
    Serial.print(" kHz, SPF: ");
    Serial.print(thisNodeConf.spreadingFactor);
    Serial.print(", CR: ");
    Serial.print(thisNodeConf.codingRate);
    Serial.print(", TxPwr: ");
    Serial.print(thisNodeConf.txPower);
    Serial.print(" dBm. \n");
  }
}

void sendNewConf() { // Metodo para comunicar al esclavo su nueva configuracion

  if (!transmitting && ((millis() - lastSendTime_ms) > txInterval_ms)) {
    SerialUSB.println("Sending new configuration...");
    uint8_t payload[50];
    uint8_t payloadLength = 0;
    payload[payloadLength]    = (thisNodeConf.bandwidth_index << 4);
    payload[payloadLength++] |= ((thisNodeConf.spreadingFactor - 6) << 1);
    payload[payloadLength]    = ((thisNodeConf.codingRate - 5) << 6);
    payload[payloadLength++] |= ((thisNodeConf.txPower - 2) << 1);

    // Incluimos el RSSI y el SNR del último paquete recibido
    // RSSI puede estar en un rango de [0, -127] dBm. Optimo: entre -55 y -30
    payload[payloadLength++] = uint8_t(-LoRa.packetRssi() * 2);
    // SNR puede estar en un rango de [20, -148] dBm. Optimo: lo más proximo posible a 20, 10 ya es aceptable
    payload[payloadLength++] = uint8_t(148 + LoRa.packetSnr());
    
    transmitting = true;
    txDoneFlag = false;
    tx_begin_ms = millis();
  
    sendMessage(payload, payloadLength, msgCount);
    Serial.print("\nSending configuration ");
    Serial.print(msgCount++);
    Serial.print(": ");
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
    
    pastTxTime_ms = actualTxTime_ms;
    actualTxTime_ms = TxTime_ms;
    
    Serial.print("Duty cycle: ");
    Serial.print(duty_cycle, 1);
    Serial.println(" %");

    // Solo si el ciclo de trabajo es superior al 1% lo ajustamos
    if (duty_cycle > 1.0f) {
      txInterval_ms = TxTime_ms * 40;
    }
    
    transmitting = false;

    updateConf();

    if (ack) {
      packetCount = 0;
    } else {
      packetCount++;
    }
    if (packetCount == 10) {
      resetNodesConf();
      packetCount = 0;
    }
    
    // Reactivamos la recepción de mensajes, que se desactiva
    // en segundo plano mientras se transmite
    LoRa.receive();
    SerialUSB.println("Message reception reactivated\n");  
  }
}
