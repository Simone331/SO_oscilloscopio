// disastrOS_test.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <poll.h>

#include "disastrOS.h"
#include "disastrOS_bqueue.h"   // tipo risorsa e wrapper enqueue/dequeue

// =================== Configurazione Blocking Queue ===================
#define RES_ID               4242        // id della risorsa "coda bloccante"
#define N_PRODUCERS          3
#define N_CONSUMERS          2
#define ITEMS_PER_PRODUCER   8

#define TOTAL_ITEMS   (N_PRODUCERS * ITEMS_PER_PRODUCER)

// we need this to handle the sleep state
void sleeperFunction(void* args){
  printf("Hello, I am the sleeper, and I sleep %d\n", disastrOS_getpid());
  while (1) {
    getc(stdin);              // premi Invio per stampare lo stato
    disastrOS_printStatus();
  }
}

// =================== Produttore ===================
void producerFunction(void* args) {
  int producer_id = (int)(intptr_t) args;

  int fd = disastrOS_openResource(RES_ID, DSOS_RESOURCE_TYPE_BQUEUE, 0);
  printf("[P%d] openResource -> fd=%d\n", producer_id, fd);
  if (fd < 0) {
    printf("[P%d] ERROR: openResource failed (%d)\n", producer_id, fd);
    disastrOS_exit(-1);
    return;
  }

  for (int i=0; i<ITEMS_PER_PRODUCER; ++i) {
    int value = producer_id*1000 + i;   // valore tracciabile
    printf("[P%d] enqueue -> %d\n", producer_id, value);
    int r = disastrOS_bqEnqueue(fd, value);   // si blocca se piena
    if (r < 0) {
      printf("[P%d] ERROR: enqueue failed (%d)\n", producer_id, r);
      disastrOS_closeResource(fd);
      disastrOS_exit(-2);
      return;
    }
    disastrOS_preempt(); // interleaving
  }

  disastrOS_closeResource(fd);
  printf("[P%d] done\n", producer_id);
  disastrOS_exit(0);
}

// =================== Consumatore ===================
void consumerFunction(void* args) {
  int consumer_idx = (int)(intptr_t) args;

  // ripartizione elementi: l'ultimo consuma anche l'eventuale resto
  int base = TOTAL_ITEMS / N_CONSUMERS;
  int rest = TOTAL_ITEMS % N_CONSUMERS;
  int my_quota = base + ((consumer_idx == N_CONSUMERS-1) ? rest : 0);

  int fd = disastrOS_openResource(RES_ID, DSOS_RESOURCE_TYPE_BQUEUE, 0);
  printf("[C%d] openResource -> fd=%d (quota=%d)\n", consumer_idx, fd, my_quota);
  if (fd < 0) {
    printf("[C%d] ERROR: openResource failed (%d)\n", consumer_idx, fd);
    disastrOS_exit(-1);
    return;
  }

  for (int i=0; i<my_quota; ++i) {
    int v = disastrOS_bqDequeue(fd);     // si blocca se vuota
    if (v < 0) {
      printf("[C%d] ERROR: dequeue failed (%d)\n", consumer_idx, v);
      disastrOS_closeResource(fd);
      disastrOS_exit(-2);
      return;
    }
    printf("[C%d] dequeue <- %d\n", consumer_idx, v);
    disastrOS_preempt(); // interleaving
  }

  disastrOS_closeResource(fd);
  printf("[C%d] done\n", consumer_idx);
  disastrOS_exit(0);
}

// =================== initFunction ===================
void initFunction(void* args) {
  disastrOS_printStatus();
  printf("hello, I am init and I just started (pid=%d)\n", disastrOS_getpid());

  // thread "sleeper" per ispezionare lo stato a richiesta
  //disastrOS_spawn(sleeperFunction, 0);

  // Crea la risorsa "blocking queue" (se non esiste)
  int mode = DSOS_CREATE;
  printf("[INIT] opening/creating BQueue resource (id=%d, mode=%d)\n", RES_ID, mode);
  int fd_parent = disastrOS_openResource(RES_ID, DSOS_RESOURCE_TYPE_BQUEUE, mode);
  printf("[INIT] openResource -> fd=%d\n", fd_parent);
  if (fd_parent < 0) {
    printf("[INIT] ERROR: openResource failed (%d)\n", fd_parent);
    disastrOS_shutdown();
    return;
  }
  // il padre non usa direttamente la coda: può chiudere
  disastrOS_closeResource(fd_parent);

  // Spawno produttori e consumatori (disastrOS_spawn restituisce void)
  printf("[INIT] spawning %d producers and %d consumers\n", N_PRODUCERS, N_CONSUMERS);
  int alive_children = 0;

  for (int p=0; p<N_PRODUCERS; ++p) {
    disastrOS_spawn(producerFunction, (void*)(intptr_t)(p+1)); // id 1..N
    ++alive_children;
  }

  for (int c=0; c<N_CONSUMERS; ++c) {
    disastrOS_spawn(consumerFunction, (void*)(intptr_t)c);     // idx 0..M-1
    ++alive_children;
  }

  disastrOS_printStatus();
  for (int k=0; k<5; ++k) disastrOS_preempt();
  // Attendo tutti i figli (usa la tua semantica di wait: qui 0 = qualunque)
  int retval, pid;
  while (alive_children>0 && (pid=disastrOS_wait(0, &retval))>=0) {
    disastrOS_printStatus();
    printf("[INIT] child %d terminated, retval=%d, alive=%d\n", pid, retval, alive_children);
    --alive_children;
  }

  // Distruggo la risorsa (nessuno deve più usarla)
  int r = disastrOS_destroyResource(RES_ID);
  printf("[INIT] destroyResource(%d) -> %d\n", RES_ID, r);

  printf("shutdown!\n");
  disastrOS_shutdown();
}

// =================== main ===================
int main(int argc, char** argv){
  char* logfilename = 0;
  if (argc>1) {
    logfilename = argv[1];
  }
  printf("the function pointer is: %p\n", producerFunction);
  printf("start\n");
  // mantengo la tua firma a 3 parametri
  disastrOS_start(initFunction, 0, logfilename);
  return 0;
}
