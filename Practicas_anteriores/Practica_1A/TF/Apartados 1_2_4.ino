#include <time.h>
#include <RTCZero.h>

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

void setup() {

  // Registramos el tiempo de comienzo
  uint32_t t_start_ms = millis();

  // Activamos el pin de control del LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

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
}

void loop() {

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
    //En caso de recibir una interrupción externa notificamos el momento
    //añadimos una notificación de que es una interrupción externa y de cuantas restan por procesar 
    printDateTime();
    SerialUSB.print(" --- interrupción externa, restantes: ");
    SerialUSB.print(int_flag);
    SerialUSB.print('\n');
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

  SerialUSB.print(dateTime);
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

// --------------------------------------------------------------------------------
// Rutina de servicio de la interrupción
// --------------------------------------------------------------------------------
void my_isr()
{
  int_flag++;
}