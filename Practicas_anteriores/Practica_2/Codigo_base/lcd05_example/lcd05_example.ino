/* 
 * lcd05_example.ino
 * Example showing how to use the Devantech LCD05 LCD 16x2 display
 * in I2C mode. More info about this LCD display in
 *   http://www.robot-electronics.co.uk/htm/Lcd05tech.htm
 *
 * author: Antonio C. Domínguez Brito <antonio.dominguez@ulpgc.es>
 */
 
/* 
 * Arduino MKR WAN 1310 I2C pins
 *   Pin 11 -> SDA (I2C data line)
 *   Pin 12 -> SCL (I2C clock line)
 */

constexpr const char* the_msg="Hola Caracola";
 
#include <Wire.h> // Arduino's I2C library
 
#define LCD05_I2C_ADDRESS byte((0xC6)>>1)
#define LCD05_I2C_INIT_DELAY 100 // in milliseconds

// LCD05's command related definitions
#define COMMAND_REGISTER byte(0x00)
#define FIFO_AVAILABLE_LENGTH_REGISTER byte(0x00)
#define LCD_STYLE_16X2 byte(5)

// LCD05's command codes
#define CURSOR_HOME             byte(1)
#define SET_CURSOR              byte(2) // specify position with a byte in the interval 0-32/80
#define SET_CURSOR_COORDS       byte(3) // specify position with two bytes, line and column
#define HIDE_CURSOR             byte(4)
#define SHOW_UNDERLINE_CURSOR   byte(5)
#define SHOW_BLINKING_CURSOR    byte(6)
#define BACKSPACE               byte(8)
#define HORIZONTAL_TAB          byte(9) // advances cursor a tab space
#define SMART_LINE_FEED         byte(10) // moves the cursor to the next line in the same column
#define VERTICAL_TAB            byte(11) // moves the cursor to the previous line in the same column
#define CLEAR_SCREEN            byte(12)
#define CARRIAGE_RETURN         byte(13)
#define CLEAR_COLUMN            byte(17)
#define TAB_SET                 byte(18) // specify tab size with a byte in the interval 1-10
#define BACKLIGHT_ON            byte(19)
#define BACKLIGHT_OFF           byte(20) // this is the default
#define DISABLE_STARTUP_MESSAGE byte(21)
#define ENABLE_STARTUP_MESSAGE  byte(22)
#define SAVE_AS_STARTUP_SCREEN  byte(23)
#define SET_DISPLAY_TYPE        byte(24) // followed by the type, which is byte 5 for a 16x2 LCD style
#define CHANGE_ADDRESS          byte(25) // see LCD05 specification
#define CUSTOM_CHAR_GENERATOR   byte(27) // see LCD05 specification
#define DOUBLE_KEYPAD_SCAN_RATE byte(28)
#define NORMAL_KEYPAD_SCAN_RATE byte(29)
#define CONTRAST_SET            byte(30) // specify contrast level with a byte in the interval 0-255
#define BRIGHTNESS_SET          byte(31) // specify brightness level with a byte in the interval 0-255

inline void write_command(byte command)
{ Wire.write(COMMAND_REGISTER); Wire.write(command); }

void set_display_type(byte address, byte type)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(SET_DISPLAY_TYPE);
  Wire.write(type);
  Wire.endTransmission();
}

void clear_screen(byte address)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(CLEAR_SCREEN);
  Wire.endTransmission();
}

void cursor_home(byte address)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(CURSOR_HOME);
  Wire.endTransmission();
}

void set_cursor(byte address, byte pos)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(CURSOR_HOME);
  Wire.write(pos);
  Wire.endTransmission();
}

void set_cursor_coords(byte address, byte line, byte column)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(CURSOR_HOME);
  Wire.write(line);
  Wire.write(column);
  Wire.endTransmission();
}

void show_blinking_cursor(byte address)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(SHOW_BLINKING_CURSOR);
  Wire.endTransmission();
}

void backlight_on(byte address)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(BACKLIGHT_ON);
  Wire.endTransmission();
}

void backlight_off(byte address)
{
  Wire.beginTransmission(address); // start communication with LCD 05
  write_command(BACKLIGHT_OFF);
  Wire.endTransmission();
}

bool ascii_chars(byte address, byte* bytes, int length)
{
  if(length<=0) return false;
  Wire.beginTransmission(address); // start communication with LCD 05
  Wire.write(COMMAND_REGISTER);
  for(int i=0; i<length; i++, bytes++) Wire.write(*bytes);
  Wire.endTransmission();
  return true;
}

byte read_fifo_length(byte address)
{
  Wire.beginTransmission(address);
  Wire.write(FIFO_AVAILABLE_LENGTH_REGISTER);                           // Call the register for start of ranging data
  Wire.endTransmission();
  
  Wire.requestFrom(address,byte(1)); // start communication with LCD 05, request one byte
  while(!Wire.available()) { /* do nothing */ }
  return Wire.read();
}
 
bool backlight=false; 
 
// the setup routine runs once when you press reset:
void setup() 
{
  Serial.begin(9600);
  
  Serial.println("initializing Wire interface ...");
  Wire.begin();
  delay(LCD05_I2C_INIT_DELAY);  
  
  Serial.print("initializing LCD05 display in address 0x");
  Serial.print(LCD05_I2C_ADDRESS,HEX); Serial.println(" ...");
  
  set_display_type(LCD05_I2C_ADDRESS,LCD_STYLE_16X2);
  clear_screen(LCD05_I2C_ADDRESS);
  cursor_home(LCD05_I2C_ADDRESS);
  show_blinking_cursor(LCD05_I2C_ADDRESS);
  backlight_off(LCD05_I2C_ADDRESS);
}

// the loop routine runs over and over again forever:
void loop() 
{
  int fifo_length;
  while( (fifo_length=read_fifo_length(LCD05_I2C_ADDRESS))<strlen(the_msg) ) { /*do nothing*/ }
  //Serial.print("fifo length: "); Serial.println(fifo_length);
  
  ascii_chars(
    LCD05_I2C_ADDRESS,
    reinterpret_cast<byte*>(const_cast<char*>(the_msg)),
    strlen(the_msg)
  );
  
  backlight=!backlight;
  if(backlight) backlight_off(LCD05_I2C_ADDRESS);
  else backlight_on(LCD05_I2C_ADDRESS);
  
  delay(1000);
}
