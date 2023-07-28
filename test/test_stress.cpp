#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <atomic>

#include "cxlmalloc.h"
#include "cxlmalloc-internal.h"

static int THREADS = 2;      // more repeatable if THREADS <= #processors
static int SCALE   = 250;      // scaling factor
static int ITER    = 50;      // N full iterations destructing and re-creating all threads

// transfer pointer between threads
#define MAX_THREADS   (100)
static uint64_t queue_addr[MAX_THREADS+1][MAX_THREADS+1];
static std::atomic<bool> status[MAX_THREADS+1];
const uintptr_t cookie = 0xbf58476d1ce4e5b9UL;

size_t length;
int shm_id;

typedef uintptr_t* random_t;

static uintptr_t pick(random_t r) {
  uintptr_t x = *r;
  // by Sebastiano Vigna, see: <http://xoshiro.di.unimi.it/splitmix64.c>
  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9UL;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebUL;
  x ^= x >> 31;
  *r = x;
  return x;
}

static bool chance(size_t perc, random_t r) {
  return (pick(r) % 100 <= perc);
}

static CXLRef* alloc_items(size_t items, random_t r, cxl_shm* shm) {
  if (items==0) items = 1;
  CXLRef* ref = new CXLRef(shm->cxl_malloc(items*sizeof(uintptr_t), 0));
  uintptr_t* p = (uintptr_t*)ref->get_addr();
  if (p != NULL) {
    for (uintptr_t i = 0; i < items; i++) {
      p[i] = (items - i) ^ cookie;
    }
  }
  return ref;
}

static void free_items(void* p) {
  if (p != NULL) {
    uintptr_t* q = (uintptr_t*)p;
    uintptr_t items = (q[0] ^ cookie);
    for (uintptr_t i = 0; i < items; i++) {
      if ((q[i] ^ cookie) != items - i) {
        fprintf(stderr, "memory corruption at block %p at %zu\n", p, i);
        abort();
      }
    }
  }
}

