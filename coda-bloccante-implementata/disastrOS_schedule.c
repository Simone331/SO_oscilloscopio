#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"

// replaces the running process with the next one in the ready list
void internal_schedule() {
  // Se c'è qualcuno pronto da eseguire...
  if (ready_list.first) {
    PCB* next = (PCB*) List_detach(&ready_list, ready_list.first);

    // Se c'è un running attuale, rimettimolo in ready
    if (running) {
      running->status = Ready;
      List_insert(&ready_list, ready_list.last, (ListItem*) running);
    }

    // Fai partire il prossimo
    next->status = Running;
    running = next;
  }

  // (opzionale) debug
  if (running) disastrOS_debug("SCHEDULE -> %d\n", running->pid);
}
