/* ======================================================================= *\
 *  Ejemplo ChibiOS-3
 *  Este ejemplo muestra cómo estimar la carga computacional de cada hebra
 *  
 *  IMPORTANTE: en ChRt/src/rt/templates/chconf.h
 *    - CH_DBG_THREADS_PROFILING debe activarse (TRUE) 
 *    - CH_CFG_NO_IDLE_THREAD debe activarse (TRUE)
 *    
 *  Requiere el uso de la librería ChRt de Bill Greiman
 *    https://github.com/greiman/ChRt
 *    
 *  Asignatura (GII-IoT)
\* ======================================================================= */ 
#include <ChRt.h>
#include <math.h>

//------------------------------------------------------------------------------
// Parametrization
//------------------------------------------------------------------------------
#define USE_DOUBLE    FALSE   // Change to TRUE to use double precision (heavier)

#define CYCLE_MS      1000
#define NUM_THREADS   5  // Three working threads + loadEstimator (top) + 
                         // loop (as the idle thread)
                         // TOP thread is thread with id 0

char thread_name[NUM_THREADS][15] = { "top", 
                                       "worker_1", "worker_2", "worker_3",
                                       "idle" };

volatile uint32_t threadPeriod_ms[NUM_THREADS] = { CYCLE_MS, 200, 100, 200, 0 };
volatile int threadLoad[NUM_THREADS] = {0, 50, 50, 50, 0};

volatile uint32_t threadEffectivePeriod_ms[NUM_THREADS] = { 0, 0, 0, 0, 0 };
volatile uint32_t threadCycle_ms[NUM_THREADS] = { 0, 0, 0, 0, 0 };

// Struct to measure the cpu load using the ticks consumed by each thread
typedef struct {
  thread_t * thd;
  systime_t lastSampleTime_i;
  sysinterval_t lastPeriod_i;
  sysinterval_t ticksTotal;
  sysinterval_t ticksPerCycle;
  float loadPerCycle_per;
} threadLoad_t;

typedef struct {
  threadLoad_t threadLoad[NUM_THREADS];
  uint32_t idling_per;
} systemLoad_t;

systemLoad_t sysLoad;

//------------------------------------------------------------------------------
// Load estimator (top)
// High priority thread that executes periodically
//------------------------------------------------------------------------------
BSEMAPHORE_DECL(top_sem, true);
static THD_WORKING_AREA(waTop, 256);

static THD_FUNCTION(top, arg) 
{
  (void)arg;
  bool ledState = LOW;

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, ledState);

  // Initialize sysLoad struct
  memset(&sysLoad, 0, sizeof(sysLoad));

  systime_t lastTime_i = 0;
  systime_t period_i = TIME_MS2I(CYCLE_MS);
  
  // Reset top_sem as "taken"
  chBSemReset(&top_sem, true);

  while (!chThdShouldTerminateX()) {
    
    // Wait a certain amount of time
    // chThdSleepMilliseconds(CYCLE_MS);
    systime_t deadline_i = lastTime_i + period_i;
    if (deadline_i > chVTGetSystemTimeX()) {
      chBSemWaitTimeout(&top_sem, sysinterval_t(deadline_i - chVTGetSystemTimeX()));
    }
    
    // Accumulated ticks for this cycle
    uint32_t accumTicks = 0;
    
    // This assumes that no other thread will accumulate ticks during this sampling
    // so we can use this timestamp for all threads 
    lastTime_i = chVTGetSystemTimeX();

    // tid starts at 1 because we do not include this thread (top)
    for (int tid = 1; tid < NUM_THREADS; tid++) {
      threadLoad_t * thdLoad = &(sysLoad.threadLoad[tid]);
      thdLoad->lastSampleTime_i = lastTime_i;
      systime_t ticks = chThdGetTicksX(thdLoad->thd);
/*     
      SerialUSB.print(tid);
      SerialUSB.print(" ");
      SerialUSB.println(ticks);
*/
      thdLoad->ticksPerCycle = ticks - thdLoad->ticksTotal;
      thdLoad->ticksTotal = ticks;
      accumTicks += thdLoad->ticksPerCycle;
    }
    
    for (int tid = 1; tid < NUM_THREADS; tid++) {
      threadLoad_t * thdLoad = &sysLoad.threadLoad[tid];
      thdLoad->loadPerCycle_per = (100 * (float)thdLoad->ticksPerCycle) / accumTicks;
      SerialUSB.print(thread_name[tid]);
      SerialUSB.print("  ticks(last cycle): ");
      SerialUSB.print(thdLoad->ticksPerCycle);
      SerialUSB.print("  CPU(%): ");
      SerialUSB.print(thdLoad->loadPerCycle_per);
      SerialUSB.print("   Cycle duration(ms): ");
      SerialUSB.print(threadCycle_ms[tid]);
      SerialUSB.print("  period(ms): ");
      SerialUSB.println(threadEffectivePeriod_ms[tid]);
    }
    SerialUSB.println();
    
    // Switch the led state
    ledState = (ledState == HIGH) ? LOW : HIGH;
    digitalWrite(LED_BUILTIN, ledState);
  }
}

