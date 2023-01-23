/* 
 * srf02_example.ino
 * Example showing how to use the Devantech SRF02 ultrasonic sensor
 * in I2C mode. More info about this LCD display in
 *   http://www.robot-electronics.co.uk/htm/srf02techI2C.htm
 *
 * author: Antonio C. Domínguez Brito <adominguez@iusiani.ulpgc.es>
 */
 
/* 
 * Arduino MKR WAN 1310 I2C pins
 *   Pin 11 -> SDA (I2C data line)
 *   Pin 12 -> SCL (I2C clock line)
 */
 
#include <Wire.h>
#include <string.h>
#include <SPI.h>             
#include <LoRa.h>
#include <Arduino_PMIC.h>

 // Arduino's I2C library
 
#define SRF02_I2C_ADDRESS byte((0xEA)>>1)
#define SRF02_I2C_INIT_DELAY 100 // in milliseconds
int SRF02_RANGING_DELAY = 11000; // milliseconds

// LCD05's command related definitions
#define COMMAND_REGISTER byte(0x00)
#define SOFTWARE_REVISION byte(0x00)
#define RANGE_HIGH_BYTE byte(2)
#define RANGE_LOW_BYTE byte(3)
#define AUTOTUNE_MINIMUM_HIGH_BYTE byte(4)
#define AUTOTUNE_MINIMUM_LOW_BYTE byte(5)

// SRF02's command codes
#define REAL_RANGING_MODE_INCHES    byte(80)
#define REAL_RANGING_MODE_CMS       byte(81)
#define REAL_RANGING_MODE_USECS     byte(82)
#define FAKE_RANGING_MODE_INCHES    byte(86)
#define FAKE_RANGING_MODE_CMS       byte(87)
#define FAKE_RANGING_MODE_USECS     byte(88)
#define TRANSMIT_8CYCLE_40KHZ_BURST byte(92)
#define FORCE_AUTOTUNE_RESTART      byte(96)
#define ADDRESS_CHANGE_1ST_SEQUENCE byte(160)
#define ADDRESS_CHANGE_3RD_SEQUENCE byte(165)
#define ADDRESS_CHANGE_2ND_SEQUENCE byte(170)

#define TX_LAPSE_MS 10000

// NOTA: Ajustar estas variables 
const uint8_t localAddress = 0xD1;     // Dirección de este dispositivo
uint8_t destination = 0xA0;            // Dirección de destino, 0xFF es la dirección de broadcast

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

uint8_t min_measurement = 0;
uint8_t measurement_unit = 1;
uint16_t real_measurement = 0;

bool cm_flag = true;
bool ms_flag = false;
bool inc_flag = false;

inline void write_command(byte address,byte command)
{ 
  Wire.beginTransmission(address);
  Wire.write(COMMAND_REGISTER); 
  Wire.write(command); 
  Wire.endTransmission();
}

byte read_register(byte address,byte the_register)
{
  Wire.beginTransmission(address);
  Wire.write(the_register);
  Wire.endTransmission();
  
  // getting sure the SRF02 is not busy
  Wire.requestFrom(address,byte(1));
  while(!Wire.available()) { /* do nothing */ }
  return Wire.read();
} 
 
