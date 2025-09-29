// bqueue.h
#pragma once
#include "disastrOS.h"
#include "disastrOS_pcb.h"
#include "linked_list.h"

// Node that stores one value in the queue
typedef struct BQItem {
  ListItem list;
  long value;
} BQItem;

// Blocking bounded FIFO queue identified by an integer id
typedef struct BlockingQueue {
  ListItem list;   // to chain queues in a global list
  int id;
  int capacity;
  int size;
  ListHead items;         // BQItem list (FIFO)
  ListHead wait_getters;  // List of PCBPtr waiting on get()
  ListHead wait_putters;  // List of PCBPtr waiting on put()
} BlockingQueue;

// Initialization of allocators and global queue registry
void        BQ_init(void);

// Allocate/free a queue instance
BlockingQueue* BQ_alloc(int id, int capacity);
int         BQ_free(BlockingQueue* q);

// Allocate/free an item node
BQItem*     BQItem_alloc(void);
int         BQItem_free(BQItem* it);

// Lookup by id inside a list (typically the global registry)
BlockingQueue* BQ_byId(ListHead* head, int id);

// Global registry accessor
ListHead* BQ_registry();
