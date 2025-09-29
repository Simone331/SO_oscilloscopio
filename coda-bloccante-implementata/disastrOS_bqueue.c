
// disastrOS_bqueue.c
#include <assert.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_globals.h"
#include "disastrOS_syscalls.h"
#include "bqueue.h"

// helper: wake a waiting process and put into ready
static void wake_ready(PCB* p){
  if (!p) return;
  if (List_find(&waiting_list, (ListItem*)p)){
    List_detach(&waiting_list, (ListItem*)p);
  }
  p->status=Ready;
  List_insert(&ready_list, ready_list.last, (ListItem*)p);
}


// Create: args -> id, capacity
void internal_bq_create(){
  int id  = (int) running->syscall_args[0];
  int cap = (int) running->syscall_args[1];

  printf("bq_create(pid=%d, id=%d, cap=%d)\n", running ? running->pid : -1, id, cap);

  if (cap<=0){
    running->syscall_retvalue=DSOS_ESYSCALL_ARGUMENT_OUT_OF_BOUNDS;
    return;
  }
  if (!BQ_registry()->first){
    // if first time, ensure allocator is initialized
    // (the caller should call BQ_init at boot)
  }
  if (BQ_byId(BQ_registry(), id)){
  running->syscall_retvalue = DSOS_EEXIST; // queue with id already exists
  return;
  }
  
  BlockingQueue* q=BQ_alloc(id, cap);
  if (!q){
    running->syscall_retvalue=DSOS_ESYSCALL_NOT_IMPLEMENTED;
    return;
  }
  List_insert(BQ_registry(), BQ_registry()->last, (ListItem*) q);
  running->syscall_retvalue=0;
}
// Delete: args -> id
void internal_bq_delete(){
  int id = (int) running->syscall_args[0];

  // find in registry
  BlockingQueue* q=BQ_byId(BQ_registry(), id);
  if (!q){
    running->syscall_retvalue = DSOS_ERESOURCENOFD; // reuse an error code
    return;
  }
  // must be unused by processes (no waiters)
  if (q->wait_getters.size || q->wait_putters.size){
    running->syscall_retvalue = DSOS_ERESOURCEINUSE;
    return;
  }
  List_detach(BQ_registry(), (ListItem*)q);
  BQ_free(q);
  running->syscall_retvalue=0;
}

// Put: args -> id, value
void internal_bq_put(){
  int  id    = (int)  running->syscall_args[0];
  long value = (long) running->syscall_args[1];

  BlockingQueue* q = BQ_byId(BQ_registry(), id);
  if (!q){
    running->syscall_retvalue = DSOS_ERESOURCENOFD;
    return;
  }

  // immediate handoff to a waiting getter
  if (q->wait_getters.first){
    PCBPtr* gptr = (PCBPtr*) List_detach(&q->wait_getters, q->wait_getters.first);
    PCB* getter = gptr->pcb; PCBPtr_free(gptr);
    long* outp = (long*) getter->syscall_args[1];
    if (outp) *outp = value;
    getter->syscall_retvalue = 0;
    wake_ready(getter);
    running->syscall_retvalue = 0;
    return;
  }

  // space available in buffer
  if (q->size < q->capacity){
    BQItem* it = BQItem_alloc();
    if (!it){ running->syscall_retvalue = DSOS_ESYSCALL_NOT_IMPLEMENTED; return; }
    it->value = value;
    List_insert(&q->items, q->items.last, (ListItem*)it);
    ++q->size;
    running->syscall_retvalue = 0;
    return;
  }

  // buffer full: block producer
  running->status = Waiting;
  PCBPtr* pptr = PCBPtr_alloc(running);
  List_insert(&q->wait_putters, q->wait_putters.last, (ListItem*)pptr);
  List_insert(&waiting_list, waiting_list.last, (ListItem*)running);

  // schedule next
  if (ready_list.first){
    PCB* next=(PCB*) List_detach(&ready_list, ready_list.first);
    next->status=Running;
    running=next;
  } else {
    running=0;
  }
}

