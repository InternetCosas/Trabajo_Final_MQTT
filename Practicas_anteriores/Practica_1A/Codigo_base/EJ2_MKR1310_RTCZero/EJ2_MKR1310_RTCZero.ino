/* -----------------------------------------------------------
 *  Ejemplo 2: Puesta en hora del RTC
 *  - Requiere la librería RTCZero
 *    https://www.arduino.cc/reference/en/libraries/rtczero/
 *  Asignatura (GII-IoT)
 * ----------------------------------------------------------- 
 */
  
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
  // Generamos e imprimimos la fecha y la hora 
  char dateTime[25];
  snprintf(dateTime, sizeof(dateTime),"%4u/%02u/%02u %02u:%02u:%02u",
           (uint16_t)rtc.getYear() + 2000, rtc.getMonth(), rtc.getDay(), 
           rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());
           
  SerialUSB.println(dateTime);

  delay(1000);
}
