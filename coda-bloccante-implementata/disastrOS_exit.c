#include <assert.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_pcb.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_schedule.h"

void internal_exit(){

  running->return_value = running->syscall_args[0];

  while(running->children.first){
    ListItem* child_item = List_detach(&running->children, running->children.first);
    PCBPtr* child_ptr = (PCBPtr*) child_item;

    List_insert(&init_pcb->children, init_pcb->children.last, child_item);
    child_ptr->pcb->parent = init_pcb;
  }

  running->status = Zombie;
  List_insert(&zombie_list, zombie_list.last, (ListItem*) running);

  PCB* parent = running->parent;
  if (parent && parent->status == Waiting && parent->syscall_num == DSOS_CALL_WAIT) {
    int pid_to_wait = parent->syscall_args[0];
    if (pid_to_wait == 0 || pid_to_wait == running->pid) {
      List_detach(&waiting_list, (ListItem*) parent);
      parent->status = Ready;
      List_insert(&ready_list, ready_list.last, (ListItem*) parent);
      
    }
  }

  running = 0; 
  internal_schedule();
}