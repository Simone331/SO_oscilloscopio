/**
 * disastrOS_test_bqueue_small.c
 *
 * 1) Test "unitari" (no blocchi): create/put/get/delete su una coda cap>0
 * 2) Mini test concorrente: 1 producer + 1 consumer su coda cap=1
 *
 * NOTE:
 * - Non usiamo wait() nel test concorrente: l'init fa polling su una variabile
 *   globale "done" e dorme con disastrOS_sleep(1). Se preferisci il wait()
 *   imposta USE_WAIT=1 e assicurati che le tue wait/exit siano corrette.
 */

#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_constants.h"
#include "bqueue.h"

/* ====================== Parametri ====================== */

#define UNIT_QID   10
#define UNIT_CAP   2

#define CONC_QID   20
#define CONC_CAP   1
#define CONC_VAL   777

#define USE_WAIT   0   // 0 = polling con disastrOS_sleep; 1 = usa disastrOS_wait

/* ====================== Helpers log/test ====================== */

static int tests_run = 0;
static int tests_failed = 0;

static void pass(const char* what){
  ++tests_run;
  printf("[PASS] %s\n", what);
}

static void fail(const char* what, int err){
  ++tests_run; ++tests_failed;
  printf("[FAIL] %s (err=%d)\n", what, err);
}

static void assert_ok(int rc, const char* what){
  (rc==0) ? pass(what) : fail(what, rc);
}

static void assert_neg(int rc, const char* what){
  (rc<0) ? pass(what) : fail(what, rc);
}

static void assert_eq_long(long got, long exp, const char* what){
  if (got==exp) pass(what);
  else {
    ++tests_run; ++tests_failed;
    printf("[FAIL] %s (got=%ld exp=%ld)\n", what, got, exp);
  }
}

static inline void logp(const char* role){
  printf("[%s pid=%d] ", role, disastrOS_getpid());
  fflush(stdout);
}

/* ====================== Mini workers ====================== */

static volatile int done = 0;  // incrementata alla fine da producer/consumer

void producer(void* arg){
  long id = (long) arg;
  disastrOS_sleep(1); // lascio bloccare il consumer prima
  logp("P"); printf("put %d on q=%ld ...\n", CONC_VAL, id);
  int rc = disastrOS_bqPut((int)id, CONC_VAL);
  logp("P"); printf("put rc=%d\n", rc);
  ++done;
  disastrOS_exit(1000);
}

void consumer(void* arg){
  long id = (long) arg;
  long v = -1;
  logp("C"); printf("get on q=%ld ...\n", id);
  int rc = disastrOS_bqGet((int)id, &v);
  logp("C"); printf("get rc=%d, v=%ld\n", rc, v);
  ++done;
  disastrOS_exit(2000 + (int)v);
}

/* ====================== Test 1: unitario ====================== */

static void test_unitario_bq(void){
  printf("\n--- TEST UNITARIO BQ (no blocchi) ---\n");

  // 1) create
  int rc = disastrOS_bqCreate(UNIT_QID, UNIT_CAP);
  assert_ok(rc, "bqCreate(UNIT_QID, UNIT_CAP)");

  // 1b) create duplicato deve fallire (qualsiasi rc<0 va bene)
  rc = disastrOS_bqCreate(UNIT_QID, UNIT_CAP);
  assert_neg(rc, "bqCreate duplicate must fail");

  // 2) due put (cap=2 => non devono bloccare)
  rc = disastrOS_bqPut(UNIT_QID, 111);
  assert_ok(rc, "bqPut(111)");
  rc = disastrOS_bqPut(UNIT_QID, 222);
  assert_ok(rc, "bqPut(222)");

  // 3) due get (deve uscire in FIFO: 111 poi 222)
  long v=-1;
  rc = disastrOS_bqGet(UNIT_QID, &v);
  assert_ok(rc, "bqGet #1 rc");
  assert_eq_long(v, 111, "bqGet #1 value");
  rc = disastrOS_bqGet(UNIT_QID, &v);
  assert_ok(rc, "bqGet #2 rc");
  assert_eq_long(v, 222, "bqGet #2 value");

  // 4) delete (nessun waiter => ok)
  rc = disastrOS_bqDelete(UNIT_QID);
  assert_ok(rc, "bqDelete(UNIT_QID)");

  printf("--- FINE TEST UNITARIO: %d run, %d failed ---\n", tests_run, tests_failed);
}

/* ====================== Test 2: mini concorrente ====================== */

static void test_mini_concorrente(void){
  printf("\n--- TEST MINI CONCORRENTE (1P+1C, cap=1) ---\n");

  // crea coda cap=1
  int rc = disastrOS_bqCreate(CONC_QID, CONC_CAP);
  assert_ok(rc, "bqCreate(CONC_QID, cap=1)");

  // spawna consumer poi producer (producer dorme 1 tick)
  disastrOS_spawn(consumer, (void*) (long) CONC_QID);
  disastrOS_spawn(producer, (void*) (long) CONC_QID);

#if USE_WAIT
  // usa wait() se preferisci (richiede wait/exit corretti)
  for (int k=0; k<2; ++k){
    int ret=0, pid=disastrOS_wait(0, &ret);
    printf("[init] child %d exited, retval=%d\n", pid, ret);
  }
#else
  // polling semplice: attendo che producer e consumer incrementino "done"
  while (done<2){
    disastrOS_sleep(1);
  }
#endif

  // cleanup
  rc = disastrOS_bqDelete(CONC_QID);
  assert_ok(rc, "bqDelete(CONC_QID)");

  printf("--- FINE TEST MINI CONCORRENTE ---\n");
}

/* ====================== init & main ====================== */

void initFunction(void* args){
  printf("\n=== BQueue SMALL TESTS ===\n");
  test_unitario_bq();
  test_mini_concorrente();

  printf("\n=== SUMMARY: %d run, %d failed ===\n", tests_run, tests_failed);
  disastrOS_shutdown();
}

int main(int argc, char** argv){
  char* logfilename=0;
  if (argc>1) logfilename=argv[1];
  printf("start (bqueue small tests)\n");
  disastrOS_start(initFunction, 0, logfilename);
  return 0;
}
