#include "coda_bloccante.h"
#include "pool_allocator.h"
#include "disastrOS_constants.h"
#include "disastrOS_resource.h"
#include "disastrOS_descriptor.h"
#include "disastrOS_globals.h"
#include "linked_list.h"
#include <assert.h>
#include <string.h>

/************************************************************
 * Parametri di configurazione                               
 * Puoi modificarli a seconda delle necessità del tuo sistema
 ************************************************************/
#define MAX_NUM_MESSAGEQUEUES   16   /* quante code totali può gestire il kernel */
#define MAX_MSGS_PER_QUEUE      64   /* capacità massima (numero messaggi) per coda */

/* Pool per le strutture MessageQueue ---------------------- */
#define MQ_SIZE         sizeof(MessageQueue)
#define MQ_MEMSIZE      (MQ_SIZE + sizeof(int))
#define MQ_BUFFER_SIZE  (MAX_NUM_MESSAGEQUEUES * MQ_MEMSIZE)

static char _mq_buffer[MQ_BUFFER_SIZE];
static PoolAllocator _mq_allocator;

/* Pool per i buffer circolari (array di void*) ------------- */
#define MQDATA_SIZE        (MAX_MSGS_PER_QUEUE * sizeof(void*))
#define MQDATA_MEMSIZE     (MQDATA_SIZE + sizeof(int))
#define MQDATA_BUFFER_SIZE (MAX_NUM_MESSAGEQUEUES * MQDATA_MEMSIZE)

static char _mqdata_buffer[MQDATA_BUFFER_SIZE];
static PoolAllocator _mqdata_allocator;

/************************************************************
 * Inizializzazione del modulo                              *
 * Chiama questa funzione una sola volta all'avvio del kernel
 ************************************************************/
void MessageQueue_initModule() {
    int res = PoolAllocator_init(&_mq_allocator,
                                 MQ_SIZE,
                                 MAX_NUM_MESSAGEQUEUES,
                                 _mq_buffer,
                                 MQ_BUFFER_SIZE);
    assert(!res);

    res = PoolAllocator_init(&_mqdata_allocator,
                             MQDATA_SIZE,
                             MAX_NUM_MESSAGEQUEUES,
                             _mqdata_buffer,
                             MQDATA_BUFFER_SIZE);
    assert(!res);
}

/* macro di supporto per buffer circolare */
#define NEXT_IDX(i, cap)   (((i) + 1) % (cap))

/******************************
 * disastrOS_mq_open          *
 ******************************/
int disastrOS_mq_open(int key, int flags, int capacity) {
    /* 1) Validazione argomenti */
    if (capacity <= 0 || capacity > MAX_MSGS_PER_QUEUE)
        return DSOS_ESYSCALL_ARGUMENT_OUT_OF_BOUNDS;

    /* 2) Cerco risorsa esistente con la stessa key */
    Resource* res = ResourceList_byId(&resources_list, key);
    if (res) {
        /* Se esiste già e l'utente chiede esclusività, errore */
        if (flags & DSOS_EXCL)
            return DSOS_ERESOURCENOEXCL;
    }
    else {
        /* Se non esiste, devo avere il flag CREATE per poterla fare */
        if (!(flags & DSOS_CREATE))
            return DSOS_ERESOURCENOFD;

        /* 3) Alloco la Resource globale */
        res = Resource_alloc(key, DSOS_RESOURCE_MQUEUE);
        if (!res)
            return DSOS_ERESOURCECREATE;

        /* 4) Alloco MessageQueue via PoolAllocator */
        MessageQueue* mq = (MessageQueue*) PoolAllocator_getBlock(&_mq_allocator);
        if (!mq) {
            Resource_free(res);
            return DSOS_ESYSCALL_OUT_OF_RANGE;
        }

        /* 5) Alloco il buffer circolare via PoolAllocator */
        void** buf = (void**) PoolAllocator_getBlock(&_mqdata_allocator);
        if (!buf) {
            PoolAllocator_releaseBlock(&_mq_allocator, mq);
            Resource_free(res);
            return DSOS_ESYSCALL_OUT_OF_RANGE;
        }

        /* 6) Inizializzo la struttura MessageQueue */
        mq->buffer   = buf;
        mq->capacity = capacity;
        mq->size     = 0;
        mq->head     = 0;
        mq->tail     = 0;
        List_init(&mq->send_wait);
        List_init(&mq->recv_wait);

        /* 7) Collego la MessageQueue alla Resource e la metto nella lista globale */
        res->resource = mq;
        List_insert(&resources_list, resources_list.last, (ListItem*) res);
    }

    /* 8) Creo un Descriptor per il processo corrente */
    running->last_fd++;
    int fd = running->last_fd;
    Descriptor* d = Descriptor_alloc(fd, res, running);
    if (!d)
        return DSOS_ESYSCALL_OUT_OF_RANGE;
    List_insert(&running->descriptors, running->descriptors.last, (ListItem*) d);

    /* 9) Creo il DescriptorPtr nella Resource */
    DescriptorPtr* dptr = DescriptorPtr_alloc(d);
    if (!dptr)
        return DSOS_ESYSCALL_OUT_OF_RANGE;
    List_insert(&res->descriptors_ptrs, res->descriptors_ptrs.last, (ListItem*) dptr);

    /* 10) Ritorno il file descriptor */
    return fd;
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
