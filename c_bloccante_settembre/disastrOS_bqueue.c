#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_globals.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_bqueue.h"

static int _detach_from_waiting_list(PCB* pcb) {
  // rimuove il PCB dalla waiting_list globale
  ListItem* aux = waiting_list.first;
  while (aux) {
    PCB* p = (PCB*) aux;
    if (p==pcb) {
      List_detach(&waiting_list, aux);
      return 1;
    }
    aux = aux->next;
  }
  return 0;
}

BQueue* BQueue_create(int capacity) {
  if (capacity<=0) capacity=BQUEUE_DEFAULT_CAPACITY;
  BQueue* q = (BQueue*) malloc(sizeof(BQueue));
  if (!q) return 0;
  q->capacity = capacity;
  q->count = 0;
  q->head = 0;
  q->tail = 0;
  q->buf = (int*) malloc(sizeof(int)*capacity);
  if (!q->buf) {
    free(q);
    return 0;
  }
  List_init(&q->wait_readers);
  List_init(&q->wait_writers);
  return q;
}

void BQueue_free(BQueue* q) {
  if (!q) return;
  // Risveglia eventuali reader in attesa con errore
  while (q->wait_readers.first) {
    PCBPtr* pp = (PCBPtr*) List_popFront(&q->wait_readers);
    if (pp && pp->pcb) {
      _detach_from_waiting_list(pp->pcb);
      pp->pcb->status=Ready;
      List_insert(&ready_list, ready_list.last, (ListItem*) pp->pcb);
      pp->pcb->syscall_retvalue = DSOS_ERESOURCEINUSE;
    }
    PCBPtr_free(pp);
  }
  // Risveglia eventuali writer in attesa con errore
  while (q->wait_writers.first) {
    BQWriterWaiter* ww = (BQWriterWaiter*) List_popFront(&q->wait_writers);
    if (ww && ww->pcb) {
      _detach_from_waiting_list(ww->pcb);
      ww->pcb->status=Ready;
      ww->pcb->syscall_retvalue = DSOS_ERESOURCEINUSE;
      List_insert(&ready_list, ready_list.last, (ListItem*) ww->pcb);
    }
    free(ww);
  }
  free(q->buf);
  free(q);
}

void BQueue_wake_one_reader(BQueue* q, int value) {
  PCBPtr* pp=(PCBPtr*) List_popFront(&q->wait_readers);
  if (!pp) return;
  PCB* pcb = pp->pcb;
  _detach_from_waiting_list(pcb);
  pcb->status=Ready;
  pcb->syscall_retvalue=value;   // consegna diretta del valore
  List_insert(&ready_list, ready_list.last, (ListItem*) pcb);
  PCBPtr_free(pp);
}

void BQueue_wake_one_writer(BQueue* q) {
  BQWriterWaiter* ww=(BQWriterWaiter*) List_popFront(&q->wait_writers);
  if (!ww) return;
  assert(q->count < q->capacity);
  q->buf[q->tail]=ww->value;           // committa il valore nel buffer
  q->tail = (q->tail+1)%q->capacity;
  ++q->count;
  _detach_from_waiting_list(ww->pcb);
  ww->pcb->status=Ready;
  ww->pcb->syscall_retvalue=0;         // successo
  List_insert(&ready_list, ready_list.last, (ListItem*) ww->pcb);
  free(ww);
}

static BQueue* _bq_from_fd(int fd, Descriptor** out_des) {
  Descriptor* des=DescriptorList_byFd(&running->descriptors, fd);
  if (!des) return 0;
  if (out_des) *out_des=des;
  Resource* res=des->resource;
  if (!res || res->type!=DSOS_RESOURCE_TYPE_BQUEUE) return (BQueue*)-1;
  return (BQueue*) res->data;
}

void internal_bqEnqueue() {
  int fd   = running->syscall_args[0];
  int value= running->syscall_args[1];

  Descriptor* des=0;
  BQueue* q=_bq_from_fd(fd, &des);
  if (!q) { running->syscall_retvalue=DSOS_EBQUEUE_BADFD; return; }
  if (q==(BQueue*)-1) { running->syscall_retvalue=DSOS_EBQUEUE_BADTYPE; return; }

  // se c'è un reader in attesa, consegna diretta
  if (q->wait_readers.first) {
    BQueue_wake_one_reader(q, value);
    running->syscall_retvalue=0;
    return;
  }

  // spazio nel buffer?
  if (q->count < q->capacity) {
    q->buf[q->tail]=value;
    q->tail=(q->tail+1)%q->capacity;
    ++q->count;
    running->syscall_retvalue=0;
    return;
  }

  // coda piena: blocca writer e memorizza il valore
  BQWriterWaiter* ww=(BQWriterWaiter*) malloc(sizeof(BQWriterWaiter));
  if (!ww) { running->syscall_retvalue=DSOS_EBQUEUE_NOMEM; return; }
  ww->pcb=running;
  ww->value=value;
  List_insert(&q->wait_writers, q->wait_writers.last, (ListItem*) ww);

  running->status=Waiting;
  List_insert(&waiting_list, waiting_list.last, (ListItem*) running);

  // schedula un altro processo
  if (ready_list.first)
    running=(PCB*) List_detach(&ready_list, ready_list.first);
  else
    running=0;
}

void internal_bqDequeue() {
  int fd = running->syscall_args[0];

  Descriptor* des=0;
  BQueue* q=_bq_from_fd(fd, &des);
  if (!q) { running->syscall_retvalue=DSOS_EBQUEUE_BADFD; return; }
  if (q==(BQueue*)-1) { running->syscall_retvalue=DSOS_EBQUEUE_BADTYPE; return; }

  // buffer non vuoto: consumiamo
  if (q->count>0) {
    int value=q->buf[q->head];
    q->head=(q->head+1)%q->capacity;
    --q->count;
    running->syscall_retvalue=value;
    // ora c'è spazio: completiamo un writer in attesa (se c'è)
    if (q->wait_writers.first) {
      BQueue_wake_one_writer(q);
    }
    return;
  }

  // buffer vuoto ma writer in attesa: handoff diretto
  if (q->wait_writers.first) {
    BQWriterWaiter* ww=(BQWriterWaiter*) List_popFront(&q->wait_writers);
    int value=ww->value;
    _detach_from_waiting_list(ww->pcb);
    ww->pcb->status=Ready;
    ww->pcb->syscall_retvalue=0;
    List_insert(&ready_list, ready_list.last, (ListItem*) ww->pcb);
    free(ww);
    running->syscall_retvalue=value;
    return;
  }

  // nessuno: blocca il reader
  PCBPtr* pp=PCBPtr_alloc(running);
  List_insert(&q->wait_readers, q->wait_readers.last, (ListItem*) pp);

  running->status=Waiting;
  List_insert(&waiting_list, waiting_list.last, (ListItem*) running);

  // schedula un altro processo
  if (ready_list.first)
    running=(PCB*) List_detach(&ready_list, ready_list.first);
  else
    running=0;
}
