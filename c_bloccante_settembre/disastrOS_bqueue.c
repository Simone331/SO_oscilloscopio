// disastrOS_bqueue.c
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "disastrOS.h"
#include "disastrOS_globals.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_bqueue.h"
#include "disastrOS_descriptor.h"
#include "disastrOS_resource.h"

// ====== liste globali / running (definite altrove) ======
extern ListHead ready_list;
extern ListHead waiting_list;
extern PCB*     running;

// ====== helper: stacca (se presente) un PCB dalla waiting_list globale ======
static void detach_from_waiting(PCB* pcb){
  if (!pcb) return;
  ListItem* it = waiting_list.first;
  while (it){
    PCB* p = (PCB*) it;
    ListItem* next = it->next;
    if (p==pcb){
      List_detach(&waiting_list, it);
      break;
    }
    it = next;
  }
}

// ====== alloc / free ======
BQueue* BQueue_create(int capacity) {
  if (capacity<=0) capacity = BQUEUE_DEFAULT_CAPACITY;
  BQueue* q = (BQueue*) malloc(sizeof(BQueue));
  if (!q) return 0;

  q->capacity = capacity;
  q->count    = 0;
  q->head     = 0;
  q->tail     = 0;
  q->buf      = (int*) malloc(sizeof(int)*capacity);
  if (!q->buf){
    free(q);
    return 0;
  }
  List_init(&q->wait_readers);
  List_init(&q->wait_writers);
  return q;
}

void BQueue_free(BQueue* q) {
  if (!q) return;

  // sveglia eventuali reader bloccati con errore
  while (q->wait_readers.first){
    PCBPtr* pp = (PCBPtr*) List_popFront(&q->wait_readers);
    if (pp && pp->pcb){
      detach_from_waiting(pp->pcb);
      pp->pcb->status = Ready;
      pp->pcb->syscall_retvalue = DSOS_ERESOURCEINUSE; // o altro codice errore
      List_insert(&ready_list, ready_list.last, (ListItem*) pp->pcb);
    }
    PCBPtr_free(pp);
  }
  // sveglia eventuali writer bloccati con errore
  while (q->wait_writers.first){
    BQWriterWaiter* ww = (BQWriterWaiter*) List_popFront(&q->wait_writers);
    if (ww && ww->pcb){
      detach_from_waiting(ww->pcb);
      ww->pcb->status = Ready;
      ww->pcb->syscall_retvalue = DSOS_ERESOURCEINUSE;
      List_insert(&ready_list, ready_list.last, (ListItem*) ww->pcb);
    }
    free(ww);
  }

  free(q->buf);
  free(q);
}

// ====== wake helpers ======
void BQueue_wake_one_reader(BQueue* q, int value) {
  PCBPtr* pp = (PCBPtr*) List_popFront(&q->wait_readers);
  if (!pp) return;
  PCB* pcb = pp->pcb;

  detach_from_waiting(pcb);
  pcb->status = Ready;
  pcb->syscall_retvalue = value;
  List_insert(&ready_list, ready_list.last, (ListItem*) pcb);

  PCBPtr_free(pp);
}

void BQueue_wake_one_writer(BQueue* q) {
  BQWriterWaiter* ww = (BQWriterWaiter*) List_popFront(&q->wait_writers);
  if (!ww) return;

  // ora c'è spazio nel buffer (perché si è fatto un dequeue prima)
  assert(q->count < q->capacity);
  q->buf[q->tail] = ww->value;
  q->tail = (q->tail + 1) % q->capacity;
  ++q->count;

  PCB* pcb = ww->pcb;
  detach_from_waiting(pcb);
  pcb->status = Ready;
  pcb->syscall_retvalue = 0;
  List_insert(&ready_list, ready_list.last, (ListItem*) pcb);

  free(ww);
}

// ====== fd -> BQueue ======
static BQueue* _bq_from_fd(int fd, Descriptor** out_des) {
  Descriptor* des = DescriptorList_byFd(&running->descriptors, fd);
  if (!des) return 0;
  if (out_des) *out_des = des;

  Resource* res = des->resource;
  if (!res || res->type!=DSOS_RESOURCE_TYPE_BQUEUE) return (BQueue*) -1;
  return (BQueue*) res->data;
}

