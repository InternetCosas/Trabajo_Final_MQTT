/* 
 *  --------------------------------------------------------------------------------
 *  Ejemplo 8:
 *  - Este programa muestra la forma de usar el SPIFFS sobre el chip de memoria.
 *  - Ejecutar este programa es necesario para poder usar un sistema de archivos
 *    SPIFFS (SPI Flash File System) sobre la memoria.
 *    
 *  - Usa la librería MKRMEM 
 *    https://github.com/arduino-libraries/Arduino_MKRMEM
 *  - Pero es necesario comentar la declaración de la variable flash como en el 
 *    fichero src/Arduino_WQ16DV.cpp de la librería MKRMEM (líneas 193 - 199)
 *  
 *--------------------------------------------------------------------------------
 */

#include <Arduino_MKRMEM.h>
 
// --------------------------------------------------------------------------------
// IMPORTANTE: Trasladamos aquí la declaración de la variable flash para ajustar
//             el bus SPI y el pin CS de la FLASH
// --------------------------------------------------------------------------------

Arduino_W25Q16DV flash(SPI1, FLASH_CS);
char filename[] = "datos.txt";

// --------------------------------------------------------------------------------
// 
// --------------------------------------------------------------------------------
void setup()
{
  // Recordar que LORA_RESET está definida en 
  // .arduino15/packages/arduino/hardware/samd/1.8.13/variants/mkrwan1300/variant.h
  pinMode(LORA_RESET, OUTPUT);    // Declaramos LORA reset pin como salida
  digitalWrite(LORA_RESET, LOW);  // Lo ponemos a nivel bajo para desactivar el módulo 
                                  // LoRA
   
  SerialUSB.begin(9600);
  while(!SerialUSB) { ; }

  // Inicializamos la memoria FLASH
  flash.begin();

  // Montamos el sistema de archivos
  SerialUSB.println("Mounting ...");
  int res = filesystem.mount();
  if(res != SPIFFS_OK && res != SPIFFS_ERR_NOT_A_FS) {
    SerialUSB.println("mount() failed with error code "); 
    SerialUSB.println(res); 
    exit(EXIT_FAILURE);
  }

  // Creamos un nuevo fichero
  // Podríamos usar create(), pero open() proporciona más flexibilidad (flags)
  File file = filesystem.open(filename,  CREATE | TRUNCATE);
  if (!file) {
    SerialUSB.print("Creation of file ");
    SerialUSB.print(filename);
    SerialUSB.print(" failed. Aborting ...");
    on_exit_with_error_do();
  }
  file.close();
}

// --------------------------------------------------------------------------------
// 
// --------------------------------------------------------------------------------
void loop() 
{ 
  static int line = 0;
  char data_line[50];

  if (line < 10) {
    
    // Reabrimos el fichero en cada iteración para añadir datos
    File file = filesystem.open(filename,  WRITE_ONLY | APPEND);
    if (!file) {
      SerialUSB.print("Opening file ");
      SerialUSB.print(filename);
      SerialUSB.print(" failed for appending. Aborting ...");
      on_exit_with_error_do();
    }
    
    // Escribimos una línea en el fichero por segundo
    snprintf(data_line, sizeof(data_line), "At %lu: this is line no. %02d\n",
             millis(), ++line);

    SerialUSB.print("Writing: ");
    SerialUSB.print(data_line);
    
    int const bytes_to_write = strlen(data_line);
    int const bytes_written = file.write((void *)data_line, bytes_to_write);
    if (bytes_to_write != bytes_written) {
      SerialUSB.print("write() failed with error code "); 
      SerialUSB.println(filesystem.err());
      SerialUSB.println("Aborting ...");
      on_exit_with_error_do();
    }
    // Cerramos el fichero
    file.close();
    delay(998);
  }
  else {
    
    // Abrimos el fichero para lectura
    File file = filesystem.open(filename,  READ_ONLY);
    if (!file) {
      SerialUSB.print("Opening file ");
      SerialUSB.print(filename);
      SerialUSB.print(" failed for reading. Aborting ...");
      on_exit_with_error_do();
    }    
    SerialUSB.print("Reading file contents:\n\t ");
    
    // Leemos el contenido del fichero hasta alcanzar la marca EOF
    while(!file.eof()) {
      char c;
      int const bytes_read = file.read(&c, sizeof(c));
      if (bytes_read) {
        SerialUSB.print(c);
        if (c == '\n') SerialUSB.print("\t ");
      }
    }

    // Cerramos el fichero
    file.close();
    SerialUSB.println("\nFile closed");

    // Desmontamos el sistema de archivos
    SerialUSB.println("Unmounting filesystem ... (program finished)");
    filesystem.unmount();
    exit(0);
  }
}

// --------------------------------------------------------------------------------
// Utilidad para desmontar el sistema de archivos y terminar en caso de error
// --------------------------------------------------------------------------------
void on_exit_with_error_do()
{
  filesystem.unmount();
  exit(EXIT_FAILURE);
}
