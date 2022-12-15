/* ---------------------------------------------------------------------
 *  Práctica 3
 *  Asignatura (GII-IoT)
 *  
 *  Módulo Esclavo
 *  Simon Rusu
 *  Maria Naranjo
 * ---------------------------------------------------------------------
 */

#include <SPI.h>             
#include <LoRa.h>
#include <Arduino_PMIC.h>

#define TX_LAPSE_MS          10000

// NOTA: Ajustar estas variables 
const uint8_t localAddress = 0x91;     // Dirección de este dispositivo
uint8_t destination = 0x90;            // Dirección de destino, 0xFF es la dirección de broadcast

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

LoRaConfig_t thisNodeConf   = { 7, 12, 5, 2}; //Max, Max, Min, Min
LoRaConfig_t remoteNodeConf = { 0, 0, 0, 0};
int remoteRSSI = 0;
float remoteSNR = 0;

static uint32_t lastSendTime_ms = 0;
static uint16_t msgCount = 0;
static uint32_t txInterval_ms = TX_LAPSE_MS;
static uint32_t tx_begin_ms = 0;

// -------------------------------------------------
// Variables de control de comunicación
uint8_t status = 0;
bool changesOnQueue = false;
unsigned long currentMillis;
unsigned long startMillis;
const unsigned long period = 1000;
int timeCount = 0;

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
  LoRa.setSyncWord(0xAA);         // Palabra de sincronización privada por defecto para SX127X 
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

// --------------------------------------------
// Cada segundo se comprueba si hay 
// cambios pendientes de aplicar

  currentMillis = millis();
  if( (currentMillis - startMillis >= 10000) && !transmitting){
    if(changesOnQueue){
      changeParams();
      changesOnQueue = false;
    }
    if (timeCount == 15){
      resetValues();
      timeCount = 0; 
    }

    timeCount++;
    startMillis = currentMillis;
  }
// --------------------------------------------    
                      
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
    Serial.println(" %");

    // Solo si el ciclo de trabajo es superior al 1% lo ajustamos
    if (duty_cycle > 1.0f) {
      txInterval_ms = TxTime_ms * 80;
    }
    
    transmitting = false;
    
    // Reactivamos la recepción de mensajes, que se desactiva
    // en segundo plano mientras se transmite
    LoRa.receive();   
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
  LoRa.write(status);                     // Cambiamos el estado ------------------------> Añadida
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
  if (transmitting && !txDoneFlag) txDoneFlag = true;
  
  if (packetSize == 0) return;          // Si no hay mensajes, retornamos

  // Leemos los primeros bytes del mensaje
  uint8_t buffer[10];                   // Buffer para almacenar el mensaje
  int recipient = LoRa.read();          // Dirección del destinatario
  uint8_t sender = LoRa.read();         // Dirección del remitente
  uint8_t newStatus = LoRa.read();      // Leemos el estado ------------------------> Añadida

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

  // Actualizamos remoteNodeConf y lo mostramos
  if (receivedBytes == 4) {

    remoteNodeConf.bandwidth_index = buffer[0] >> 4;
    remoteNodeConf.spreadingFactor = 6 + ((buffer[0] & 0x0F) >> 1);
    remoteNodeConf.codingRate = 5 + (buffer[1] >> 6);
    remoteNodeConf.txPower = 2 + ((buffer[1] & 0x3F) >> 1);
    remoteRSSI = -int(buffer[2]) / 2.0f;
    remoteSNR  =  int(buffer[3]) - 148;

    thisNodeConf.bandwidth_index = remoteNodeConf.bandwidth_index;
    thisNodeConf.spreadingFactor = remoteNodeConf.spreadingFactor;
    thisNodeConf.txPower = remoteNodeConf.txPower;
    thisNodeConf.codingRate = remoteNodeConf.codingRate;

    Serial.println("\n-----------------------------------------------");
    Serial.println("Packet received from: 0x" + String(sender, HEX));
    Serial.print("Slave config: BW: ");
    Serial.print(String(bandwidth_kHz[thisNodeConf.bandwidth_index]));
    Serial.print(" kHz, SPF: ");
    Serial.print(String(thisNodeConf.spreadingFactor));
    Serial.print(", CR: ");
    Serial.print(String(thisNodeConf.codingRate));
    Serial.print(", TxPwr: ");
    Serial.print(String(thisNodeConf.txPower));
    Serial.print(" dBm, RSSI: ");
    Serial.print(String(LoRa.packetRssi()));
    Serial.print(" dBm, SNR: ");
    Serial.print(String(LoRa.packetSnr()));
    Serial.print(" dB, ");
    Serial.println("Status: "+ String(status));
  
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
    Serial.print(" dB, ");
    Serial.println("NewStatus: "+ String(newStatus));
    Serial.println("-----------------------------------------------");

// ------------------------------------------------------------------
// Actualizamos el estado, añadimos cambios pendientes y mandamos
// un paquete de confirmación
    status = newStatus;
    timeCount = 0; 

    if(status == 1){
      status = 2;
      changesOnQueue = true;
      sendPacket();
    }
// ------------------------------------------------------------------

  }
  else {
    Serial.print("Unexpected payload size: ");
    Serial.print(receivedBytes);
    Serial.println(" bytes\n");

// ------------------------------------------------------------------
// Volvemos a confirmar cambios y mandar un paquete de confirmación
// en caso de que haya un fallo de recepción de paquete
    if(status == 1){
      status = 2;
      changesOnQueue = true;
      sendPacket();
    }
// ------------------------------------------------------------------
  }

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

