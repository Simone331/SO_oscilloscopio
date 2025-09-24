#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"

void internal_preempt() {
   // 1) rimetti il processo corrente in Ready
  if (running) {
    running->status = Ready;
    List_insert(&ready_list, ready_list.last, (ListItem*) running);
  }
  // 2) poi scegli il prossimo
  internal_schedule();
}