// Get: args -> id, long* out
void internal_bq_get(){
  int   id   = (int)  running->syscall_args[0];
  long* outp = (long*) running->syscall_args[1];

  BlockingQueue* q = BQ_byId(BQ_registry(), id);
  if (!q){
    running->syscall_retvalue = DSOS_ERESOURCENOFD;
    return;
  }

  // buffer has items
  if (q->items.first){
    BQItem* it=(BQItem*) List_detach(&q->items, q->items.first);
    --q->size;
    if (outp) *outp = it->value;
    BQItem_free(it);

    // wake a putter and complete its put into the buffer
    if (q->wait_putters.first){
      PCBPtr* pptr = (PCBPtr*) List_detach(&q->wait_putters, q->wait_putters.first);
      PCB* prod = pptr->pcb; PCBPtr_free(pptr);
      long v2 = (long) prod->syscall_args[1];
      BQItem* it2=BQItem_alloc();
      if (it2){
        it2->value=v2;
        List_insert(&q->items, q->items.last, (ListItem*)it2);
        ++q->size;
      }
      prod->syscall_retvalue = 0;
      wake_ready(prod);
    }

    running->syscall_retvalue = 0;
    return;
  }

  // empty buffer but a producer is waiting: direct handoff
  if (q->wait_putters.first){
    PCBPtr* pptr = (PCBPtr*) List_detach(&q->wait_putters, q->wait_putters.first);
    PCB* prod = pptr->pcb; PCBPtr_free(pptr);
    long v = (long) prod->syscall_args[1];
    if (outp) *outp = v;
    prod->syscall_retvalue = 0;
    wake_ready(prod);
    running->syscall_retvalue = 0;
    return;
  }

  // no data and no producers: block getter
  running->status = Waiting;
  PCBPtr* gptr = PCBPtr_alloc(running);
  List_insert(&q->wait_getters, q->wait_getters.last, (ListItem*)gptr);
  List_insert(&waiting_list, waiting_list.last, (ListItem*)running);

  if (ready_list.first){
    PCB* next=(PCB*) List_detach(&ready_list, ready_list.first);
    next->status=Running;
    running=next;
  } else {
    running=0;
  }
}

// helper DUMP
static int list_len(ListHead* h){
  int n=0; for (ListItem* it=h->first; it; it=it->next) ++n; return n;
}

// Dump: args -> id
void internal_bq_dump() {
  int id = (int) running->syscall_args[0];

  BlockingQueue* q = BQ_byId(BQ_registry(), id);
  if (!q) { running->syscall_retvalue = DSOS_ERESOURCENOFD; return; }

  printf("BQ[%d] size=%d/%d items=[", q->id, q->size, q->capacity);
  for (ListItem* it=q->items.first; it; it=it->next) {
    BQItem* bi = (BQItem*) it;
    printf("%ld%s", bi->value, it->next ? ", " : "");
  }
  printf("] wait_putters=%d wait_getters=%d\n",
         list_len(&q->wait_putters), list_len(&q->wait_getters));

  running->syscall_retvalue = 0;
}

// user-facing wrappers
int disastrOS_bqCreate(int id, int capacity){
  return disastrOS_syscall(DSOS_CALL_BQ_CREATE, id, capacity);
}

int disastrOS_bqPut(int id, long value){
  return disastrOS_syscall(DSOS_CALL_BQ_PUT, id, value);
}
int disastrOS_bqGet(int id, long* out){
  return disastrOS_syscall(DSOS_CALL_BQ_GET, id, (long) out);
}
int disastrOS_bqDelete(int id){
  return disastrOS_syscall(DSOS_CALL_BQ_DELETE, id);
}
int disastrOS_bqDump(int id){
  return disastrOS_syscall(DSOS_CALL_BQ_DUMP, id);
}

