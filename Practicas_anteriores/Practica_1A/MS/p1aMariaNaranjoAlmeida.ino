/* 
 *  --------------------------------------------------------------------------------
 *  Práctica 1a
 *  Realizada por: María Naranjo Almeida
 *--------------------------------------------------------------------------------
 */

#include <time.h>
#include <RTCZero.h>
#include <Arduino_MKRMEM.h>

RTCZero rtc;
Arduino_W25Q16DV flash(SPI1, FLASH_CS);
char filename[] = "datos.txt";

//Variables volátiles para la rutina de servicio de la alarma
volatile uint32_t _period_sec = 0;
volatile uint32_t _rtcFlag = 0;

// Declaramos la variable bandera como volátil y la inicializamos para la interrupción del pin 5
volatile uint32_t int_flag = 0;

// Macro útil para medir el tiempo transcurrido en milisegundos.
#define elapsedMilliseconds(since_ms)  (uint32_t)(millis() - since_ms)

#define WITH_HYSTERESIS   

#if defined(WITH_HYSTERESIS)
  const uint32_t inhibitionTime_ms = 250;
#endif

#define ellapsedTime_ms(since_ms) (uint32_t)(millis() - since_ms)

#define INTERRUPT_PIN     5

void setup() 
{

  uint32_t t_start_ms = millis();

  pinMode(LORA_RESET, OUTPUT);    // Declaramos LORA reset pin como salida
  digitalWrite(LORA_RESET, LOW);  // Lo ponemos a nivel bajo para desactivar el módulo 
                                  // LoRA

  // Activamos el pin de control del LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  //Activamos Pin5 y le ponemos la interrupción
  pinMode(INTERRUPT_PIN, INPUT_PULLUP); 
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), my_isr, FALLING);

  
  SerialUSB.begin(115200);
  while(!SerialUSB) {;}

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

  // Creamos o truncamos un nuevo fichero
  File file = filesystem.open(filename, CREATE | TRUNCATE) ;
  if (!file) {
    SerialUSB.print("Creation of file ");
    SerialUSB.print(filename);
    SerialUSB.print(" failed. Aborting ...");
    on_exit_with_error_do();
  }
  file.close();

  // Habilitamos el uso del rtc
  rtc.begin();

  // Analizamos las dos cadenas para extraer fecha y hora y fijar el RTC
  if (!setDateTime(__DATE__, __TIME__))
  {
    SerialUSB.println("setDateTime() failed!\nExiting ...");
    while (1) { ; }
  }

  // Activar la alarma cada 10 segundos a partir de 5 secs
   setPeriodicAlarm(10, 5);

   // Apagamos el led, pero esperamos a que hayan pasado 3 segundos 
   // como mínimo desde el principio
   while(elapsedMilliseconds(t_start_ms) > 3000) { delay(1); }
   digitalWrite(LED_BUILTIN, LOW);

   // Limpiamos _rtcFlag
   _rtcFlag = 0;
   
   // Activar la rutina de atención
   rtc.attachInterrupt(alarmCallback);


}

void loop()
{
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
    writeFile("Interrupcion en pin 5 \n");
  }

  #if defined(WITH_HYSTERESIS)
    if ((!int_enabled) && 
        (ellapsedTime_ms(last_int_ms) >= inhibitionTime_ms) &&
        (digitalRead(INTERRUPT_PIN) == LOW)) {
      attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), my_isr, FALLING);
      int_enabled = true;
    }
  #endif 

    // Escribimos una línea en el fichero
  if ( _rtcFlag ) {
      
    // Imprimimos la fecha y la hora y la escribimos en el fichero
    printDateTime();

    // Decrementamos _rtcFlag
    _rtcFlag--;
    if ( _rtcFlag) SerialUSB.println("WARNING: Unattended RTC alarm events!");
    
    // Apagamos el led
    digitalWrite(LED_BUILTIN, LOW);
  }
}

// ---------------------------------------------
// Rutina de servicio de interrupción del pin 5
// ---------------------------------------------
void my_isr(){
  int_flag++;
}

// -------------------------------------------------------
// Rutina para escribir en un fichero, pasando la string a
// escribir como parámetro
// -------------------------------------------------------

void writeFile(const char * msg){
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
    

    SerialUSB.print("Writing at ");
    SerialUSB.print(millis());
    SerialUSB.print(":\n");    
    SerialUSB.print(msg);
    
    int const bytes_to_write = strlen(msg);
    int const bytes_written = file.write((void *)msg, bytes_to_write);
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

bool setDateTime(const char * date_str, const char * time_str)
{
  char month_str[4];
  char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
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
  static int line = 0;
  char data_line[50];

  const char *weekDay[7] = { "Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat" };
  
  // Obtenemos el tiempo Epoch, segundos desde el 1 de enero de 1970
  time_t epoch = rtc.getEpoch();

  // Covertimos a la forma habitual de fecha y hora
  struct tm stm;
  gmtime_r(&epoch, &stm);
  
  // Generamos e imprimimos la fecha y la hora
  char dateTime[32]; 
  snprintf(dateTime, sizeof(dateTime),"%s %4u/%02u/%02u %02u:%02u:%02u \n",
           weekDay[stm.tm_wday], 
           stm.tm_year + 1900, stm.tm_mon + 1, stm.tm_mday, 
           stm.tm_hour, stm.tm_min, stm.tm_sec);

  writeFile(dateTime);
   
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
  rtc.setAlarmEpoch(rtc.getEpoch() + _period_sec);
}

void on_exit_with_error_do()
{
  filesystem.unmount();
  exit(EXIT_FAILURE);
}
