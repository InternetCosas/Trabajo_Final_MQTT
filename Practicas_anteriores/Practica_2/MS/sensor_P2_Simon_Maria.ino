/* ----------------------------------------------------------------------
 *  Sensor 
 *    Arduino sensor
 *    
 *  María Naranjo Almeida
 * ---------------------------------------------------------------------- 
 */

 #include <Wire.h> // Arduino's I2C library

constexpr const uint32_t serial_monitor_bauds=115200;
constexpr const uint32_t serial1_bauds=9600;

constexpr const uint32_t pseudo_period_ms=1000;

uint8_t led_state=LOW;
int data_length = 4;
byte data[4];


//Primer sensor
#define SRF02_1_I2C_ADDRESS byte((0xEA)>>1)
#define SRF02_1_I2C_INIT_DELAY 100 // in milliseconds
#define SRF02_1_RANGING_DELAY 70 // milliseconds

// LCD05's command related definitions
#define COMMAND_REGISTER byte(0x00)
#define SOFTWARE_REVISION byte(0x00)
#define RANGE_HIGH_BYTE byte(2)
#define RANGE_LOW_BYTE byte(3)
#define AUTOTUNE_MINIMUM_HIGH_BYTE byte(4)
#define AUTOTUNE_MINIMUM_LOW_BYTE byte(5)


//Segundo sensor 
#define SRF02_2_I2C_ADDRESS byte((0xEE)>>1)
#define SRF02_2_I2C_INIT_DELAY 100 // in milliseconds
#define SRF02_2_RANGING_DELAY 70 // milliseconds


// SRF02's command codes
#define REAL_RANGING_MODE_INCHES    byte(80)
#define REAL_RANGING_MODE_CMS       byte(81)
#define REAL_RANGING_MODE_USECS     byte(82)
#define FAKE_RANGING_MODE_INCHES    byte(86)
#define FAKE_RANGING_MODE_CMS       byte(87)
#define FAKE_RANGING_MODE_USECS     byte(88)
#define TRANSMIT_8CYCLE_40KHZ_BURST byte(92)
#define FORCE_AUTOTUNE_RESTART      byte(96)
#define ADDRESS_CHANGE_1ST_SEQUENCE byte(160)
#define ADDRESS_CHANGE_3RD_SEQUENCE byte(165)
#define ADDRESS_CHANGE_2ND_SEQUENCE byte(170)

inline void write_command(byte address,byte command)
{ 
  Wire.beginTransmission(address);
  Wire.write(COMMAND_REGISTER); 
  Wire.write(command); 
  Wire.endTransmission();
}

byte read_register(byte address,byte the_register)
{
  Wire.beginTransmission(address);
  Wire.write(the_register);
  Wire.endTransmission();
  
  // getting sure the SRF02 is not busy
  Wire.requestFrom(address,byte(1));
  while(!Wire.available()) { /* do nothing */ }
  return Wire.read();
} 

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

  // Inicialización de los us
  Wire.begin();
  delay(SRF02_1_I2C_INIT_DELAY);  
  delay(SRF02_2_I2C_INIT_DELAY);  
   
}

