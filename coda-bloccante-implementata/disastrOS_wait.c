// disastrOS_wait.c — DROP-IN

#include <stdio.h>
#include <assert.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_pcb.h"

// Reaping sicuro: stacca child da zombie_list e da parent->children, consegna *result e libera
static int reap_child(PCB* parent, PCB* child, int* result_out) {
  // 1) rimuovi dallo zombie_list (se presente)
  if (List_find(&zombie_list, (ListItem*) child)) {
    List_detach(&zombie_list, (ListItem*) child);
  }
  // 2) rimuovi il link del figlio dalla lista children del padre
  if (parent) {
    PCBPtr* link = PCBPtr_byPID(&parent->children, child->pid);
    if (link) {
      List_detach(&parent->children, (ListItem*) link);
      PCBPtr_free(link);
    }
  }
  // 3) consegna l'exit code e libera il PCB del figlio
  if (result_out) *result_out = child->return_value;
  int reaped_pid = child->pid;
  PCB_free(child);
  return reaped_pid;
}

void internal_wait() {
  // argomenti: pid da attendere (0 = qualunque) e puntatore a result
  int  pid_to_wait = running->syscall_args[0];
  int* result      = (int*) running->syscall_args[1];

  // se non ho figli, errore immediato
  if (!running->children.first) {
    running->syscall_retvalue = DSOS_EWAIT;   // qualsiasi negativo va bene
    return;
  }

  // cerca uno zombie del chiamante (match su parent e, se specificato, su pid)
  PCB* awaited = 0;
  for (ListItem* it = zombie_list.first; it; it = it->next) {
    PCB* z = (PCB*) it;
    if (z->parent != running) continue;
    if (pid_to_wait==0 || z->pid==pid_to_wait) { awaited = z; break; }
  }

  // caso 1: ho già uno zombie atteso -> reaping immediato
  if (awaited) {
    int reaped_pid = reap_child(running, awaited, result);
    running->syscall_retvalue = reaped_pid;
    return;
  }

  // caso 2: nessuno zombie atteso -> blocco il padre su WAIT
  running->status = Waiting;
  List_insert(&waiting_list, waiting_list.last, (ListItem*) running);

  // scegli il prossimo pronto (nessun setcontext qui dentro!)
  PCB* next = (PCB*) List_detach(&ready_list, ready_list.first);
  running = next;  // se non c'è nessuno pronto, resterà 0: penserà il timer a svegliare qualcuno
}
