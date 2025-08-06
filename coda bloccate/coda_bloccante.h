
#pragma once

#include "disastrOS_resource.h"
#include "linked_list.h"

/* Tipo risorsa per la coda bloccante */
#define DSOS_RESOURCE_MQUEUE  1 /* Valore da allineare a ResourceType in disastrOS_resource.h */

/* Struttura per la coda circolare bloccante */
typedef struct MessageQueue {
    void**      buffer;      /* Puntatore al buffer circolare */
    int        capacity;    /* Capacità massima (# messaggi) */
    int        size;        /* Messaggi correnti nella coda */
    int        head;        /* Indice di lettura */
    int        tail;        /* Indice di scrittura */
    ListHead   send_wait;   /* Processi in attesa di spazio libero */
    ListHead   recv_wait;   /* Processi in attesa di messaggi */
} MessageQueue;

/* Descriptor specifico per la coda di messaggi */
typedef struct MQDescriptor {
    int           fd;       /* File descriptor locale */
    MessageQueue* queue;    /* Puntatore alla struttura globale */
} MQDescriptor;

/* Interfaccia delle nuove syscall */
int disastrOS_mq_open(int key, int flags, int capacity);
int disastrOS_mq_send(int fd, void* msg, int len);
int disastrOS_mq_recv(int fd, void* buf, int len);
int disastrOS_mq_close(int fd);
int disastrOS_mq_unlink(int key);

