#include <stdio.h>
#include <stdint.h>
#include "disastrOS.h"
#include "coda_bloccante.h"

void producerFunction(void* args) {
  int key = (int)(intptr_t)args;
  int fd  = disastrOS_mq_open(key, 0, 4);
  printf("[producer %d] opened fd=%d\n", disastrOS_getpid(), fd);
  for (int i = 0; i < 8; ++i) {
    printf("[producer %d] sending %d\n", disastrOS_getpid(), i);
    int rc = disastrOS_mq_send(fd, (void*)(intptr_t)i, sizeof(int));
    if (rc) {
      printf("[producer %d] mq_send error %d\n", disastrOS_getpid(), rc);
      break;
    }
  }
  disastrOS_exit(0);
}

void consumerFunction(void* args) {
  int key = (int)(intptr_t)args;
  int fd  = disastrOS_mq_open(key, 0, 4);
  printf("[consumer %d] opened fd=%d\n", disastrOS_getpid(), fd);
  for (int i = 0; i < 8; ++i) {
    void* msg = NULL;
    int rc = disastrOS_mq_recv(fd, &msg, sizeof(void*));
    if (rc) {
      printf("[consumer %d] mq_recv error %d\n", disastrOS_getpid(), rc);
      break;
    }
    printf("[consumer %d] received %d\n", disastrOS_getpid(), (int)(intptr_t)msg);
  }
  disastrOS_exit(0);
}


void initFunction(void* args) {
  // inizializzo i pool
  MessageQueue_initModule();
  int key = 500;
  int fd  = disastrOS_mq_open(key, DSOS_CREATE, 4);
  printf("[init] created mq key=%d, fd=%d\n", key, fd);

  // spawn davvero producer e consumer
  disastrOS_spawn(producerFunction, (void*)(intptr_t)key);
  disastrOS_spawn(consumerFunction, (void*)(intptr_t)key);

  // aspetto i due figli
  int alive = 2, pid, retval;
  while (alive > 0 && (pid = disastrOS_wait(0, &retval)) >= 0) {
    printf("[init] child %d exited with %d, alive=%d\n", pid, retval, --alive);
  }

  printf("[init] all done, shutdown\n");
  disastrOS_shutdown();
}

int main(int argc, char** argv) {
  printf("[main] start\n");
  disastrOS_start(initFunction, 0, 0);
  return 0;
}

