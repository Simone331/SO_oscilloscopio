#include "coda_bloccante.h"
#include "disastrOS_resource.h"
#include "disastrOS_descriptor.h"
#include "disastrOS_globals.h"
#include "linked_list.h"
#include "disastrOS_pcb.h"

#include <stdlib.h>
#include <string.h>

/* Helper macro per calcolare l'indice successivo in un buffer circolare */
#define NEXT_IDX(i, cap) (((i) + 1) % (cap))

/******************************
 * disastrOS_mq_open          *
 ******************************/
int disastrOS_mq_open(int key, int flags, int capacity) {
    /* TODO:
       1. Validare capacity > 0
       2. Cercare Resource esistente con id=key
       3. Se non esiste, allocare:
            - Resource con type=DSOS_RESOURCE_MQUEUE
            - MessageQueue + buffer circolare
            - Inizializzare send_wait / recv_wait (List_init)
       4. Creare MQDescriptor, linkarlo al PCB corrente, restituire fd
    */
    if (capacity <= 0) {
        return DSOS_ESYSCALL_ARGUMENT_OUT_OF_BOUNDS; // Invalid capacity
    }
     /* 2) Cerco risorsa esistente */
    Resource* res = ResourceList_byId(&resources_list, key);
    if (res) {
        // Risorsa già esistente, ritorno il file descriptor
        return disastrOS_descriptor_create(DSOS_RESOURCE_MQUEUE, res->id);
    }
    /* 3) Risorsa non esistente, creo una nuova risorsa */
    res = (Resource*)Pool_alloc(sizeof(Resource));
    return -1; // stub
}

/******************************
 * disastrOS_mq_send          *
 ******************************/
int disastrOS_mq_send(int fd, void* msg, int len) {
    /* TODO:
       1. Recuperare MQDescriptor dal PCB (fd)
       2. Se coda piena -> bloccare processo in send_wait (List_pushBack + disastrOS_block)
       3. Copiare il messaggio in buffer[tail], aggiornare tail/size
       4. Se esiste un processo in recv_wait, svegliarlo (disastrOS_wakeup)
    */
    return -1; // stub
}

/******************************
 * disastrOS_mq_recv          *
 ******************************/
int disastrOS_mq_recv(int fd, void* buf, int len) {
    /* TODO:
       1. Recuperare MQDescriptor dal PCB (fd)
       2. Se coda vuota -> bloccare processo in recv_wait
       3. Prelevare messaggio da buffer[head], aggiornare head/size
       4. Se esiste un processo in send_wait, svegliarlo
    */
    return -1; // stub
}

/******************************
 * disastrOS_mq_close         *
 ******************************/
int disastrOS_mq_close(int fd) {
    /* TODO:
       1. Rimuovere MQDescriptor dal PCB
       2. Se ultimo descriptor, liberare resource e buffer
    */
    return -1; // stub
}

/******************************
 * disastrOS_mq_unlink        *
 ******************************/
int disastrOS_mq_unlink(int key) {
    /* TODO:
       1. Cercare Resource con id=key
       2. Svegliare eventuali processi bloccati con errore
       3. Liberare memoria e rimuovere risorsa
    */
    return -1; // stub
}