void loop()
{
  
  
  if(Serial1.available()>0) 
  {
    Serial1.readBytes(data, data_length);
    Serial.print("Código: ");
    Serial.println(data[0]);
    if(data[0] == byte(0)){ //help
      Serial.println("Los comandos aceptados son:\n" 
      " - us <srf02> {one-shot | on <period_ms> | off}\n" 
      " - us <srf02> unit {inc | cm | ms}\n - us <srf02> delay <ms>\n"
      " - us <srf02> status\n" 
      " - us");
    } else if(data[0] == byte(255)){
      
      Serial.println("ERROR -> Comando no reconocido");
    } else if (data[0] == byte(1)){
      byte software_revision1=read_register(SRF02_1_I2C_ADDRESS,SOFTWARE_REVISION);
      Serial.print("SFR02_1 ultrasonic range finder in address 0x");
      Serial.print(SRF02_1_I2C_ADDRESS,HEX); Serial.print("(0x");
      Serial.print(software_revision1,HEX); Serial.println(")");

      byte software_revision2=read_register(SRF02_2_I2C_ADDRESS,SOFTWARE_REVISION);
      Serial.print("SFR02_2 ultrasonic range finder in address 0x");
      Serial.print(SRF02_2_I2C_ADDRESS,HEX); Serial.print("(0x");
      Serial.print(software_revision2,HEX); Serial.println(")");

    } else if (data[0] == byte(2)){ //Si es us <srf02_x> one-shot
      Serial.print("ranging ");
      if (data[1] == 75){
        Serial.print("srf02_1...");
        write_command(SRF02_1_I2C_ADDRESS,REAL_RANGING_MODE_CMS); //Cambiar [num]
        delay(70);
        
        byte high_byte_range=read_register(SRF02_1_I2C_ADDRESS,RANGE_HIGH_BYTE);
        byte low_byte_range=read_register(SRF02_1_I2C_ADDRESS,RANGE_LOW_BYTE);
        byte high_min=read_register(SRF02_1_I2C_ADDRESS,AUTOTUNE_MINIMUM_HIGH_BYTE);
        byte low_min=read_register(SRF02_1_I2C_ADDRESS,AUTOTUNE_MINIMUM_LOW_BYTE);
        
        Serial.print(int((high_byte_range<<8) | low_byte_range)); Serial.print(" cms. (min=");
        Serial.print(int((high_min<<8) | low_min)); Serial.println(" cms.)");
        
        delay(1000);
      
      } else {
        Serial.print("srf02_2...");
        write_command(SRF02_2_I2C_ADDRESS,REAL_RANGING_MODE_CMS); //Cambiar [num]
        delay(70);
        
        byte high_byte_range=read_register(SRF02_2_I2C_ADDRESS,RANGE_HIGH_BYTE);
        byte low_byte_range=read_register(SRF02_2_I2C_ADDRESS,RANGE_LOW_BYTE);
        byte high_min=read_register(SRF02_2_I2C_ADDRESS,AUTOTUNE_MINIMUM_HIGH_BYTE);
        byte low_min=read_register(SRF02_2_I2C_ADDRESS,AUTOTUNE_MINIMUM_LOW_BYTE);
        
        Serial.print(int((high_byte_range<<8) | low_byte_range)); Serial.print(" cms. (min=");
        Serial.print(int((high_min<<8) | low_min)); Serial.println(" cms.)");
        
        delay(1000);

      }
      
    } else if(data[0] == byte(3)){
      //Lo de on
    } else if(data[0] == byte(4)){ //unit
      if (data[1] == 75){
        if(data[2] == 0){ //inc

        } else if (data[2] == 1){ // cm

        } else { // ms

        }

      } else {

      }      
    } else if(data[0] == byte(5)){ //status
      if(data[1] == 75){
        byte software_revision1=read_register(SRF02_1_I2C_ADDRESS,SOFTWARE_REVISION);
        Serial.print("SFR02_1 ultrasonic range finder in address 0x");
        Serial.print(SRF02_1_I2C_ADDRESS,HEX); Serial.print("(0x");
        Serial.print(software_revision1,HEX); Serial.println(")");
      } else {
        byte software_revision2=read_register(SRF02_2_I2C_ADDRESS,SOFTWARE_REVISION);
        Serial.print("SFR02_2 ultrasonic range finder in address 0x");
        Serial.print(SRF02_2_I2C_ADDRESS,HEX); Serial.print("(0x");
        Serial.print(software_revision2,HEX); Serial.println(")");
      }
    }
    
  }
  

  digitalWrite(LED_BUILTIN,led_state); led_state=(led_state+1)&0x01;
}

void us(byte address){

}
