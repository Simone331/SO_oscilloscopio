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

#define USE_WAIT   1   // 0 = polling con disastrOS_sleep; 1 = usa disastrOS_wait

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
  long id=(long)arg;
  
  printf("\n");
  logp("P"); printf("put %d on q=%ld ...\n", CONC_VAL, id);
  disastrOS_bqPut((int)id, CONC_VAL);
  logp("P"); printf("after put dump\n");
  disastrOS_bqDump((int)id);

  disastrOS_exit(1000);
}

void consumer(void* arg){
  long id=(long)arg;
  long v=-1;
  printf("\n");

  logp("C"); printf("get on q=%ld ...\n", id);
  disastrOS_bqGet((int)id, &v);

  logp("C"); printf("after get dump (v=%ld)\n", v);
  disastrOS_bqDump((int)id);

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

/* ====================== Test 2: producer-consumer esteso ====================== */

static void test_mini_concorrente(void){
  printf("\n--- TEST PROD-CONS ESTESO (più processi monoshot) ---\n");

  // Usiamo la coda CONC_QID ma con capacità >1 per vedere buffering + handoff
  const int QID          = CONC_QID;  // 20
  const int CAP          = 3;         // più grande di 1 per mostrare buffer pieno/vuoto
  const int NPROD_PROCS  = 10;         // numero di processi "producer" (ognuno fa 1 put)
  const int NCONS_PROCS  = 8;         // numero di processi "consumer" (ognuno fa 1 get)

  int rc = disastrOS_bqCreate(QID, CAP);
  assert_ok(rc, "bqCreate(CONC_QID, cap=3)");
  if (rc<0) return; // se fallisce, non proseguire

  // Pattern di spawn: metà consumer (per farli bloccare), poi tutti i producer, poi i consumer restanti
  int halfC = NCONS_PROCS/2;

  printf("[init] spawn %d consumer (fase 1)...\n", halfC);
  for (int i=0; i<halfC; ++i)
    disastrOS_spawn(consumer, (void*) (long) QID);

  printf("[init] spawn %d producer...\n", NPROD_PROCS);
  for (int i=0; i<NPROD_PROCS; ++i)
    disastrOS_spawn(producer, (void*) (long) QID);

  printf("[init] spawn %d consumer (fase 2)...\n", NCONS_PROCS - halfC);
  for (int i=halfC; i<NCONS_PROCS; ++i)
    disastrOS_spawn(consumer, (void*) (long) QID);

#if USE_WAIT
  // aspetta la terminazione di TUTTI i figli di questo test
  const int to_wait = NPROD_PROCS + NCONS_PROCS;
  for (int k=0; k<to_wait; ++k){
    int ret=0;
    int pid = disastrOS_wait(0, &ret);
    printf("[init] child %d exited, retval=%d (k=%d/%d)\n", pid, ret, k+1, to_wait);
  }
#else
  // Variante senza wait(): polling su 'done' (incrementato dai worker)
  done = 0;
  const int target_done = NPROD_PROCS + NCONS_PROCS;
  while (done < target_done){
    disastrOS_sleep(1);
  }
#endif

  // delete
  rc = disastrOS_bqDelete(QID);
  assert_ok(rc, "bqDelete(CONC_QID)");

  printf("--- FINE TEST PROD-CONS ESTESO ---\n");
}


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
