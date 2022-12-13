/* ----------------------------------------------------------------------
 *  Ejemplo ChibiOS-1
 *    Este ejemplo ilustra cómo es posible crear varia hebras de forma
 *    estática. Permite experimentar con el uso de un esquema
 *    cooperativo de despacho de hebras de igual prioridad u otro 
 *    no cooperativo (round-robin con desplazamiento)
 *    
 *    Requiere el uso de la librería ChRt de Bill Greiman
 *        https://github.com/greiman/ChRt
 *
 *  Asignatura (GII-IoT)
 * ---------------------------------------------------------------------- 
 */

#include "ChRt.h"

// El uso de un esquema ROUND_ROBIN requiere fijar esta macro a false
#define COOPERATIVE_SCHEDULER   true

volatile uint32_t count = 0;

#if COOPERATIVE_SCHEDULER

  volatile uint32_t maxDelay_i = 0;

  #define BLINKING_THD_PRIO     NORMALPRIO
  #define COUNTING_THD_PRIO     NORMALPRIO
  #define PRINTING_THD_PRIO     NORMALPRIO

#else

  #define BLINKING_THD_PRIO     (NORMALPRIO + 1)
  #define PRINTING_THD_PRIO     (NORMALPRIO)
  #define COUNTING_THD_PRIO     (NORMALPRIO - 1)

#endif

// ----------------------------------------------------------------------
// BlinkingThread - Hace parpadear el LED
//                  Prioridad:  BLINKING_THD_PRIO
//                  STACK: 64 bytes
// ----------------------------------------------------------------------
static THD_WORKING_AREA(wa_BlinkingThread, 64);

static THD_FUNCTION(BlinkingThread , arg) 
{
  (void)arg;

  // LED_BUILTIN = 13 (predefinido)
  pinMode(LED_BUILTIN, OUTPUT);

  // Flash led every 200 ms.
  while (true) {
    // Turn LED on.
    digitalWrite(LED_BUILTIN, HIGH);

    // Sleep for 50 milliseconds.
    chThdSleepMilliseconds(50);

    // Turn LED off.
    digitalWrite(LED_BUILTIN, LOW);

    // Sleep for 150 milliseconds.
    chThdSleepMilliseconds(150);
  }
}
// ----------------------------------------------------------------------
// PrinterThread - Imprime el valor del contador
//                 Prioridad: PRINTING_THD_PRIO
//                 STACK: 256 bytes
// ----------------------------------------------------------------------
static THD_WORKING_AREA(wa_PrinterThread, 256);

static THD_FUNCTION(PrinterThread , arg) 
{
  (void)arg;

  while (true) {
    // Dormimos durante un segundo
    chThdSleepMilliseconds(1000);
    
    // Imprime el conteo del segundo anterior 
    #if COOPERATIVE_SCHEDULER == false
    
      SerialUSB.print("Count: ");
      SerialUSB.println(count);
      
      // Ponemos a cero el contador.
      count = 0;
          
    #else
    
      SerialUSB.print("Count: ");
      SerialUSB.print(count);
      SerialUSB.print(" Max delay(usec): ");
      SerialUSB.println(TIME_I2US(maxDelay_i));
    
      // Ponemos a cero el contador.
      count = 0;
      maxDelay_i = 0;
    #endif
  }
}

// ----------------------------------------------------------------------
// CountingThread - Hebra contadora
//                  Prioridad: COUNTING_THD_PRIO
//                  STACK: 64 bytes
// ----------------------------------------------------------------------
static THD_WORKING_AREA(wa_CountingThread, 64);

static THD_FUNCTION(CountingThread , arg) 
{
  (void)arg;

  while (true) {
    noInterrupts();
    count++;
    interrupts();

    #if COOPERATIVE_SCHEDULER
      systime_t t_i = chVTGetSystemTimeX();
      chThdYield();
      t_i = chVTTimeElapsedSinceX(t_i);
      if (t_i > maxDelay_i) maxDelay_i = t_i;
    #endif
  }
}

// ----------------------------------------------------------------------
// Creamos tres hebras 
// ----------------------------------------------------------------------
void chSetup() 
{
  // Aquí asumimos que el valor de CH_CFG_ST_TIMEDELTA es siempre cero
  // para cualquier tarjeta basada en SAMD que están soportadas solo en
  // "tick mode"
  
  // Verificamos primero la configuración de ChibiOS
  if (COOPERATIVE_SCHEDULER) {
    if (CH_CFG_TIME_QUANTUM) {
      SerialUSB.println("You must set CH_CFG_TIME_QUANTUM zero in");
      #if defined(__arm__)
          SerialUSB.print("src/<board type>/chconfig<board>.h");
      #elif defined(__AVR__)
          SerialUSB.print("src/avr/chconfig_avr.h"); 
      #endif 
      SerialUSB.println(" to enable cooperative scheduling.");
      while (true) {}
    } 
  }
  else {
    if (CH_CFG_TIME_QUANTUM == 0) {
      SerialUSB.println("You must set CH_CFG_TIME_QUANTUM to a non-zero value in");
      #if defined(__arm__)
          SerialUSB.print("src/<board type>/chconfig<board>.h");
      #elif defined(__AVR__)
          SerialUSB.print("src/avr/chconfig_avr.h"); 
      #endif 
      SerialUSB.println(" to enable round-robin scheduling.");
      while (true) {}
    } 
  }
  
  // Lanzamos la hebra que controla el LED. 
  // Prioridad: BLINKING_THD_PRIO
  chThdCreateStatic(wa_BlinkingThread, sizeof(wa_BlinkingThread),
                    BLINKING_THD_PRIO, BlinkingThread, NULL);
                    
  // Lanzamos la hebra que imorime el conteo. 
  // Prioridad: PRINTING_THD_PRIO.
  chThdCreateStatic(wa_PrinterThread, sizeof(wa_PrinterThread),
                    PRINTING_THD_PRIO, PrinterThread, NULL);
                   
  // Lanzamos la hebra de menor prioridad (CountThread).  
  // Prioridad: COUNTING_THD_PRIO
  chThdCreateStatic(wa_CountingThread, sizeof(wa_CountingThread),
                    COUNTING_THD_PRIO, CountingThread, NULL);
}

// ----------------------------------------------------------------------
// Set up puede usarse para activar los dispositivos que vayan a 
// emplearse
// ----------------------------------------------------------------------
void setup() 
{
  SerialUSB.begin(115200);
  
  // Wait for USB Serial.
  while (!SerialUSB) {}

  // chBegin() resetea el stack y nunca retorna.
  chBegin(chSetup);
}

// ----------------------------------------------------------------------
// El entorno Arduino exige la declaración de esta función que se trata
// como una hebra de prioridad normal (NORMALPRIO)
//
// NUNCA usar delay() para imponer un retardo. Si lo hacemos, impediremos
// la ejecución de las hebras de menor prioridad. Esto es fácil de comprobar
// en este ejemplo sustituyendo la llamada a chThdSleepMilliseconds() por
// delay(). Usar siempre chThdSleepMilliseconds() o alguna función 
// equivalente
// ----------------------------------------------------------------------
void loop() 
{ 
  SerialUSB.println("Loop");
  chThdSleepMilliseconds(5000);
  //delay(5000);
}
