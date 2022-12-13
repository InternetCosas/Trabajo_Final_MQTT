
// ----------------------------------------------------------------------
// PrinterThread - Imprime los mensajes recibidos en el mailbox
//                 Prioridad: PRINTING_THD_PRIO
//                 STACK:256 bytes
// ----------------------------------------------------------------------
static THD_FUNCTION(PrinterThread , arg) 
{
  (void)arg;

  while (true) {
    
    printerMsg_t *p = 0;

    // Get mail.
    msg_t res = chMBFetchTimeout(&printer_mailbox, (msg_t*)&p, TIME_INFINITE);
    if (res == MSG_OK) {
      debugPort.print(p->name);
      debugPort.write(' ');
      debugPort.print(TIME_I2MS(p->time_i));
      debugPort.print(" ms ");
      debugPort.println(p->msg);
      
      // Put memory back into pool.
      chPoolFree(&printerMemPool, p);  
    }
    else {
      debugPort.println("printer: chMBFetchTimeout() failed");
    }
        
  }
}