// ====== SYSCALL: ENQUEUE ======
void internal_bqEnqueue() {
  int fd    = running->syscall_args[0];
  int value = running->syscall_args[1];

  Descriptor* des = 0;
  BQueue* q = _bq_from_fd(fd, &des);
  if (!q)                   { running->syscall_retvalue = DSOS_EBQUEUE_BADFD;  return; }
  if (q==(BQueue*) -1)      { running->syscall_retvalue = DSOS_EBQUEUE_BADTYPE;return; }

  printf("[BQ] ENQ pid=%d fd=%d count=%d cap=%d\n",
         running->pid, fd, q->count, q->capacity);

  // se c'è un reader in attesa: handoff diretto
  if (q->wait_readers.first){
    printf("[BQ] ENQ -> handoff a reader in wait\n");
    BQueue_wake_one_reader(q, value);
    running->syscall_retvalue = 0;
    return;
  }

  // spazio disponibile nel buffer
  if (q->count < q->capacity){
    q->buf[q->tail] = value;
    q->tail = (q->tail + 1) % q->capacity;
    ++q->count;
    printf("[BQ] ENQ -> buffered, count=%d\n", q->count);
    running->syscall_retvalue = 0;
    return;
  }

  // coda piena: blocca writer
  printf("[BQ] ENQ -> FULL, blocco pid=%d\n", running->pid);
  BQWriterWaiter* ww = (BQWriterWaiter*) malloc(sizeof(BQWriterWaiter));
  if (!ww){ running->syscall_retvalue = DSOS_EBQUEUE_NOMEM; return; }
  ww->pcb   = running;
  ww->value = value;
  List_insert(&q->wait_writers, q->wait_writers.last, (ListItem*) ww);

  running->status = Waiting;
  List_insert(&waiting_list, waiting_list.last, (ListItem*) running);

  // scegli il prossimo (Idle come fallback)
  internal_schedule();
}

// ====== SYSCALL: DEQUEUE ======
void internal_bqDequeue() {
  int fd = running->syscall_args[0];

  Descriptor* des = 0;
  BQueue* q = _bq_from_fd(fd, &des);
  if (!q)                   { running->syscall_retvalue = DSOS_EBQUEUE_BADFD;  return; }
  if (q==(BQueue*) -1)      { running->syscall_retvalue = DSOS_EBQUEUE_BADTYPE;return; }

  printf("[BQ] DEQ pid=%d fd=%d count=%d cap=%d\n",
         running->pid, fd, q->count, q->capacity);

  // buffer non vuoto
  if (q->count > 0){
    int value = q->buf[q->head];
    q->head = (q->head + 1) % q->capacity;
    --q->count;

    running->syscall_retvalue = value;
    printf("[BQ] DEQ -> got %d, count=%d\n", value, q->count);

    // se ci sono writer in attesa, svegliane uno (ora c'è spazio)
    if (q->wait_writers.first){
      printf("[BQ] DEQ -> sveglio writer\n");
      BQueue_wake_one_writer(q);
    }
    return;
  }

  // buffer vuoto ma writer in attesa: handoff diretto
  if (q->wait_writers.first){
    BQWriterWaiter* ww = (BQWriterWaiter*) List_popFront(&q->wait_writers);
    int value = ww->value;
    PCB* wpcb = ww->pcb;
    int  wpid = wpcb ? wpcb->pid : -1;

    detach_from_waiting(wpcb);
    if (wpcb){
      wpcb->status = Ready;
      wpcb->syscall_retvalue = 0;
      List_insert(&ready_list, ready_list.last, (ListItem*) wpcb);
    }
    free(ww);

    running->syscall_retvalue = value;
    printf("[BQ] DEQ -> handoff diretto da writer %d, value=%d\n", wpid, value);
    return;
  }

  // nessun dato e nessun writer pronto: blocca reader
  printf("[BQ] DEQ -> EMPTY, blocco pid=%d\n", running->pid);
  PCBPtr* pp = PCBPtr_alloc(running);
  List_insert(&q->wait_readers, q->wait_readers.last, (ListItem*) pp);

  running->status = Waiting;
  List_insert(&waiting_list, waiting_list.last, (ListItem*) running);

  // scegli il prossimo (Idle come fallback)
  internal_schedule();
}