//------------------------------------------------------------------------------
// Worker thread executes periodically
//------------------------------------------------------------------------------
static THD_WORKING_AREA(waWorker1, 256);
static THD_WORKING_AREA(waWorker2, 256);
static THD_WORKING_AREA(waWorker3, 256);

static THD_FUNCTION(worker, arg) 
{
  int worker_ID = (int)arg;
  sysinterval_t period_i = TIME_MS2I(threadPeriod_ms[worker_ID]);
  systime_t deadline_i = chVTGetSystemTimeX();
  systime_t lastBeginTime_i = 0;
  
  while (!chThdShouldTerminateX()) {
    systime_t beginTime_i = chVTGetSystemTimeX();
    threadEffectivePeriod_ms[worker_ID] = TIME_I2MS(beginTime_i - lastBeginTime_i);
    
    int niter = threadLoad[worker_ID];
    #if USE_DOUBLE
      double num = 10;
    #else
      float num = 10;
    #endif
    
    for (int iter = 0; iter < niter; iter++) {
      #if USE_DOUBLE
        num = exp(num) / (1 + exp(num));
      #else
        num = expf(num) / (1 + expf(num));
      #endif
    }
    
    deadline_i += period_i;
    
/*
    SerialUSB.print(worker_ID);
    SerialUSB.print(" ");
    SerialUSB.print(deadline_i);
    SerialUSB.print(" ");
    SerialUSB.println(chVTGetSystemTimeX());
*/
    lastBeginTime_i = beginTime_i;
    threadCycle_ms[worker_ID] = TIME_I2MS(chVTGetSystemTimeX() - beginTime_i);
    if (deadline_i > chVTGetSystemTimeX()) {
      chThdSleepUntil(deadline_i);
    }
  }
}

//------------------------------------------------------------------------------
// Continue setup() after chBegin() and create the two threads
//------------------------------------------------------------------------------
void chSetup() 
{
  // Here we assume that CH_CFG_ST_TIMEDELTA is set to zero
  // All SAMD-based boards are only supported in “tick mode”
  
  // Check first if ChibiOS configuration is compatible
  // with a non-cooperative scheme checking the value of CH_CFG_TIME_QUANTUM
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
  SerialUSB.print("CH_CFG_TIME_QUANTUM: ");
  SerialUSB.println(CH_CFG_TIME_QUANTUM);

  // Check we do not spawn the idle thread
  if (CH_CFG_NO_IDLE_THREAD == FALSE) {
    SerialUSB.println("You must set CH_CFG_NO_IDLE_THREAD to TRUE");
  }
  
  // Start top thread
  sysLoad.threadLoad[0].thd = chThdCreateStatic(waTop, sizeof(waTop),
    NORMALPRIO + 2, top, (void *)threadPeriod_ms[0]);

  // Start working threads.
  sysLoad.threadLoad[1].thd = chThdCreateStatic(waWorker1, sizeof(waWorker1),
    NORMALPRIO + 1, worker, (void *)1);
  
  sysLoad.threadLoad[2].thd = chThdCreateStatic(waWorker2, sizeof(waWorker2),
    NORMALPRIO + 1, worker, (void *)2);

  sysLoad.threadLoad[3].thd = chThdCreateStatic(waWorker3, sizeof(waWorker3),
    NORMALPRIO + 1, worker, (void *)3);

  // This thread ID
  sysLoad.threadLoad[4].thd = chThdGetSelfX();
}

//------------------------------------------------------------------------------
// setup() function
//------------------------------------------------------------------------------
void setup() 
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  SerialUSB.begin(115200);
  while(!SerialUSB) { ; }
  
  SerialUSB.println("Hit any key + ENTER to start ...");
  while(!SerialUSB.available()) { delay(10); }
  
  // Initialize OS and then call chSetup.
  // chBegin() never returns. Loop() is invoked directly from chBegin()
  chBegin(chSetup);
}

//------------------------------------------------------------------------------
// loop() function. It is considered here as the idle thread
//------------------------------------------------------------------------------
void loop() { }
