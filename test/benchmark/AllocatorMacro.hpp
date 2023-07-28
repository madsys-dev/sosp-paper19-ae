#ifndef ALLOCATOR_MACRO
#define ALLOCATOR_MACRO

#include <cassert>
#include <cstdlib>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <numaif.h>

#ifndef THREAD_PINNING
#define THREAD_PINNING
#endif

#ifdef THREAD_PINNING
// current pinning map.
#define PINNING_MAP pinning_map_2x20a_1
// thread pinning strategy for 2x20a:
// 1 thread per core on one socket -> hyperthreads on the same socket -> cross socket.
static const int pinning_map_2x20a_1[] = {
 	0,2,4,6,8,10,12,14,16,18,
 	20,22,24,1,3,5,32,34,36,38,
 	40,42,44,46,96,98,100,104,106,108,
 	110,112,114,116,118,120,122,124,126,128,
 	130,132,134,136,138,140,142,1,3,5,
 	7,9,11,13,15,17,19,21,23,25,
 	27,29,31,33,35,37,39,41,43,45,
 	47,97,99,101,103,105,107,109,111,113,
	115,117,119,121,123,125,127,129,131,133,135,137,139,141,143};

// thread pinning strategy for 2x20a:
// 5 cores on one socket -> 5 cores on the other ----> hyperthreads
static const int pinning_map_2x20a_2[] = {
	0,2,4,6,8,1,3,5,7,9,
	10,12,14,16,18,11,13,15,17,19,
	20,22,24,26,28,21,23,25,27,29,
	30,32,34,36,38,31,33,35,37,39,
	40,42,44,46,48,41,43,45,47,49,
	50,52,54,56,58,51,53,55,57,59,
	60,62,64,66,68,61,63,65,67,69,
	70,72,74,76,78,71,73,75,77,79};

// thread pinning strategy for 2x10c:
// 1 thread per core on one socket -> hyperthreads on the same socket -> cross socket.
static const int pinning_map_2x10c[] = {
 	0,2,4,6,8,10,12,14,16,18,
 	20,22,24,26,28,30,32,34,36,38,
 	1,3,5,7,9,11,13,15,17,19,
 	21,23,25,27,29,31,33,35,37,39};

#endif
volatile static int init_count = 0;

#define REGION_SIZE (6*1024*1024*1024ULL + 24)

#include "cxlmalloc.h"
#include "cxlmalloc-internal.h" 
#include "numa-config.h"

extern void* roots[1024];
inline CXLRef* pm_malloc(size_t s, cxl_shm* shm) { return new CXLRef(shm->cxl_malloc(s, 0)); }
inline void pm_free(CXLRef* p) 
{ 
    delete p;
    p = NULL;
}

inline int pm_init(size_t& length, int& shm_id) 
{ 
	length = (ZU(1) << 34);
    shm_id = shmget(10, length, IPC_CREAT|0664);
    void* data = shmat(shm_id, NULL, 0);

	int numaNode = 1;
    unsigned long nodemask = 0;
    nodemask |= 1 << numaNode;
    if(mbind(data, length, MPOL_BIND, &nodemask, sizeof(nodemask) * 8, 0) < 0) {
        printf( "mbind buf failed: %s\n", strerror(errno) );
        exit(1);
    }

    memset(data, 0, length);
    shmdt(data);
	return 0;
}

inline int pm_init(size_t& length, void **cxl_mem) 
{
	length = (ZU(1) << 34);
	void *data = get_cxl_mm(length);
	memset(data, 0, length);
	*cxl_mem = data;
	return 0;
}

inline void pm_close() { return; }
template<class T>
inline T* pm_get_root(unsigned int i){
return (T*)roots[i];
}
inline void pm_set_root(void* ptr, unsigned int i) { roots[i] = ptr; }

inline int get_core_id(size_t thread_id)
{
#ifdef USE_CXL_DEV
  int socket = thread_id / NUM_PHYSICAL_CPU_PER_SOCKET;
  int physical_cpu = thread_id % NUM_PHYSICAL_CPU_PER_SOCKET;
  int smt = 0;
  if(thread_id >= (NUM_PHYSICAL_CPU_PER_SOCKET * NUM_SOCKET * SMT_LEVEL)/2) {
          smt = 1;
          socket = socket % NUM_SOCKET;
  }
  return OS_CPU_ID[socket][physical_cpu][smt];
#else
  int socket = thread_id / NUM_PHYSICAL_CPU_PER_SOCKET;
  int physical_cpu = thread_id % NUM_PHYSICAL_CPU_PER_SOCKET;
  int smt = 0;
  if(thread_id >= (NUM_PHYSICAL_CPU_PER_SOCKET * (NUM_SOCKET-1) * SMT_LEVEL)/2) {
          smt = 1;
          socket = socket % NUM_SOCKET;
  }
  return OS_CPU_ID[socket][physical_cpu][smt];
#endif
}


#endif
