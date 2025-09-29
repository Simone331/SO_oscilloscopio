
// bqueue.c
#include <assert.h>
#include <stdio.h>
#include "bqueue.h"
#include "pool_allocator.h"

// ---- configuration of slab allocators ----
#define MAX_NUM_BQUEUES   128
#define MAX_NUM_BQITEMS   4096

#define BQ_SIZE       sizeof(BlockingQueue)
#define BQ_MEMSIZE    (sizeof(BlockingQueue)+sizeof(int))
#define BQ_BUFFER_SIZE  (MAX_NUM_BQUEUES*BQ_MEMSIZE)

#define BQITEM_SIZE       sizeof(BQItem)
#define BQITEM_MEMSIZE    (sizeof(BQItem)+sizeof(int))
#define BQITEM_BUFFER_SIZE  (MAX_NUM_BQITEMS*BQITEM_MEMSIZE)

// buffers and allocators
static char _bq_buffer[BQ_BUFFER_SIZE];
static PoolAllocator _bq_allocator;

static char _bqitem_buffer[BQITEM_BUFFER_SIZE];
static PoolAllocator _bqitem_allocator;



// global registry
static ListHead _bq_registry;

void BQ_init(void){
  PoolAllocatorResult r;
  r=PoolAllocator_init(&_bq_allocator, BQ_SIZE, MAX_NUM_BQUEUES,
                       _bq_buffer, BQ_BUFFER_SIZE);
  assert(r==Success);

  r=PoolAllocator_init(&_bqitem_allocator, BQITEM_SIZE, MAX_NUM_BQITEMS,
                       _bqitem_buffer, BQITEM_BUFFER_SIZE);
  assert(r==Success);

  List_init(&_bq_registry);
}

ListHead* BQ_registry(){
  return &_bq_registry;
}

BlockingQueue* BQ_alloc(int id, int capacity){
  BlockingQueue* q=(BlockingQueue*) PoolAllocator_getBlock(&_bq_allocator);
  if (!q) return 0;
  q->list.prev=q->list.next=0;
  q->id=id;
  q->capacity=capacity;
  q->size=0;
  List_init(&q->items);
  List_init(&q->wait_getters);
  List_init(&q->wait_putters);
  return q;
}

int BQ_free(BlockingQueue* q){
  // free all items
  while(q->items.first){
    BQItem* it=(BQItem*) List_detach(&q->items, q->items.first);
    PoolAllocator_releaseBlock(&_bqitem_allocator, it);
  }
  q->size=0;
  // lists of waiters must be empty (kernel should have moved them)
  assert(q->wait_getters.size==0);
  assert(q->wait_putters.size==0);
  PoolAllocator_releaseBlock(&_bq_allocator, q);
  return 0;
}

BQItem* BQItem_alloc(void){
  BQItem* it=(BQItem*) PoolAllocator_getBlock(&_bqitem_allocator);
  if (!it) return 0;
  it->list.prev=it->list.next=0;
  it->value=0;
  return it;
}

int BQItem_free(BQItem* it){
  PoolAllocator_releaseBlock(&_bqitem_allocator, it);
  return 0;
}

BlockingQueue* BQ_byId(ListHead* head, int id){
  ListItem* aux=head->first;
  while(aux){
    BlockingQueue* q=(BlockingQueue*) aux;
    if (q->id==id) return q;
    aux=aux->next;
  }
  return 0;
}
