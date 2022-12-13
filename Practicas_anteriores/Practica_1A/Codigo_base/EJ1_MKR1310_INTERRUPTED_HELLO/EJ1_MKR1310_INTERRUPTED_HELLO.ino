/* 
 * --------------------------------------------------------------------------------
 *  Ejemplo 1:
 *  - Un ejemplo sencillo sobre cómo es posible programar una rutina de atención 
 *    de interrupciones
 *  
 * --------------------------------------------------------------------------------
 */
// Descomentar para impedir múltiples interrupciones por rebote 
#define WITH_HYSTERESIS   

#if defined(WITH_HYSTERESIS)
  const uint32_t inhibitionTime_ms = 250;
#endif

#define ellapsedTime_ms(since_ms) (uint32_t)(millis() - since_ms)

#define INTERRUPT_PIN     5

// Declaramos la variable bandera como volátil y la inicializamos
volatile uint32_t int_flag = 0;

// --------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------
void setup() 
{
  SerialUSB.begin(9600);
  while(!SerialUSB) {;}

  // Ajustamos el modo del INTERRUPT_PIN
  // Lo configuramos en modo pullup para que se produzca una
  // interrupción por flanco de bajada (FALLING) al llevarl a tierra
  pinMode(INTERRUPT_PIN, INPUT_PULLUP); 
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), my_isr, FALLING);

  SerialUSB.println("\nLooping ...");
}

// --------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------
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
    SerialUSB.print("At ");
    SerialUSB.print(millis());
    SerialUSB.print(": Hello world!");
    SerialUSB.print(" --- remaining: ");
    SerialUSB.println(int_flag);
  }

  #if defined(WITH_HYSTERESIS)
    if ((!int_enabled) && 
        (ellapsedTime_ms(last_int_ms) >= inhibitionTime_ms) &&
        (digitalRead(INTERRUPT_PIN) == LOW)) {
      attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), my_isr, FALLING);
      int_enabled = true;
    }
  #endif 
}

// --------------------------------------------------------------------------------
// Rutina de servicio de la interrupción
// --------------------------------------------------------------------------------
void my_isr()
{
  int_flag++;
}
