#pragma once
#include "linked_list.h"
#include "disastrOS_resource.h"
#include "disastrOS_descriptor.h"
#include "disastrOS_pcb.h"

// Tipo di risorsa per la blocking queue
#define DSOS_RESOURCE_TYPE_BQUEUE  1

// Capacità di default (puoi cambiarla)
#define BQUEUE_DEFAULT_CAPACITY    8

// Errori specifici (ritorno negativo)
#define DSOS_EBQUEUE_BADFD         -30
#define DSOS_EBQUEUE_BADTYPE       -31
#define DSOS_EBQUEUE_NOMEM         -32

typedef struct BQWriterWaiter {
  ListItem list;
  PCB* pcb;
  int value;
} BQWriterWaiter;

typedef struct BQueue {
  int capacity;
  int count;
  int head, tail;
  int* buf;

  // attese
  ListHead wait_readers;   // PCBPtr
  ListHead wait_writers;   // BQWriterWaiter
} BQueue;

// allocazione / cleanup
BQueue* BQueue_create(int capacity);
void    BQueue_free(BQueue* q);

// risvegli interni
void    BQueue_wake_one_reader(BQueue* q, int value);
void    BQueue_wake_one_writer(BQueue* q);

// syscall (lato kernel)
void internal_bqEnqueue();
void internal_bqDequeue();

// wrapper user-space (dichiarati in disastrOS.h / definiti in disastrOS.c)
// int disastrOS_bqEnqueue(int fd, int value);
// int disastrOS_bqDequeue(int fd);
