// bqueue.h
#pragma once
#include "disastrOS.h"
#include "disastrOS_pcb.h"
#include "linked_list.h"

typedef struct BQItem {
  ListItem list;
  long value;
} BQItem;

typedef struct BlockingQueue {
  ListItem list;        // per mettere le code in resources_list
  int id;
  int capacity;
  int size;
  ListHead items;       // BQItem
  ListHead wait_getters; // PCBPtr
  ListHead wait_putters; // PCBPtr
} BlockingQueue;

// SLAB-like allocators (come PCB/Timer)
void        BQ_init(void);
BlockingQueue* BQ_alloc(int id, int capacity);
int         BQ_free(BlockingQueue* q);

BQItem*     BQItem_alloc(void);
int         BQItem_free(BQItem* it);

// helpers
BlockingQueue* BQ_byId(ListHead* head, int id);
