// test_mq_recv.c

#include <stdio.h>
#include <stdint.h>
#include "coda_bloccante.h"
#include "disastrOS_constants.h"
#include "disastrOS_globals.h"
#include "disastrOS_descriptor.h"
#include "disastrOS_resource.h"
#include "disastrOS_pcb.h"

// init dei moduli
extern void PCB_init();
extern void Resource_init();
extern void Descriptor_init();
extern void MessageQueue_initModule();

int main() {
  // 1) Inizializzazione
  PCB_init();
  Resource_init();
  Descriptor_init();
  MessageQueue_initModule();

  PCB* pcb = PCB_alloc();
  if (!pcb) { fprintf(stderr, "ERROR: PCB_alloc\n"); return 1; }
  running = pcb;
  running->status = Running;
  running->last_fd = 0;

  // 2) Apertura della coda (cap=3)
  int fd = disastrOS_mq_open(55, DSOS_CREATE, 3);
  if (fd < 0) {
    fprintf(stderr, "FAIL: mq_open -> %d\n", fd);
    return 1;
  }

  // 3) Invia 3 messaggi
  intptr_t payloads[3] = { 0xAAA1, 0xBBB2, 0xCCC3 };
  for (int i = 0; i < 3; ++i) {
    int rc = disastrOS_mq_send(fd, (void*)payloads[i], sizeof(intptr_t));
    if (rc != 0) {
      fprintf(stderr, "FAIL: mq_send #%d -> %d\n", i, rc);
      return 1;
    }
  }

  // 4) Ricevi 3 messaggi e verifica
  for (int i = 0; i < 3; ++i) {
    void* received = NULL;
    int rc = disastrOS_mq_recv(fd, &received, sizeof(void*));
    if (rc != 0) {
      fprintf(stderr, "FAIL: mq_recv #%d -> %d\n", i, rc);
      return 1;
    }
    if (received != (void*)payloads[i]) {
      fprintf(stderr,
              "FAIL: recv #%d expected %p, got %p\n",
              i, (void*)payloads[i], received);
      return 1;
    }
  }

  // 5) Controlla che la coda sia svuotata
  Resource* res = ResourceList_byId(&resources_list, 55);
  MessageQueue* mq = (MessageQueue*)res->resource;
  if (mq->size != 0) {
    fprintf(stderr, "FAIL: expected final size 0, got %d\n", mq->size);
    return 1;
  }

  printf("PASS: disastrOS_mq_recv basic functionality\n");
  return 0;
}
