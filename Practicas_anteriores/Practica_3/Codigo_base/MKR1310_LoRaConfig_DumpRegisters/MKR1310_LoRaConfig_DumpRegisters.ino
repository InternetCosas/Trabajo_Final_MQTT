/* ---------------------------------------------------------------------
 *  Ejemplo MKR1310_LoRaConfig_DumpRegisters
 *  Práctica 3
 *  Asignatura (GII-IoT)
 *  
 *  Este ejemplo requiere de una versión modificada
 *  de la librería Arduino LoRa (descargable desde 
 *  CV de la asignatura
 *  
 *  También usa la librería Arduino_BQ24195 
 *  https://github.com/arduino-libraries/Arduino_BQ24195
 * ---------------------------------------------------------------------
 */
#include <SPI.h>            
#include <LoRa.h>
#include <Arduino_PMIC.h>

void setup() 
{
  SerialUSB.begin(9600);               
  while (!SerialUSB);

  if (!init_PMIC()) {
    SerialUSB.println("Initilization of BQ24195L failed!");
  }
  else {
    SerialUSB.println("Initilization of BQ24195L succeeded!");
  }

  if (!LoRa.begin(868E6)) {      // Initicializa a 868 MHz
    SerialUSB.println("LoRa init failed. Check your connections.");
    while (true);                
  }

  Serial.println("\nLoRa Dump Registers");
  SerialUSB.println("\nDefault configuration:");
  LoRa.dumpRegisters(SerialUSB);
  

  LoRa.setSignalBandwidth(500E3); // 7.8E3, 10.4E3, 15.6E3, 20.8E3, 31.25E3
                                  // 41.7E3, 62.5E3, 125E3, 250E3, 500E3 
                                  // Multiplicar por dos el ancho de banda
                                  // supone dividir a la mitad el tiempo de Tx
  LoRa.setSpreadingFactor(8);     // [6, 12] Aumentar el spreading factor incrementa 
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

  SerialUSB.println("\nCurrent configuration:");
  LoRa.dumpRegisters(SerialUSB);  // Ver sección 4 del manual del SEMTECH SX1276
                                  // Nos permite ver cómo esta configuración se refleja a
                                  // nivel de registros
                                  
  LoRa.sleep();                   // Ver tabla 16 del manual del SEMTECH SX1276
}


void loop()  { }