static void stress(intptr_t i) {
  cxl_shm* shm = new cxl_shm(length, shm_id);
  shm->thread_init();
  intptr_t tid = shm->get_thread_id();
  uintptr_t r = ((tid + 1) * 43); // rand();
  const size_t max_item_shift = 4; // 64
  const size_t max_item_retained_shift = max_item_shift + 2;
  uint64_t allocs = 100 * ((size_t)SCALE) * (tid % 8 + 1); // some threads do more
  size_t retain = allocs / 2;
  CXLRef** data = NULL;
  size_t data_size = 0;
  size_t data_top = 0;
  CXLRef** retained = (CXLRef**)malloc(retain*sizeof(CXLRef*));
  size_t retain_top = 0;

  while (allocs > 0 || retain > 0) {
    setvbuf(stdout, NULL, _IONBF, 0);
    // check transfer data
    for(size_t i=1; i<=THREADS; i++)
    {
      if(queue_addr[i][shm->get_thread_id()] != 0)
      {
        CXLRef* ref = new CXLRef(shm->cxl_unwrap(queue_addr[i][shm->get_thread_id()]));
        if(ref->get_addr() != NULL && ref->get_tbr() != NULL)
        {
          data[data_top++] = ref;
          cxl_message_queue_t* q = (cxl_message_queue_t*) get_data_at_addr(shm->get_start(), queue_addr[i][shm->get_thread_id()]);
        }
      }
    }

    if (retain == 0 || (chance(50, &r) && allocs > 0)) {
      // 50%+ alloc
      // std::cout<<"-----50%+ alloc------"<<std::endl;
      setvbuf(stdout, NULL, _IONBF, 0);
      allocs--;
      if (data_top >= data_size) {
        data_size += 100000;
        data = (CXLRef**)realloc(data, data_size * sizeof(CXLRef*));
      }
      data[data_top++] = alloc_items(1ULL << (pick(&r) % max_item_shift), &r, shm);
      // std::cout<<"-----50%+ alloc end------"<<std::endl;
      setvbuf(stdout, NULL, _IONBF, 0);
    }
    else {
      // 25% retain
      // std::cout<<"-----25% retain-----"<<std::endl;
      setvbuf(stdout, NULL, _IONBF, 0);
      retained[retain_top++] = alloc_items( 1ULL << (pick(&r) % max_item_retained_shift), &r, shm);
      retain--;
      // std::cout<<"-----25% retain end-----"<<std::endl;
      setvbuf(stdout, NULL, _IONBF, 0);
    }
    if (chance(66, &r) && data_top > 0) {
      // 66% free previous alloc
      // std::cout<<"-----66% free-----"<<std::endl;
      setvbuf(stdout, NULL, _IONBF, 0);
      size_t idx = pick(&r) % data_top;
      if(data[idx] != NULL) 
      {
        free_items(data[idx]->get_addr());
        delete data[idx];
        data[idx] = NULL;
      }
      // std::cout<<"-----66% free end-----"<<std::endl;
      setvbuf(stdout, NULL, _IONBF, 0);
    }
    if (chance(1, &r) && data_top > 0) {
      // 5% exchange a local pointer with the (shared) transfer buffer
      // std::cout<<"-----1% transfer-----"<<std::endl;
      setvbuf(stdout, NULL, _IONBF, 0);
      size_t thread_to =  pick(&r) % THREADS + 1;
      while(thread_to == shm->get_thread_id()) thread_to =  pick(&r) % THREADS + 1;
      if(status[thread_to]) continue;;
      size_t data_idx = pick(&r) % data_top;
      CXLRef* ref = data[data_idx];
      while(ref == NULL)
      {
        data_idx = pick(&r) % data_top;
        ref = data[data_idx];
      }
      if(queue_addr[shm->get_thread_id()][thread_to] == 0)
        queue_addr[shm->get_thread_id()][thread_to] = shm->create_msg_queue(thread_to);
      shm->sent_to(queue_addr[shm->get_thread_id()][thread_to], *ref);
    }
  }
  // free everything that is left
  status[tid] = true;
  for (size_t i = 0; i < retain_top; i++) {
    free_items(retained[i]->get_addr());
    delete retained[i];
    retained[i] = NULL;
  }
  for (size_t i = 0; i < data_top; i++) {
    if(data[i] != NULL)
    {
        free_items(data[i]->get_addr());
        delete data[i];
        data[i] = NULL;
    }
  }
  free(retained);
  free(data);
  //bench_end_thread();
}

static void run_os_threads(size_t nthreads, void (*entry)(intptr_t tid));

static void test_stress(void) {
  uintptr_t r = rand();
  run_os_threads(THREADS, &stress);
}


int main()
{
    printf("Using %d threads with a %d%% load-per-thread and %d iterations\n", THREADS, SCALE, ITER);
    srand(0x7feb352d);
    memset(queue_addr, 0, sizeof(queue_addr));
    length = (ZU(1) << 32);
    shm_id = shmget(100, length, IPC_CREAT|0664);
    void* data = shmat(shm_id, NULL, 0);
    memset(data, 0, length);
    shmdt(data);
    test_stress();
    shmctl(shm_id, IPC_RMID, NULL);
    return 0;
}

static void (*thread_entry_fun)(intptr_t) = &stress;

static void* thread_entry(void* param) {
  thread_entry_fun((uintptr_t)param);
  return NULL;
}

static void run_os_threads(size_t nthreads, void (*fun)(intptr_t))
{
  thread_entry_fun = fun;
  pthread_t* threads = (pthread_t*)calloc(nthreads,sizeof(pthread_t));
  memset(threads, 0, sizeof(pthread_t) * nthreads);
  memset(status, false, sizeof(status));
  for (size_t i = 0; i < nthreads; i++) {
    pthread_create(&threads[i], NULL, &thread_entry, (void*)i);
  }
  for (size_t i = 0; i < nthreads; i++) {
    pthread_join(threads[i], NULL);
  }
  delete threads;
}