// the setup routine runs once when you press reset:
void setup() 
{
  Serial.begin(9600);
  
  Serial.println("initializing Wire interface ...");
  Wire.begin();
  delay(SRF02_I2C_INIT_DELAY);  
   
  byte software_revision=read_register(SRF02_I2C_ADDRESS,SOFTWARE_REVISION);
  Serial.print("SFR02 ultrasonic range finder in address 0x");
  Serial.print(SRF02_I2C_ADDRESS,HEX); Serial.print("(0x");
  Serial.print(software_revision,HEX); Serial.println(")");

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
  LoRa.setSyncWord(0xEA);         // Palabra de sincronización privada por defecto para SX127X 
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

// the loop routine runs over and over again forever:
void loop() {

  static uint32_t lastSendTime_ms = 0;
  static uint32_t txInterval_ms = TX_LAPSE_MS;
  static uint32_t tx_begin_ms = 0;

  uint8_t payload[2];
  uint8_t payloadLength = 2;

  real_measurement = -1;
  if(!transmitting) {
    distanceMeasure();
  }
  
  memcpy(payload, &real_measurement, payloadLength);
  if (real_measurement != -1) {
    if (!transmitting && ((millis() - lastSendTime_ms) > txInterval_ms)) {
        transmitting = true;
        txDoneFlag = false;
        tx_begin_ms = millis();
    
        sendMessage(payload, payloadLength, msgCount);
        Serial.print("Sending new distance measurements (");
        Serial.print(msgCount++);
        Serial.print("): ");
        printBinaryPayload(payload, payloadLength);
        printUnitMeasurement();
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
void sendMessage(uint8_t* payload, uint8_t payloadLength, uint16_t msgCount) {
  while(!LoRa.beginPacket()) {            // Comenzamos el empaquetado del mensaje
    delay(10);                            // 
  }
  LoRa.write(destination);                // Añadimos el ID del destinatario
  LoRa.write(localAddress);               // Añadimos el ID del remitente
  LoRa.write((uint8_t)(msgCount >> 7));   // Añadimos el Id del mensaje (MSB primero)
  LoRa.write((uint8_t)(msgCount & 0xFF));
  LoRa.write(measurement_unit); 
  LoRa.write(payloadLength);              // Añadimos la longitud en bytes del mensaje
  LoRa.write(payload, (size_t)payloadLength); // Añadimos el mensaje/payload 
  LoRa.endPacket(true);
}

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
  
  uint8_t incomingConfig = LoRa.read(); // Nueva configuracion de delay o unidades
  uint8_t unit_flag = LoRa.read(); // flag para cambio de unidades
  
  uint8_t receivedBytes = 1;            // Leemos el mensaje byte a byte

  // Verificamos si se trata de un mensaje en broadcast o es un mensaje
  // dirigido específicamente a este dispositivo.
  // Nótese que este mecanismo es complementario al uso de la misma
  // SyncWord y solo tiene sentido si hay más de dos receptores activos
  // compartiendo la misma palabra de sincronización
  if ((recipient & localAddress) != localAddress ) {
    Serial.println("\nReceiving error: This message is not for me.\n");
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

  if ((int)unit_flag == 27) {
    Serial.print("\n=============================================\n");
    Serial.print("New unit configuration: ");
    if ((int)incomingConfig == 3) { // Cambio de unidad de medida a ms
      ms_flag = true;
      cm_flag = false;
      inc_flag = false;
      measurement_unit = 3;
      Serial.println("ms");
    } else if ((int)incomingConfig == 2) { // Cambio de unidad de medida a inc
      inc_flag = true;
      ms_flag = false;
      cm_flag = false;
      measurement_unit = 2;
      Serial.println("inc");
    } else { // Cambio de unidad de medida a cm
      cm_flag = true;
      inc_flag = false;
      ms_flag = false;
      measurement_unit = 1;
      Serial.println("cm");
    }
    Serial.println("=============================================\n");
  } else {
    Serial.print("\n=============================================\n");
    SRF02_RANGING_DELAY = (int)incomingConfig;
    SRF02_RANGING_DELAY = SRF02_RANGING_DELAY * 1000;
    Serial.print("New delay configuration: ");
    Serial.print(SRF02_RANGING_DELAY);
    Serial.println(" ms.");
    Serial.println("=============================================\n");
  }
}

void TxFinished() {
  txDoneFlag = true;
}

void distanceMeasure() {
  SerialUSB.println("ranging ...");
  if (ms_flag && !cm_flag && !inc_flag) {
    write_command(SRF02_I2C_ADDRESS,REAL_RANGING_MODE_USECS);
  } else if (!ms_flag && !cm_flag && inc_flag) {
    write_command(SRF02_I2C_ADDRESS,REAL_RANGING_MODE_INCHES);
  } else {
    write_command(SRF02_I2C_ADDRESS,REAL_RANGING_MODE_CMS);
  }
  delay(SRF02_RANGING_DELAY);
  byte high_byte_range=read_register(SRF02_I2C_ADDRESS,RANGE_HIGH_BYTE);
  byte low_byte_range=read_register(SRF02_I2C_ADDRESS,RANGE_LOW_BYTE);
  byte high_min=read_register(SRF02_I2C_ADDRESS,AUTOTUNE_MINIMUM_HIGH_BYTE);
  byte low_min=read_register(SRF02_I2C_ADDRESS,AUTOTUNE_MINIMUM_LOW_BYTE);
  
  real_measurement = int((high_byte_range<<8) | low_byte_range);
  SerialUSB.print("Has been measured ");
  SerialUSB.print(real_measurement);
  if (ms_flag && !cm_flag && !inc_flag) {
    Serial.print(" ms. (min=");
  } else if (!ms_flag && !cm_flag && inc_flag) {
    Serial.print(" inc. (min=");
  } else {
    Serial.print(" cms. (min=");
  }
  min_measurement = int((high_min<<8) | low_min);
  SerialUSB.print(min_measurement);
  if (ms_flag && !cm_flag && !inc_flag) {
    Serial.println(" ms.)");
  } else if (!ms_flag && !cm_flag && inc_flag) {
    Serial.println(" inc.)");
  } else {
    Serial.println(" cms.)");
  }
}

// Método que nos permite imprimir la medida que se envía
void printUnitMeasurement() {
  if (ms_flag && !cm_flag && !inc_flag) {
    Serial.print(" ms ");
  } else if (!ms_flag && !cm_flag && inc_flag) {
    Serial.print(" inc ");
  } else {
    Serial.print(" cms ");
  }
}

void printBinaryPayload(uint8_t * payload, uint8_t payloadLength) {
  for (int i = 0; i < payloadLength-1; i++) {
    Serial.print(payload[i], HEX);
    Serial.print(" ");
  }
}
