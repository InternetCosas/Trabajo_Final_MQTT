//------------------------------------------------------------------------------
// Número de slots disponibles en el Mailbox/memory pool.
const size_t MB_PRINT_SLOTS = 10;

// Tipo para el los slots del memory pool.
typedef struct  
{
  char* name;
  int msg;
  systime_t time_i;
} printerMsg_t;

// Array de objetos de tipo printerMsg_t
printerMsg_t printerPool[MB_PRINT_SLOTS];

// Declaramos el banco de memoria o Memory pool.
MEMORYPOOL_DECL(printerMemPool, sizeof(printerMsg_t), PORT_NATURAL_ALIGN, NULL);

//------------------------------------------------------------------------------
// Slots de mensajes del mailbox.  Aloja punteros a los objetos del banco
// de memoria.
msg_t letter[MB_PRINT_SLOTS];

// Mailbox queue structure.
MAILBOX_DECL(printer_mailbox, &letter, MB_PRINT_SLOTS);

//------------------------------------------------------------------------------
// Prioridades de las hebras
#define WORK_THD_PRIO       (NORMALPRIO + 1)
#define PRINTER_THD_PRIO    (NORMALPRIO)

//------------------------------------------------------------------------------
// Declaramos el tamaño de los stacks de las hebras
// Estas macros deben declararse aquí o en la pestaña principal
static THD_WORKING_AREA(wa_PrinterThread, 512);
static THD_WORKING_AREA(wa_WorkThread_1, 512);
static THD_WORKING_AREA(wa_WorkThread_2, 512);
static THD_WORKING_AREA(wa_WorkThread_3, 512);

//------------------------------------------------------------------------------
// Puerto de depuración
#define debugPort SerialUSB
