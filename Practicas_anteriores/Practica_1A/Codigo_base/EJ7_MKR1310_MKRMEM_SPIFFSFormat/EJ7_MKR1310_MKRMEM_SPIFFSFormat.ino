/* Ejemplo 7:
 *  - Este programa verifica el acceso al chip de memoria flash del MKR1310,
 *    lo formatea e informa de su capacidad.
 *  - Ejecutar este programa es necesario para poder usar un sistema de archivos
 *    SPIFFS (SPI Flash File System) sobre la memoria.
 *    
 *  - Usa la librería MKRMEM 
 *    https://github.com/arduino-libraries/Arduino_MKRMEM
 *  - Pero es necesario comentar la declaración de la variable flash como en el 
 *    fichero src/Arduino_WQ16DV.cpp de la librería MKRMEM (líneas 193 - 199)
 *   * 
 * Adaptado del programa SPIFFSFormat.ino de Alexander Entinger
 */

/**************************************************************************************
 * INCLUDE
 **************************************************************************************/

#include <Arduino_MKRMEM.h>

Arduino_W25Q16DV flash(SPI1, FLASH_CS);

/**************************************************************************************
 * SETUP/LOOP
 **************************************************************************************/

void setup()
{
  // Recordar que LORA_RESET está definida en 
  // .arduino15/packages/arduino/hardware/samd/1.8.13/variants/mkrwan1300/variant.h
  pinMode(LORA_RESET, OUTPUT);    // Declaramos LORA reset pin como salida
  digitalWrite(LORA_RESET, LOW);  // LO ponemos a nivel bajo para descativar el módulo LoRA
   
  SerialUSB.begin(9600);
  while(!SerialUSB) { ; }
  
  flash.begin();

  SerialUSB.println("Erasing chip ...");
  flash.eraseChip();

  SerialUSB.println("Mounting ...");

  // filesystem is declared inArduino_SPIFFS.cpp
  int res = filesystem.mount();
  if (res != SPIFFS_OK && res != SPIFFS_ERR_NOT_A_FS) {
    SerialUSB.println("mount() failed with error code "); SerialUSB.println(res); return;
  }

  SerialUSB.println("Unmounting ...");
  filesystem.unmount();

  SerialUSB.println("Formatting ...");
  res = filesystem.format();
  if(res != SPIFFS_OK) {
    SerialUSB.println("format() failed with error code "); SerialUSB.println(res); return;
  }

  SerialUSB.println("Mounting ...");
  res = filesystem.mount();
  if(res != SPIFFS_OK) {
    SerialUSB.println("mount() failed with error code "); SerialUSB.println(res); return;
  }

  SerialUSB.println("Checking ...");
  res = filesystem.check();
  if(res != SPIFFS_OK) {
    SerialUSB.println("check() failed with error code "); SerialUSB.println(res); return;
  }

  SerialUSB.println("Retrieving filesystem info ...");
  unsigned int bytes_total = 0,
               bytes_used  = 0;
  res = filesystem.info(bytes_total, bytes_used);
  if(res != SPIFFS_OK) {
    SerialUSB.println("check() failed with error code "); SerialUSB.println(res); return;
  } else {
    char msg[64] = {0};
    snprintf(msg, sizeof(msg), "SPIFFS Info:\nBytes Total: %d\nBytes Used:  %d", bytes_total, bytes_used);
    SerialUSB.println(msg);
  }

  SerialUSB.println("Unmounting ... (formatting finished!");
  filesystem.unmount();
}

void loop() { }
