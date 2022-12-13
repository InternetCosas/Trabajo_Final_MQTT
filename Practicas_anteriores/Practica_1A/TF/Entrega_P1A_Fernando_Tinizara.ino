// Fernando Sanfiel Reyes
// Tinizara Maria Rodriguez Delgado

#include <time.h>
#include <RTCZero.h>
#include <Arduino_MKRMEM.h>

// rtc object
RTCZero rtc;

// Descomentar para impedir múltiples interrupciones por rebote 
#define WITH_HYSTERESIS   

#if defined(WITH_HYSTERESIS)
  const uint32_t inhibitionTime_ms = 250;
#endif

#define elapsedMilliseconds(since_ms)  (uint32_t)(millis() - since_ms)

#define INTERRUPT_PIN     5

// Declaramos la variable bandera como volátil y la inicializamos
volatile uint32_t int_flag = 0;
volatile uint32_t _period_sec = 0;
volatile uint16_t _rtcFlag = 0;

static int line = 0; // Declaramos el control de líneas del fichero
String data;         // Declaramos una variable global para guardar facilmente los datos que van a almacenarse en el fichero
bool first_time = true;

// --------------------------------------------------------------------------------
// IMPORTANTE: Trasladamos aquí la declaración de la variable flash para ajustar
//             el bus SPI y el pin CS de la FLASH
// --------------------------------------------------------------------------------

Arduino_W25Q16DV flash(SPI1, FLASH_CS);
char filename[] = "datos.txt";

void setup() {

  // Registramos el tiempo de comienzo
  uint32_t t_start_ms = millis();

  // Activamos el pin de control del LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Recordar que LORA_RESET está definida en 
  // .arduino15/packages/arduino/hardware/samd/1.8.13/variants/mkrwan1300/variant.h
  pinMode(LORA_RESET, OUTPUT);    // Declaramos LORA reset pin como salida
  digitalWrite(LORA_RESET, LOW);  // Lo ponemos a nivel bajo para desactivar el módulo 
                                  // LoRA

  SerialUSB.begin(9600);
  while(!SerialUSB) {;}

  // Ajustamos el modo del INTERRUPT_PIN
  // Lo configuramos en modo pullup para que se produzca una
  // interrupción por flanco de bajada (FALLING) al llevarlo a tierra
  pinMode(INTERRUPT_PIN, INPUT_PULLUP); 
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), my_isr, FALLING);

  // Notificamos el momento en el que comienza el programa
  SerialUSB.print("Comenzando: ");
  SerialUSB.print(__DATE__);
  SerialUSB.print(" ");
  SerialUSB.println(__TIME__);

  data = "Comienzo: " + String(__DATE__) + " " +  String(__TIME__); // Guardamos el momento de inicio del programa para escribirlo en el archivo posteriormente

  // Habilitamos el uso del rtc
  rtc.begin();

  if (!setDateTime(__DATE__, __TIME__))
  {
    SerialUSB.println("setDateTime() failed!\nExiting ...");
    while (1) { ; }
  }

  // Activar la alarma cada 10 segundos a partir de 5 secs
   setPeriodicAlarm(10, 5);

   digitalWrite(LED_BUILTIN, LOW);

   // Limpiamos _rtcFlag
   _rtcFlag = 0;
   
   // Activar la rutina de atención
   rtc.attachInterrupt(alarmCallback);

     // Inicializamos la memoria FLASH
  flash.begin();

  // Montamos el sistema de archivos
  SerialUSB.println("Montando sistema de archivos ...");
  int res = filesystem.mount();
  if(res != SPIFFS_OK && res != SPIFFS_ERR_NOT_A_FS) {
    SerialUSB.println("Montaje fallido con código de error: "); 
    SerialUSB.println(res); 
    exit(EXIT_FAILURE);
  }

  // Creamos un nuevo fichero
  // Podríamos usar create(), pero open() proporciona más flexibilidad (flags)
  File file = filesystem.open(filename,  CREATE | TRUNCATE);
  if (!file) {
    SerialUSB.print("Creación del fichero ");
    SerialUSB.print(filename);
    SerialUSB.print(" fallida. Abortando ...");
    on_exit_with_error_do();
  }

  file.close();
}

