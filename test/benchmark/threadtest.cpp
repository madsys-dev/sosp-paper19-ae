#include <iostream>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


#include "fred.h"
#include "timer.h"
#include "AllocatorMacro.hpp"


// #define USE_CXL_DEV

int niterations = 1000;	// Default number of iterations.
int nobjects = 100000;  // Default number of objects.
int nthreads = 64;	// Default number of threads.
int work = 0;		// Default number of loop iterations.
int sz = 8;

class Foo {
public:
  Foo (void)
    : x (14),
      y (29)
    {}

  int x;
  int y;
};


size_t length;
int shm_id;
void *cxl_mem;

extern "C" void * worker (void * arg)
{
#ifdef THREAD_PINNING
    int task_id;
    int core_id;
    cpu_set_t cpuset;
    int set_result;
    int get_result;
    CPU_ZERO(&cpuset);
    task_id = *(int*)arg;
    // core_id = PINNING_MAP[task_id%80];
    core_id = get_core_id(task_id);
    CPU_SET(core_id, &cpuset);
    set_result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (set_result != 0){
    	fprintf(stderr, "setaffinity failed for thread %d to cpu %d\n", task_id, core_id);
	exit(1);
    }
    get_result = pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (set_result != 0){
    	fprintf(stderr, "getaffinity failed for thread %d to cpu %d\n", task_id, core_id);
	exit(1);
    }
    if (!CPU_ISSET(core_id, &cpuset)){
   	fprintf(stderr, "WARNING: thread aiming for cpu %d is pinned elsewhere.\n", core_id);	 
    } else {
    	// fprintf(stderr, "thread pinning on cpu %d succeeded.\n", core_id);
    }
#endif
  int i, j;
#ifdef USE_CXL_DEV
	cxl_shm* shm = new cxl_shm(length, cxl_mem);
#else
  cxl_shm* shm = new cxl_shm(length, shm_id);
#endif
  shm->thread_init();
  CXLRef * a = (CXLRef *) malloc(sizeof(CXLRef) * (nobjects / nthreads));
  for (j = 0; j < niterations; j++) {

    // printf ("%d\n", j);
    for (i = 0; i < (nobjects / nthreads); i ++) {
      new(a+i)CXLRef(shm->cxl_malloc(sz*sizeof(Foo), 0));
      for (volatile int d = 0; d < work; d++) {
	volatile int f = 1;
	f = f + f;
	f = f * f;
	f = f + f;
	f = f * f;
      }
      assert (a+i);
    }

    for (i = 0; i < (nobjects / nthreads); i ++) {
      (a+i)->~CXLRef_s();
      for (volatile int d = 0; d < work; d++) {
	volatile int f = 1;
	f = f + f;
	f = f * f;
	f = f + f;
	f = f * f;
      }
    }
  }

  free(a);


  return NULL;
}

int main (int argc, char * argv[])
{
  HL::Fred * threads;
  //pthread_t * threads;

  if (argc >= 2) {
    nthreads = atoi(argv[1]);
  }

  if (argc >= 3) {
    niterations = atoi(argv[2]);
  }

  if (argc >= 4) {
    nobjects = atoi(argv[3]);
  }

  if (argc >= 5) {
    work = atoi(argv[4]);
  }

  if (argc >= 6) {
    sz = atoi(argv[5]);
  }

#ifdef USE_CXL_DEV
	pm_init(length, &cxl_mem);
#else
  pm_init(length, shm_id);
#endif

  printf ("Running threadtest for %d threads, %d iterations, %d objects, %d work and %d sz...\n", nthreads, niterations, nobjects, work, sz);


  threads = new HL::Fred[nthreads];
  // threads = new hoardThreadType[nthreads];
  //  hoardSetConcurrency (nthreads);

  HL::Timer t;
  //Timer t;

  t.start ();

  int i;
  int *threadArg = (int*)malloc(nthreads*sizeof(int));
  for (i = 0; i < nthreads; i++) {
    threadArg[i] = i+26;
    threads[i].create (worker, &threadArg[i]);
  }

  for (i = 0; i < nthreads; i++) {
    threads[i].join();
  }
  t.stop ();

  printf( "Time elapsed = %f\n", (double) t);

  delete [] threads;
  pm_close();
  shmctl(shm_id, IPC_RMID, NULL);
  return 0;
}