// Se aplican los cambios de parámetro del Esclavo
void changeParams(){
    Serial.println("\nSlave está aplicando una nueva configuración...");
    LoRa.setSignalBandwidth(long(bandwidth_kHz[thisNodeConf.bandwidth_index])); 
    LoRa.setSpreadingFactor(thisNodeConf.spreadingFactor);     
    LoRa.setCodingRate4(thisNodeConf.codingRate);                  
    LoRa.setTxPower(thisNodeConf.txPower, PA_OUTPUT_PA_BOOST_PIN);
    Serial.println("Slave ha aplicado una nueva configuración.");
    Serial.println("Configuración del Slave: ");
    Serial.print("Slave config: BW: ");
    Serial.print(String(bandwidth_kHz[thisNodeConf.bandwidth_index]));
    Serial.print(" kHz, SPF: ");
    Serial.print(String(thisNodeConf.spreadingFactor));
    Serial.print(", CR: ");
    Serial.print(String(thisNodeConf.codingRate));
    Serial.print(", TxPwr: ");
    Serial.print(String(thisNodeConf.txPower));
    Serial.print(" dBm, RSSI: ");
    Serial.print(String(LoRa.packetRssi()));
    Serial.print(" dBm, SNR: ");
    Serial.print(String(LoRa.packetSnr()));
    Serial.print(" dB, ");
    Serial.println("Status: "+ String(status));
}

void sendPacket(){
    uint8_t payload[50];
    uint8_t payloadLength = 0;

    payload[payloadLength]    = (thisNodeConf.bandwidth_index << 4);
    payload[payloadLength++] |= ((thisNodeConf.spreadingFactor - 6) << 1);
    payload[payloadLength]    = ((thisNodeConf.codingRate - 5) << 6);
    payload[payloadLength++] |= ((thisNodeConf.txPower - 2) << 1);

    // Incluimos el RSSI y el SNR del último paquete recibido
    // RSSI puede estar en un rango de [0, -127] dBm
    payload[payloadLength++] = uint8_t(-LoRa.packetRssi() * 2);
    // SNR puede estar en un rango de [20, -148] dBm
    payload[payloadLength++] = uint8_t(148 + LoRa.packetSnr());
    
    transmitting = true;
    txDoneFlag = false;
    tx_begin_ms = millis();

    sendMessage(payload, payloadLength, msgCount);
    Serial.print("Sending packet ");
    Serial.print(msgCount++);
    Serial.print(": ");
    printBinaryPayload(payload, payloadLength);
}

void resetValues(){
  Serial.println("Se ha perdido comunicación. Reseteando valores...");
  thisNodeConf   = { 7, 12, 5, 2}; //Max, Max, Min, Min
  remoteNodeConf = { 0, 0, 0, 0};
  changeParams();
}