void loop() {

  if (first_time) {
    SerialUSB.print('\n');
    fileWrite();  // Escribimos en el fichero el momento de inicio del programa
    first_time = false;
  }

  #if defined(WITH_HYSTERESIS)
    static uint32_t last_int_ms = 0;
    static boolean int_enabled = true;
  #endif

  if (int_flag) {
    
    #if defined(WITH_HYSTERESIS)
      detachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN));
      last_int_ms = millis();
      int_enabled = false;
      int_flag = 0;
    #else
      int_flag--;
    #endif
    // En caso de recibir una interrupción externa notificamos el momento
    // añadimos una notificación de que es una interrupción externa y de cuantas restan por procesar
    printDateTime();
    SerialUSB.print(" --- interrupción externa, restantes: ");
    SerialUSB.print(int_flag);
    SerialUSB.print('\n');
    data = data + " --- externa"; 
    fileWrite();
  }

  #if defined(WITH_HYSTERESIS)
    if ((!int_enabled) && 
        (elapsedMilliseconds(last_int_ms) >= inhibitionTime_ms) &&
        (digitalRead(INTERRUPT_PIN) == LOW)) {
      attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), my_isr, FALLING);
      int_enabled = true;
    }
  #endif

  if ( _rtcFlag ) {
      
    // Imprimimos la fecha y la hora
    printDateTime();
    SerialUSB.print('\n');
    fileWrite();
    // Decrementamos _rtcFlag
    _rtcFlag--;
    if ( _rtcFlag) SerialUSB.println("WARNING: Unattended RTC alarm events!");
    
    // Apagamos el led
    digitalWrite(LED_BUILTIN, LOW);
  }

}

bool setDateTime(const char * date_str, const char * time_str)
{
  char month_str[4];
  char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
                        "Sep", "Oct", "Nov", "Dec"};
  uint16_t i, mday, month, hour, min, sec, year;

  if (sscanf(date_str, "%3s %hu %hu", month_str, &mday, &year) != 3) return false;
  if (sscanf(time_str, "%hu:%hu:%hu", &hour, &min, &sec) != 3) return false;

  for (i = 0; i < 12; i++) {
    if (!strncmp(month_str, months[i], 3)) {
      month = i + 1;
      break;
    }
  }
  if (i == 12) return false;
  
  rtc.setTime((uint8_t)hour, (uint8_t)min, (uint8_t)sec);
  rtc.setDate((uint8_t)mday, (uint8_t)month, (uint8_t)(year - 2000));
  return true;
}

// --------------------------------------------------------------------------------
// Imprime la hora y fecha en un formato internacional estándar
// --------------------------------------------------------------------------------
void printDateTime()
{

  const char *weekDay[7] = { "Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat" };
  
  // Obtenemos el tiempo Epoch, segundos desde el 1 de enero de 1970
  time_t epoch = rtc.getEpoch();

  // Covertimos a la forma habitual de fecha y hora
  struct tm stm;
  gmtime_r(&epoch, &stm);
  
  // Generamos e imprimimos la fecha y la hora
  char dateTime[32]; 
  snprintf(dateTime, sizeof(dateTime),"%s %4u/%02u/%02u %02u:%02u:%02u",
           weekDay[stm.tm_wday], 
           stm.tm_year + 1900, stm.tm_mon + 1, stm.tm_mday, 
           stm.tm_hour, stm.tm_min, stm.tm_sec);

  SerialUSB.print('\n');
  SerialUSB.print(dateTime);
  data = dateTime;
}

// --------------------------------------------------------------------------------
// Programa la alarma del RTC para que se active de en period_sec segundos a 
// partir de "offset" en segundos desde el instante actual
// --------------------------------------------------------------------------------
void setPeriodicAlarm(uint32_t period_sec, uint32_t offsetFromNow_sec)
{
  _period_sec = period_sec;
  rtc.setAlarmEpoch(rtc.getEpoch() + offsetFromNow_sec);

  // Ver enum Alarm_Match en RTCZero.h
  rtc.enableAlarm(rtc.MATCH_YYMMDDHHMMSS);
}

