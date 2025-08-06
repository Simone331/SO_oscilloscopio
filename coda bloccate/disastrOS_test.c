// test_mq_all.c
//
// Kernel‐inner test per tutte le syscall di message‐queue:
//   - disastrOS_mq_open
//   - disastrOS_mq_send
//   - disastrOS_mq_recv
//   - disastrOS_mq_close
//   - disastrOS_mq_unlink
//
// Compilalo e linkalo con disastrOS_start per eseguirlo nel kernel.

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "disastrOS.h"
#include "coda_bloccante.h"
#include "disastrOS_globals.h"
#include "disastrOS_constants.h"
#include "disastrOS_resource.h"

void initFunction(void* args) {
  int passed = 0;
  int total  = 0;
  int rc;

  printf("=== Kernel‐inner full MQ syscall test ===\n");

  /* Inizializza il modulo delle message‐queue */
  MessageQueue_initModule();

  //
  // 1) disastrOS_mq_open: CREATE
  //
  total++;
  rc = disastrOS_mq_open(/*key*/ 10, /*flags*/ DSOS_CREATE, /*cap*/ 3);
  if (rc >= 0) {
    printf("PASS 1: mq_open CREATE returned fd=%d\n", rc);
    passed++;
  } else {
    printf("FAIL 1: mq_open CREATE returned %d\n", rc);
  }
  int fd1 = rc;

  //
  // 2) disastrOS_mq_open: reopen (no EXCL)
  //
  total++;
  rc = disastrOS_mq_open(10, /*flags*/ 0, /*cap*/ 3);
  if (rc >= 0) {
    printf("PASS 2: mq_open reopen returned fd=%d\n", rc);
    passed++;
  } else {
    printf("FAIL 2: mq_open reopen returned %d\n", rc);
  }

  //
  // 3) disastrOS_mq_open: EXCL on existing
  //
  total++;
  rc = disastrOS_mq_open(10, DSOS_EXCL, 3);
  if (rc == DSOS_ERESOURCENOEXCL) {
    printf("PASS 3: mq_open EXCL returned ERESOURCENOEXCL (%d)\n", rc);
    passed++;
  } else {
    printf("FAIL 3: mq_open EXCL returned %d (expected %d)\n",
           rc, DSOS_ERESOURCENOEXCL);
  }

  //
  // 4) disastrOS_mq_send / mq_recv basic
  //
  // send up to capacity
  for (int i = 0; i < 3; ++i) {
    total++;
    rc = disastrOS_mq_send(fd1, (void*)(intptr_t)(0x100 + i), sizeof(void*));
    if (rc == 0) {
      printf("PASS 4.%d: mq_send #%d returned 0\n", i+1, i);
      passed++;
    } else {
      printf("FAIL 4.%d: mq_send #%d returned %d\n", i+1, i, rc);
    }
  }
  // one more send should block — simulate by checking return!=0
  total++;
  rc = disastrOS_mq_send(fd1, (void*)(intptr_t)0x999, sizeof(void*));
  if (rc != 0) {
    printf("PASS 4.4: mq_send over‐capacity returned error %d\n", rc);
    passed++;
  } else {
    printf("FAIL 4.4: mq_send over‐capacity returned 0\n");
  }

  // recv one message
  {
    total++;
    void* msg = NULL;
    rc = disastrOS_mq_recv(fd1, &msg, sizeof(void*));
    if (rc == 0) {
      printf("PASS 5: mq_recv returned 0, msg=0x%p\n", msg);
      passed++;
    } else {
      printf("FAIL 5: mq_recv returned %d\n", rc);
    }
  }

  //
  // 6) disastrOS_mq_close: last descriptor frees resource
  //
  // Before close, resource should exist
  total++;
  Resource* r = ResourceList_byId(&resources_list, 10);
  if (r) {
    printf("PASS 6.1: resource exists before close\n");
    passed++;
  } else {
    printf("FAIL 6.1: resource missing before close\n");
  }

  rc = disastrOS_mq_close(fd1);
  total++;
  if (rc == 0) {
    printf("PASS 6.2: mq_close returned 0\n");
    passed++;
  } else {
    printf("FAIL 6.2: mq_close returned %d\n", rc);
  }

  // After close, resource should be gone
  total++;
  r = ResourceList_byId(&resources_list, 10);
  if (!r) {
    printf("PASS 6.3: resource freed after close\n");
    passed++;
  } else {
    printf("FAIL 6.3: resource still present after close\n");
  }

  //
  // 7) disastrOS_mq_unlink: create, then unlink
  //
  // create new queue key=20
  total++;
  int fd2 = disastrOS_mq_open(20, DSOS_CREATE, 2);
  if (fd2 >= 0) {
    printf("PASS 7.1: mq_open CREATE key=20 fd=%d\n", fd2);
    passed++;
  } else {
    printf("FAIL 7.1: mq_open CREATE key=20 returned %d\n", fd2);
  }

  // unlink
  total++;
  rc = disastrOS_mq_unlink(20);
  if (rc == 0) {
    printf("PASS 7.2: mq_unlink returned 0\n");
    passed++;
  } else {
    printf("FAIL 7.2: mq_unlink returned %d\n", rc);
  }

  // resource gone
  total++;
  r = ResourceList_byId(&resources_list, 20);
  if (!r) {
    printf("PASS 7.3: resource freed after unlink\n");
    passed++;
  } else {
    printf("FAIL 7.3: resource still present after unlink\n");
  }

  // summary
  printf("=== Test summary: %d/%d passed ===\n", passed, total);

  disastrOS_shutdown();
}

int main() {
  printf("[main] Kernel‐inner full MQ syscall test\n");
  disastrOS_start(initFunction, 0, /*no logfile*/0);
  return 0;
}
