/* ----------------------------------------------------------------------------
 *  Ejemplo 4: Puesta en hora del RTC usando la fecha y hora de la compilación
 *             Lectura atómica del tiempo
 *             Conversión de Epoch al formato habitual de fecha/hora
 *  - Requiere la librería RTCZero
 *    https://www.arduino.cc/reference/en/libraries/rtczero/
 *  Asignatura (GII-IoT)
 * ----------------------------------------------------------------------------- 
 */
#include <time.h>
#include <RTCZero.h>

// rtc object
RTCZero rtc;

void setup() 
{
  SerialUSB.begin(115200);
  while(!SerialUSB) {;}

  SerialUSB.print(__DATE__);
  SerialUSB.print(" ");
  SerialUSB.println(__TIME__);

  // Habilitamos el uso del rtc
  rtc.begin();

  // Analizamos las dos cadenas para extraer fecha y hora y fijar el RTC
  if (!setDateTime(__DATE__, __TIME__))
  {
    SerialUSB.println("setDateTime() failed!\nExiting ...");
    while (1) { ; }
  }
}

void loop()
{
  char dateTime[32];
  struct tm stm;
  // Obtenemos el tiempo en posixTime, segundos desde el 1 de enero de 1970
  time_t epoch = rtc.getEpoch();

  // Convertimos a la forma habitual de fecha y hora
  gmtime_r(&epoch, &stm);
  
  // Generamos e imprimimos la fecha y la hora 
  snprintf(dateTime, sizeof(dateTime),"EPOCH %4u/%02u/%02u %02u:%02u:%02u",
           stm.tm_year + 1900, stm.tm_mon + 1, stm.tm_mday, 
           stm.tm_hour, stm.tm_min, stm.tm_sec);

  SerialUSB.println(dateTime);
  delay(1000);
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