// --------------------------------------------------------------------------------
// Rutina de servicio asociada a la interrupción provocada por la expiración de la 
// alarma.
// --------------------------------------------------------------------------------
void alarmCallback()
{
  // Incrementamos la variable bandera
  _rtcFlag++;
  
  // Encendemos el led
  digitalWrite(LED_BUILTIN, HIGH);
  
  // Reprogramamos la alarma usando el mismo periodo 
  rtc.setAlarmEpoch(rtc.getEpoch() + _period_sec - 1);
}


// Metodo auxiliar para la escritura en el fichero
void fileWrite() {

    char data_line[50];
    data = data + " (" + line + ")\n"; // Añadimos a los datos que van a escribirse el número de línea entre '()' y un salto de línea
    data.toCharArray(data_line, 50);   // Convertimos los datos en String a un array de tipo char que será el que usaremos posteriormente para escribir en el archivo

    // Reabrimos el fichero en cada iteración para añadir datos
    File file = filesystem.open(filename,  WRITE_ONLY | APPEND);

    if (line < 11) {  // Nos aseguramos que el límite de líneas del archivo es 11
    
    if (!file) {  // Controlamos errores en la apertura del fichero para escritura
      SerialUSB.print("Abriendo fichero ");
      SerialUSB.print(filename);
      SerialUSB.print(" Apertura fallida para escritura, cerrando ...");
      on_exit_with_error_do();
    }
    
    // Escribimos una línea en el fichero por segundo
    snprintf(data_line, sizeof(data_line), data_line, millis(), ++line);
    
    int const bytes_to_write = strlen(data_line);
    int const bytes_written = file.write((void *)data_line, bytes_to_write);
    if (bytes_to_write != bytes_written) {  // Controlamos errores durante la escritura en el fichero
      SerialUSB.print("La escritura fallo con un código de error: "); 
      SerialUSB.println(filesystem.err());
      SerialUSB.println(" Cerrando ...");
      on_exit_with_error_do();
    }

    fileRead();  // Leemos el contenido que se ha escrito en el fichero

    // Cerramos el fichero
    file.close();
    delay(998);
  } else {
    fileRead();
    fileCloser(file);
  }
}

// Metodo auxiliar para la lectura del fichero
void fileRead() {
  // Abrimos el fichero para lectura
    File file = filesystem.open(filename,  READ_ONLY);
    if (!file) {    // Controlamos errores en la apertura del fichero para lectura
      SerialUSB.print("Abriendo fichero");
      SerialUSB.print(filename);
      SerialUSB.print(" Apertura fallida para lectura, cerrando ...");
      on_exit_with_error_do();
    }    
    SerialUSB.print("Leyendo contenidos del fichero:\n\t ");
    
    // Leemos el contenido del fichero hasta alcanzar la marca EOF
    while(!file.eof()) {
      char c;
      int const bytes_read = file.read(&c, sizeof(c));
      if (bytes_read) {
        SerialUSB.print(c);
        if (c == '\n') SerialUSB.print("\t ");
      }
    }

}

// Método auxilar para cerrar el fichero
void fileCloser(File file) {
  // Cerramos el fichero
    file.close();
    SerialUSB.println("\nAlcanzado maximo número de líneas. Fichero cerrado");

    // Desmontamos el sistema de archivos
    SerialUSB.println("Desmontando sistema de ficheros ... (programa finalizado)");
    filesystem.unmount();
    exit(0);
}

// --------------------------------------------------------------------------------
// Utilidad para desmontar el sistema de archivos y terminar en caso de error
// --------------------------------------------------------------------------------
void on_exit_with_error_do()
{
  filesystem.unmount();
  exit(EXIT_FAILURE);
}

// --------------------------------------------------------------------------------
// Rutina de servicio de la interrupción
// --------------------------------------------------------------------------------
void my_isr()
{
  int_flag++;
}
