#include <RTCZero.h>
#include <time.h>
#include <Arduino_MKRMEM.h>

//Sebastián Fernández García

// rtc object
RTCZero rtc;
Arduino_W25Q16DV flash(SPI1, FLASH_CS);

char filename[] = "datos_sensor.txt";  

// OJO al carácter volátil de estas variables
volatile uint32_t _period_sec = 0;
volatile uint16_t _rtcFlag = 0; //si hay interp cada 10 seg
volatile uint32_t int_flag = 0; //interp por el pin


// Macro útil para medir el tiempo transcurrido en milisegundos
#define elapsedMilliseconds(since_ms)  (uint32_t)(millis() - since_ms)
#define WITH_HYSTERESIS   
#define INTERRUPT_PIN 5

#if defined(WITH_HYSTERESIS)
  const uint32_t inhibitionTime_ms = 250;
#endif

void setup() 
{
  SerialUSB.begin(9600);

  // Habilitamos el uso del rtc
  rtc.begin();

  setDateTime(__DATE__, __TIME__); //fecha y hora compilacion

  // Fijamos la hora y la fecha
  //rtc.setTime(hours, minutes, seconds);
  //rtc.setDate(day, month, year);

  // Registramos el tiempo de comienzo
  uint32_t t_start_ms = millis();
  
  // Activamos el pin de control del LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  pinMode(LORA_RESET, OUTPUT);    // Declaramos LORA reset pin como salida
  digitalWrite(LORA_RESET, LOW);  // LO ponemos a nivel bajo para descativar el módulo LoRA
  
  //SerialUSB.begin(9600);
  while(!SerialUSB) {;}

  // Activar la alarma cada 10 segundos a partir de 0 secs
  setPeriodicAlarm(9, 0); // Puse 9 porque con 10 segundos tardaba en realidad 11 segundos

  // Apagamos el led, pero esperamos a que hayan pasado 3 segundos como mínimo desde el principio
  while(elapsedMilliseconds(t_start_ms) > 3000) { 
    delay(1); 
  }
  digitalWrite(LED_BUILTIN, LOW);

  // Limpiamos _rtcFlag
  _rtcFlag = 0;
   
  // Activar la rutina de atención
  rtc.attachInterrupt(alarmCallback);

  flash.begin();
 
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

  pinMode(INTERRUPT_PIN, INPUT_PULLUP); 
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), my_isr, FALLING);
}

void loop() 
{
  if ( _rtcFlag ) {
    // Imprimimos la fecha y la hora
    printDateTime("");

    // Decrementamos _rtcFlag
    _rtcFlag--;
    if ( _rtcFlag) SerialUSB.println("WARNING: Unattended RTC alarm events!");
    
    // Apagamos el led
    digitalWrite(LED_BUILTIN, LOW);

    static int line = 0;
    char data_line[50];

    if (line < 10) { //Vamos a dejar que se escriba 10 veces arbitrariamente en fichero y despues comprobamos
      
      // Reabrimos el fichero en cada iteración para añadir datos
      File file = filesystem.open(filename,  WRITE_ONLY | APPEND);
      if (!file) {
        SerialUSB.print("Opening file ");
        SerialUSB.print(filename);
        SerialUSB.print(" failed for appending. Aborting ...");
        on_exit_with_error_do();
      }
      
      // Escribimos una línea en el fichero por segundo (fecha)
      snprintf(data_line, sizeof(data_line),"%4u/%02u/%02u %02u:%02u:%02u \n",
           (uint16_t)rtc.getYear() + 2000, rtc.getMonth(), rtc.getDay(), 
           rtc.getHours(), rtc.getMinutes(), rtc.getSeconds(), ++line);

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
    }else {
    
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
        if (c == '\n') SerialUSB.print("\n ");
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
      printDateTime("Por interrupción externa: ");
  }

  #if defined(WITH_HYSTERESIS)
    if ((!int_enabled) && 
        (elapsedMilliseconds(last_int_ms) >= inhibitionTime_ms) &&
        (digitalRead(INTERRUPT_PIN) == LOW)) {
      attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), my_isr, FALLING);
      int_enabled = true;
    }
  #endif 
}

// --------------------------------------------------------------------------------
// Imprime la hora y fecha en un formato internacional estándar
// --------------------------------------------------------------------------------
void printDateTime(String detail)
{
  // Generamos e imprimimos la fecha y la hora 
  char dateTime[25];
  snprintf(dateTime, sizeof(dateTime),"%4u/%02u/%02u %02u:%02u:%02u",
           (uint16_t)rtc.getYear() + 2000, rtc.getMonth(), rtc.getDay(), 
           rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());
           
  SerialUSB.println(detail + dateTime);
}

// --------------------------------------------------------------------------------
// Programa la alarma del RTC para que se active de en period_sec segundos a 
// partir de "offset" en segundos desde el instante actual
// --------------------------------------------------------------------------------
void setPeriodicAlarm(uint32_t period_sec, uint32_t offsetFromNow_sec)
{
  //startMillis = millis();  //initial start time
  _period_sec = period_sec;
  rtc.setAlarmEpoch(rtc.getEpoch() + offsetFromNow_sec);

  // Ver enum Alarm_Match en RTCZero.h
  rtc.enableAlarm(rtc.MATCH_YYMMDDHHMMSS);
  //currentMillis = millis(); //final time
  //SerialUSB.println("EL TIEMPO DIF ES:" +)
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

// --------------------------------------------------------------------------------
// Utilidad para desmontar el sistema de archivos y terminar en caso de error
// --------------------------------------------------------------------------------
void on_exit_with_error_do()
{
  filesystem.unmount();
  exit(EXIT_FAILURE);
}

void my_isr()
{
  int_flag++;
}

bool setDateTime(const char * date_str, const char * time_str) {
  char month_str[4];
  char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
                          "Sep", "Oct", "Nov", "Dec"};
    uint16_t i, mday, month, hour, min, sec, year;
  if (sscanf(date_str, "%3s %hu %hu", month_str, &mday, &year) != 3) return false; if (sscanf(time_str, "%hu:%hu:%hu", &hour, &min, &sec) != 3) return false;
    for (i = 0; i < 12; i++) {
      if (!strncmp(month_str, months[i], 3)) {
  month = i + 1;
  break; }
    }
    if (i == 12) return false;
    rtc.setTime((uint8_t)hour, (uint8_t)min, (uint8_t)sec);
    rtc.setDate((uint8_t)mday, (uint8_t)month, (uint8_t)(year - 2000));
    return true;
}