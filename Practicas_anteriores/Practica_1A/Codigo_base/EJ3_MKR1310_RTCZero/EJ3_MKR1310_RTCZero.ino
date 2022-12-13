/* -------------------------------------------------------------------
 *  Ejemplo 3: Puesta en hora del RTC
 *             Lectura atómica del tiempo
 *             Conversión de Epoch al formato habitual de fecha/hora
 *  - Requiere la librería RTCZero
 *    https://www.arduino.cc/reference/en/libraries/rtczero/
 *  Asignatura (GII-IoT)
 * -------------------------------------------------------------------- 
 */
#include <time.h>
#include <RTCZero.h>

// rtc object
RTCZero rtc;

// Ponemos una hora y fecha arbitrarias
const uint8_t seconds = 50;
const uint8_t minutes = 59;
const uint8_t hours = 23;

const uint8_t day = 31;
const uint8_t month = 12;
const uint8_t year = 22;

void setup() 
{
  SerialUSB.begin(115200);

  // Habilitamos el uso del rtc
  rtc.begin();

  // Fijamos la hora y la fecha
  // Existen funciones para fijar cualquiera de estos campos de forma independiente
  rtc.setTime(hours, minutes, seconds);
  rtc.setDate(day, month, year);
}

void loop() 
{
  char dateTime[32];
  static bool useEpoch = false;

  if (!useEpoch) {
    // Generamos e imprimimos la fecha y la hora 
    snprintf(dateTime, sizeof(dateTime),"_____ %4u/%02u/%02u %02u:%02u:%02u",
             (uint16_t)rtc.getYear() + 2000, rtc.getMonth(), rtc.getDay(), 
             rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());
  }
  else {
    struct tm stm;
    // Obtenemos el tiempo en posixTime, segundos desde el 1 de enero de 1970
    time_t epoch = rtc.getEpoch();

    // Convertimos a la forma habitual de fecha y hora
    gmtime_r(&epoch, &stm);
    
    // Generamos e imprimimos la fecha y la hora 
    snprintf(dateTime, sizeof(dateTime),"EPOCH %4u/%02u/%02u %02u:%02u:%02u",
             stm.tm_year + 1900, stm.tm_mon + 1, stm.tm_mday, 
             stm.tm_hour, stm.tm_min, stm.tm_sec);
  }
           
  SerialUSB.println(dateTime);
  useEpoch = !useEpoch;

  delay(1000);
}
