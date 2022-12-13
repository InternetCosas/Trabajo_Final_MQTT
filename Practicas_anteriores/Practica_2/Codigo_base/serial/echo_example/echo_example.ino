/* ----------------------------------------------------------------------
 *  Ejemplo echo.ino 
 *    Este ejemplo muestra como utilizar el puerto serie uart (Serial1) 
 *    para comunicarse con otro dispositivo.
 *    
 *  Asignatura (GII-IC)
 * ---------------------------------------------------------------------- 
 */

constexpr const uint32_t serial_monitor_bauds=115200;
constexpr const uint32_t serial1_bauds=9600;

constexpr const uint32_t pseudo_period_ms=1000;

uint8_t led_state=LOW;

void setup()
{
  // Configuración del LED incluido en placa
  // Inicialmente apagado
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,led_state); led_state=(led_state+1)&0x01;
  
  // Inicialización del puerto para el serial monitor 
  Serial.begin(serial_monitor_bauds);
  while (!Serial);

  // Inicialización del puerto de comunicaciones con el otro dispositivo MKR 
  Serial1.begin(serial1_bauds);
}

void loop()
{
  Serial.println("******************** echo example *********************"); 

  uint32_t last_ms=millis();
  while(millis()-last_ms<pseudo_period_ms) 
  { 
    if(Serial1.available()>0) 
    {
      uint8_t data=Serial1.read();
      Serial.print("<-- received: "); Serial.println(static_cast<int>(data)); 
      Serial.print("--> sending back: "); Serial.println(static_cast<int>(data)); 
      Serial1.write(data);
      break;
    }
  }

  if(millis()-last_ms<pseudo_period_ms) delay(pseudo_period_ms-(millis()-last_ms));
  else Serial.println("<-- received: TIMEOUT!!"); 

  Serial.println("*******************************************************"); 

  digitalWrite(LED_BUILTIN,led_state); led_state=(led_state+1)&0x01;
}
