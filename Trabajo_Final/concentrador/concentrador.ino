/*
 * Código del concentrador
 */

#include <SPI.h>             
#include <LoRa.h>
#include <Arduino_PMIC.h>
#include <Regexp.h>
#include <Wire.h>

#define LCD05  0x63                   // Dirección de LCD05
#define TX_LAPSE_MS          10000

byte buffer[3];
char stringBuf[24];

// NOTA: Ajustar estas variables 
const uint8_t localAddress = 0xA0;     // Dirección de este dispositivo
static uint8_t  destination = 0xFF;            // Dirección de destino, 0xFF es la dirección de broadcast

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

uint16_t distance_measurement = 0;
uint16_t ultrasound_wait = 10000;
uint16_t ultrasound_unit = 1;

uint16_t temperature_measurement = 0;
uint16_t thermistor_wait = 10000;
uint16_t thermistor_unit = 1;

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

  clearLCDScreen();
  writeLCDMsg(" LoRa init succeeded");
  delay(5000);
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
      char thermistor_delay_result = ms.Match("^thermistor delay [0-9]+"); // Orden para modificar el tiempo entre una medida de termistor y la siguiente
      char thermistor_unit_result = ms.Match("^thermistor unit [1-3]"); // Orden para modificar el tiempo entre una medida de termistor y la siguiente

      if (input.equalsIgnoreCase("help")) {
        // Si se solicita la ayuda o se escribe mal algún comando motramos todos los comandos disponibles
        SerialUSB.println("\n=============================================================");
        SerialUSB.println("The available commands are: ");
        SerialUSB.println("bright [number between 0 and 255]: This command allows to modify the delay between light measurements");
        SerialUSB.println("ultrasound delay [number between 0 and 255]: This command allows to modify the delay between distance measurements");
        SerialUSB.println("ultrasound unit [number between 1 and 3]: This command allows to modify the unit for distance measurements");
        SerialUSB.println("    1: It's the default option, it sets the measurement unit to centimetres (cm)");
        SerialUSB.println("    2: It sets the measurement unit to inches (inc)");
        SerialUSB.println("    3: It sets the measurement unit to milliseconds (ms)");
        SerialUSB.println("thermistor delay [number between 0 and 255]: This command allows to modify the delay between temperature measurements");
        SerialUSB.println("thermistor unit [number between 1 and 3]: This command allows to modify the unit for temperature measurements");
        SerialUSB.println("    1: It's the default option, it sets the measurement unit to Celcius degrees(C)");
        SerialUSB.println("    2: It sets the measurement unit to Kelvin (K)");
        SerialUSB.println("    3: It sets the measurement unit to Fahrenheit (F)");
        SerialUSB.println("=============================================================");
      }
      if (bright_result == REGEXP_MATCHED) { // Modificamos el delay de la fotorresistencia
        destination = 0xB1; // Cambiamos la direccion de destino al sensor de luminosidad
        int spacePositon = input.indexOf(" ");
        bright_wait = (uint8_t)(input.substring(spacePositon).toInt());   // Cambiamos el tiempo entre una medida y otra por el monitor serie y lo pasamos a ms
        SerialUSB.println("\n=============================================================");
        SerialUSB.print("The delay between brightness measurements has been changed to: ");
        SerialUSB.print((int)(bright_wait * 1000));
        SerialUSB.println("ms");
        SerialUSB.println("=============================================================");
        uint8_t payload = bright_wait;

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
      if (ultrasound_delay_result == REGEXP_MATCHED) { // Modificamos el delay del sensor de ultra sonido
        destination = 0xD1;  // Cambiamos la direccion de destino al sensor de ultrasonido
        int spacePositon = input.indexOf(" ");
        String aux = input.substring(spacePositon+1);
        int secondSpacePositon = aux.indexOf(" ");
        ultrasound_wait = (uint8_t)(aux.substring(secondSpacePositon).toInt());   // Cambiamos el tiempo entre una medida y otra por el monitor serie y lo pasamos a ms
        SerialUSB.println("\n=============================================================");
        SerialUSB.print("The delay between distance measurements has been changed to: ");
        SerialUSB.print((int)(ultrasound_wait * 1000));
        SerialUSB.println("ms");
        SerialUSB.println("=============================================================");
        uint8_t payload = ultrasound_wait;

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
      if (ultrasound_unit_result == REGEXP_MATCHED) { // Modificamos la unidad de medida del sensor de ultrasonido
        destination = 0xD1;  // Cambiamos la direccion de destino al sensor de ultrasonido
        int spacePositon = input.indexOf(" ");
        String aux = input.substring(spacePositon+1);
        int secondSpacePositon = aux.indexOf(" ");
        ultrasound_unit = (uint8_t)(aux.substring(secondSpacePositon).toInt());   // Cambiamos la unidad de medida para la distancia
        SerialUSB.println("\n=============================================================");
        SerialUSB.print("The unit for distance measurements has been changed to: ");
        if (ultrasound_unit == 3) {
          Serial.println("ms");
        } else if (ultrasound_unit == 2) {
          Serial.println("inc");
        } else {
          Serial.println("cm");
        }
        SerialUSB.println("=============================================================");
        uint8_t payload = ultrasound_unit;

        if (!transmitting && ((millis() - lastSendTime_ms) > txInterval_ms)) {
          transmitting = true;
          //txDoneFlag = false;
          txDoneFlag = true;
          tx_begin_ms = millis();
      
          sendUnitMessage(payload, msgCount, 27);
          Serial.print("Sending new ultrasound measurements unit (");
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
      if (thermistor_delay_result == REGEXP_MATCHED) { // Modificamos el delay del termistor
        destination = 0xC1;  // Cambiamos la direccion de destino al sensor de temperatura
        int spacePositon = input.indexOf(" ");
        String aux = input.substring(spacePositon+1);
        int secondSpacePositon = aux.indexOf(" ");
        thermistor_wait = (uint8_t)(aux.substring(secondSpacePositon).toInt());   // Cambiamos el tiempo entre una medida y otra por el monitor serie y lo pasamos a ms
        SerialUSB.println("\n=============================================================");
        SerialUSB.print("The delay between temperature measurements has been changed to: ");
        SerialUSB.print((int)(thermistor_wait * 1000));
        SerialUSB.println("ms");
        SerialUSB.println("=============================================================");
        uint8_t payload = thermistor_wait;

        if (!transmitting && ((millis() - lastSendTime_ms) > txInterval_ms)) {
          transmitting = true;
          //txDoneFlag = false;
          txDoneFlag = true;
          tx_begin_ms = millis();
      
          sendMessage(payload, msgCount);
          Serial.print("Sending new thermistor measurements delay (");
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
      if (thermistor_unit_result == REGEXP_MATCHED) { // Modificamos la unidad de medida del termistor
        destination = 0xC1;  // Cambiamos la direccion de destino al sensor de temperatura
        int spacePositon = input.indexOf(" ");
        String aux = input.substring(spacePositon+1);
        int secondSpacePositon = aux.indexOf(" ");
        thermistor_unit = (uint8_t)(aux.substring(secondSpacePositon).toInt());   // Cambiamos la unidad de medida para la temperatura
        SerialUSB.println("\n=============================================================");
        SerialUSB.print("The unit for temperature measurements has been changed to: ");
        if (thermistor_unit == 3) {
          Serial.println("Fahrenheit");
        } else if (thermistor_unit == 2) {
          Serial.println("Kelvin");
        } else {
          Serial.println("Celsius");
        }
        SerialUSB.println("=============================================================");
        uint8_t payload = thermistor_unit;

        if (!transmitting && ((millis() - lastSendTime_ms) > txInterval_ms)) {
          transmitting = true;
          //txDoneFlag = false;
          txDoneFlag = true;
          tx_begin_ms = millis();
      
          sendUnitMessage(payload, msgCount, 27);
          Serial.print("Sending new thermistor measurements unit (");
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

// Sending new unit configuration for a sensor
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
  if (String(sender, HEX).equalsIgnoreCase("b1")) {  // Medidas de la fotorresistencia
    bright_measurement = *((uint16_t*)buffer);

    clearLCDScreen();
    writeLCDMsg(" Brightness: "+ String(bright_measurement)+" Lux. Direct: "+ measurement);

    SerialUSB.println("\n=============================================================");
    String bright_msg = "Remote brightness measurement: " + String(bright_measurement);
    bright_msg = bright_msg + " Lux";
    Serial.println(bright_msg);
    String light_msg = "Remote direct light measurement: " + String(measurement);
    if (measurement == 1) {
      light_msg = light_msg + " (direct)";
    } else {
      light_msg = light_msg + " (indirect)";
    }
    Serial.println(light_msg);
    SerialUSB.println("=============================================================");
  } else if (String(sender, HEX).equalsIgnoreCase("d1")) { // Medidas del ultrasonido
    distance_measurement = *((uint16_t*)buffer);

    SerialUSB.println("\n=============================================================");
    String ultrasound_msg = "Remote ultrasound measurement: " + String(distance_measurement);
    ultrasound_unit = measurement;

    clearLCDScreen();
    writeLCDMsg(" Ultrasound: "+ String(distance_measurement)+". Measure: "+ ultrasound_unit);
    
    if (ultrasound_unit == 3) {
      ultrasound_msg = ultrasound_msg + " ms";
    } else if (ultrasound_unit == 2) {
      ultrasound_msg = ultrasound_msg + " inc";
    } else {
      ultrasound_msg = ultrasound_msg + " cm";
    }
    Serial.println(ultrasound_msg);
    SerialUSB.println("=============================================================");
  } else if (String(sender, HEX).equalsIgnoreCase("c1")) { // Medidas del termistor
    temperature_measurement = *((uint16_t*)buffer);
    
    SerialUSB.println("\n=============================================================");
    String temperature_msg = "Remote thermistor measurement: " + String(temperature_measurement);
    thermistor_unit = measurement;

    clearLCDScreen();
    writeLCDMsg(" Thermistor: "+ String(temperature_measurement)+". Measure: "+ thermistor_unit);
    
    if (thermistor_unit == 3) {
      temperature_msg = temperature_msg + " F"; // Se mide en Fahrenheit
    } else if (thermistor_unit == 2) {
      temperature_msg = temperature_msg + " K"; // Se mide en Kelvin
    } else {
      temperature_msg = temperature_msg + " C"; // Se mide en Celsius
    }
    Serial.println(temperature_msg);
    SerialUSB.println("=============================================================");
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

void clearLCDScreen(){
  Wire.begin();
  buffer[0] = 0;                       // Clear the screen
  buffer[1] = 12;
  Wire.beginTransmission(LCD05);
  Wire.write(buffer,2); 
  Wire.endTransmission();
}

void writeLCDMsg(String msg){
  int len = msg.length() + 1;       // Length of the message
  msg.toCharArray(stringBuf, len);  // Convert the message to a car array
  stringBuf[0] = 0;                     // First byte of message to 0 (LCD05 command register)
  Wire.beginTransmission(LCD05);
  Wire.write(stringBuf, len);
  Wire.endTransmission();
}
