// test_mq_send.c

#include <stdio.h>
#include <stdint.h>
#include "coda_bloccante.h"
#include "disastrOS_constants.h"
#include "disastrOS_globals.h"
#include "disastrOS_descriptor.h"
#include "disastrOS_resource.h"
#include "disastrOS_pcb.h"

// init dei moduli (pool allocator, risorse, descriptor, message-queue)
extern void PCB_init();
extern void Resource_init();
extern void Descriptor_init();
extern void MessageQueue_initModule();

int main() {
  // 1) Inizializzazione del kernel simulato
  PCB_init();
  Resource_init();
  Descriptor_init();
  MessageQueue_initModule();

  // 2) Crea il PCB “running”
  PCB* pcb = PCB_alloc();
  if (!pcb) {
    fprintf(stderr, "ERROR: PCB_alloc failed\n");
    return 1;
  }
  running = pcb;
  running->status = Running;
  running->last_fd = 0;

  // 3) Creazione della coda con capacità 3
  int fd = disastrOS_mq_open(42, DSOS_CREATE, 3);
  if (fd < 0) {
    fprintf(stderr, "FAIL: mq_open returned %d\n", fd);
    return 1;
  }

  // 4) Invio di tre messaggi distinti
  intptr_t payloads[3] = { 0x1111, 0x2222, 0x3333 };
  for (int i = 0; i < 3; ++i) {
    int rc = disastrOS_mq_send(fd, (void*)payloads[i], sizeof(intptr_t));
    if (rc != 0) {
      fprintf(stderr, "FAIL: mq_send #%d returned %d\n", i, rc);
      return 1;
    }
  }

  // 5) Controllo dello stato interno della coda
  Resource* res = ResourceList_byId(&resources_list, 42);
  if (!res) {
    fprintf(stderr, "FAIL: queue resource not found\n");
    return 1;
  }
  MessageQueue* mq = (MessageQueue*)res->resource;
  if (mq->size != 3) {
    fprintf(stderr, "FAIL: expected size=3, got %d\n", mq->size);
    return 1;
  }
  if (mq->tail != 0) {
    fprintf(stderr, "FAIL: expected tail wrap to 0, got %d\n", mq->tail);
    return 1;
  }

  // 6) Verifica dei singoli payload
  for (int i = 0; i < 3; ++i) {
    void* p = mq->buffer[(mq->head + i) % mq->capacity];
    if (p != (void*)payloads[i]) {
      fprintf(stderr,
              "FAIL: slot %d expected %p, got %p\n",
              i, (void*)payloads[i], p);
      return 1;
    }
  }

  printf("PASS: disastrOS_mq_send basic functionality\n");
  return 0;
}
