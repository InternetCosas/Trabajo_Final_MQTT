/* ----------------------------------------------------------------------
 *  Supervisor
 *    Arduino supervisor
 *    
 *  María Naranjo Almeida
 * ---------------------------------------------------------------------- 
 */

constexpr const uint32_t serial_monitor_bauds=115200;
constexpr const uint32_t serial1_bauds=9600;

constexpr const uint32_t pseudo_period_ms=1000;

uint8_t counter=0;
uint8_t led_state=LOW;

byte code;
byte device;
byte quantity;

byte message[4];

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
  if(Serial.available()>0){
    String input = Serial.readString();
    Serial.println(input);
    readSerial(input);
  }

  digitalWrite(LED_BUILTIN,led_state); led_state=(led_state+1)&0x01;
}

/* ----------------------------------
 * Función que escribe en el serial1
 * los códigos de los comandos
 *
 *
 * ----------------------------------
*/
void readSerial(String text){
  String words[4];
  words[0] = getValue(text, ' ', 0);
  words[1] = getValue(text, ' ', 1);
  words[2] = getValue(text, ' ', 2);
  words[3] = getValue(text, ' ', 3);

  if (words[0] == "help\n" || words[0] == "HELP\n" || words[0] == "Help\n"){
    code = byte(0); //Código
    message[0] = code;
    Serial1.write(message, 4);

  } else if (words[0] == "us\n" || words[0] == "US\n" || words[0] == "Us\n"){
    if(words[1] == NULL){ //Si es solo el comando us
      code = byte(1); //Código
      message[0] = code;
      Serial1.write(message, 4);
    }
    
  } else if(words[0] == "us" || words[0] == "US" || words[0] == "Us") {
    int third = words[3].toInt();
    if(words[1] == NULL){ //Si es solo el comando us
      code = byte(1); //Código
      message[0] = code;
      Serial1.write(message, 4);
      
    } else if (words[2] == "one-shot\n" && (words[1] == "srf02_1" ||words[1] == "srf02_2" || words[1] == "0x75" || words[1] == "0x77") ){
      code = byte(2);

      if (words[1] == "srf02_1"|| words[1] == "0x75" ) {
        device = byte(75);
      } else if (words[1] == "srf02_2" || words[1] == "0x77"){
        device = byte(77);
      }

      message[0] = code;
      message[1] = device;
      Serial1.write(message, 4);

    } else if(words[2] == "on\n" && (words[1] == "srf02_1" ||words[1] == "srf02_2" || words[1] == "0x75" || words[1] == "0x77") && isDigit(third)){
       code = byte(3);

       if (words[1] == "srf02_1"|| words[1] == "0x75" ) {
        device = byte(75);
      } else if (words[1] == "srf02_2" || words[1] == "0x77"){
        device = byte(77);
      }

      quantity = byte(third);      

      message[0] = code;
      message[1] = device;
      message[2] = quantity;
      Serial1.write(message, 4);

    } else if(words[2] == "unit\n"){
      if(words[1] == "srf02_1" ||words[1] == "srf02_2" || words[1] == "0x75" || words[1] == "0x77"){
        code = byte(4);
        if (words[1] == "srf02_1"|| words[1] == "0x75" ) {
          device = byte(75);
        } else if (words[1] == "srf02_2" || words[1] == "0x77"){
          device = byte(77);
        }
      }

      if (words[3] == "inc"){
        quantity = byte(0);
      } else if (words[3] == "cm"){
        quantity = byte(1);
      } else if (words[3] == "ms") {
        quantity = byte(2);
      } else {
        //Mandar mensaje de "Unidad equivocada"
      }
      message[0] = code;
      message[1] = device;
      message[2] = quantity;
      Serial1.write(message, 4);

    } else if(words[2] == "status\n"){
      if(words[1] == "srf02_1" ||words[1] == "srf02_2" || words[1] == "0x75" || words[1] == "0x77"){
        code = byte(5);
        if (words[1] == "srf02_1"|| words[1] == "0x75" ) {
          device = byte(75);
        } else if (words[1] == "srf02_2" || words[1] == "0x77"){
          device = byte(77);
        }
      }
    
    } else {
      //Mandar mensaje de error personalizado y replicarlo en el resto de casos
    }

  }
    
     else {
      code = byte(255); //Código
      message[0] = code;
      Serial1.write(message, 4);
    }

    else {
    code = byte(255); //Código
    message[0] = code;
    Serial1.write(message, 4);

  }

}

/* ----------------------------------
 * Función que separa una string
 * según un separador pasado por parámetro
 * y devuelve la string que se encuentra en 
 * dicha posición
 *
 * ----------------------------------
*/
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}
