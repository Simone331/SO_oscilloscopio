// test_mq_open.c

#include <stdio.h>
#include "coda_bloccante.h"
#include "disastrOS_constants.h"
#include "disastrOS_globals.h"
#include "disastrOS_descriptor.h"
#include "disastrOS_resource.h"
#include "disastrOS_pcb.h"

// Dichiarazioni delle init dei vari moduli
extern void Resource_init();
extern void Descriptor_init();
extern void MessageQueue_initModule();

int main() {
  int rc;

  // 1) inizializza i pool per PCB, Descriptor, Resource
  PCB_init();
  Resource_init();
  Descriptor_init();

  // 2) inizializza il modulo della coda di messaggi
  MessageQueue_initModule();

  // 3) crea un PCB fittizio e lo imposta come "running"
  PCB* init_pcb = PCB_alloc();
  if (!init_pcb) {
    fprintf(stderr, "ERROR: PCB_alloc failed\n");
    return 1;
  }
  running = init_pcb;
  running->status = Running;
  running->last_fd = 0;

  printf("=== Test mq_open ===\n");

  // Caso 1: CREATE con capacity valida
  rc = disastrOS_mq_open(100, DSOS_CREATE, 10);
  if (rc >= 0) {
    printf("PASS: creato mq con fd = %d\n", rc);
  } else {
    printf("FAIL: disastrOS_mq_open CREATE ha restituito %d\n", rc);
  }

  // Caso 2: riapertura senza EXCL
  rc = disastrOS_mq_open(100, 0, 10);
  if (rc >= 0) {
    printf("PASS: riapertura mq esistente fd = %d\n", rc);
  } else {
    printf("FAIL: reopen ha restituito %d\n", rc);
  }

  // Caso 3: EXCL su mq esistente
  rc = disastrOS_mq_open(100, DSOS_EXCL, 10);
  if (rc == DSOS_ERESOURCENOEXCL) {
    printf("PASS: EXCL ha fallito col codice corretto (%d)\n", rc);
  } else {
    printf("FAIL: EXCL ha restituito %d (atteso %d)\n", rc, DSOS_ERESOURCENOEXCL);
  }

  // Caso 4: CREATE con capacity non valida
  rc = disastrOS_mq_open(200, DSOS_CREATE, 0);
  if (rc == DSOS_ESYSCALL_ARGUMENT_OUT_OF_BOUNDS) {
    printf("PASS: capacity=0 ha fallito con ARG_OUT_OF_BOUNDS (%d)\n", rc);
  } else {
    printf("FAIL: bad capacity ha restituito %d\n", rc);
  }

  return 0;
}
