// disastrOS_schedule.c (o disastrOS.c se lo tieni lì)
#include "disastrOS.h"
#include "disastrOS_pcb.h"
#include "linked_list.h"
#include "disastrOS_globals.h"

// disastrOS_schedule.c (o dentro disastrOS.c)
extern PCB* idle_pcb;
extern ListHead ready_list;

void internal_schedule() {
  if (running && running->status==Running) {
    running->status=Ready;
    List_insert(&ready_list, ready_list.last, (ListItem*) running);
  }
  PCB* next = ready_list.first
              ? (PCB*) List_detach(&ready_list, ready_list.first)
              : idle_pcb;
  running = next;
  running->status = Running;
}
