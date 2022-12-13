/* ------------------------------------------------------------------------------
 *  Ejemplo 6: Uso del watchdog del SAMD21G
 *  
 *     Usa la librería WDTZero
 *     https://github.com/javos65/WDTZero
 *  
 *  Asignatura (GII-IoT)
 * ------------------------------------------------------------------------------- 
 */

#include <WDTZero.h>

#define WDT_SOFT  false // true

#define timeoutExpired_ms(start_ms, timeout_ms)  \
                         (((millis() - timeout_ms) <= start_ms) ? false : true)
                    
// Declaramos el objeto para manejear el watchdog
WDTZero wdt;

// --------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------
void setup() 
{
  SerialUSB.begin(115200);
  while(!SerialUSB) {;}

  wdt.attachShutdown(shutdownCallback);

#if WDT_SOFT

  // Inicializamos el WDT en modo "soft" (Early Warning). 
  SerialUSB.println("\n\nWDTZero-Demo : Setup Soft Watchdog at 32S interval"); 
  wdt.setup(WDT_SOFTCYCLE32S);
  
#else

  // Inicializamos el WDT en modo "hard". 
  SerialUSB.println("\n\nWDTZero-Demo : Setup Hard Watchdog at 16S interval");  
  wdt.setup(WDT_HARDCYCLE16S);
  
#endif

}

// --------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------
void loop() 
{
  static int cycle = 0;
  static uint32_t lapse_init_time_ms;

  SerialUSB.print("\nThis is cycle: ");
  SerialUSB.println(cycle);
  SerialUSB.print("WDTZeroCounter: ");
  SerialUSB.println(WDTZeroCounter);
  
  for (int i = 0; i < 60; ) {
    lapse_init_time_ms = millis();
    
    // Refrescamos el watchdog para impedir que salte
    if (!cycle) wdt.clear();

    if (i < 10) SerialUSB.print(0);
    SerialUSB.print(i);
    SerialUSB.print(" ");
    
    if ( !(++i % 10) ) {
      SerialUSB.print("  WDTZeroCounter: ");
      SerialUSB.println(WDTZeroCounter);
    }
    while (!timeoutExpired_ms(lapse_init_time_ms, 1000))  delay(1);
  }
  cycle++;
}

// --------------------------------------------------------------------------------
// Función que se invoca con anterioridad al reset en el modo "soft"
// --------------------------------------------------------------------------------
void shutdownCallback()
{
  SerialUSB.println("\nWatchdog will reset the board now ...");
}
