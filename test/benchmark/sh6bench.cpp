#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <limits.h>
#include <sys/time.h>


// #define USE_CXL_DEV

#ifdef __cplusplus
extern "C"
{
#endif

/* Unix prototypes */
#ifndef UNIX
#define UNIX 1
#endif

#include <unistd.h>
#define _INCLUDE_POSIX_SOURCE
#include <sys/signal.h>
#include <pthread.h>
typedef pthread_t ThreadID;
#include <sys/sysinfo.h>
int thread_specific;

#ifndef THREAD_NULL
#define THREAD_NULL 0
#endif
#ifndef THREAD_EQ
#define THREAD_EQ(a,b) ((a)==(b))
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#include "AllocatorMacro.hpp"

#ifdef SILENT
void fprintf_silent(FILE *, ...);
void fprintf_silent(FILE *x, ...) { (void)x; }
#else
#define fprintf_silent fprintf
#endif

#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif

#ifdef CLK_TCK
#undef CLK_TCK
#endif
#define CLK_TCK CLOCKS_PER_SEC

#define TRUE 1
#define FALSE 0
typedef int Bool;

unsigned uMaxBlockSize = 400;
unsigned uMinBlockSize = 64;
unsigned long ulCallCount = 10000;

unsigned long promptAndRead(char *msg, unsigned long defaultVal, char fmtCh);

unsigned uThreadCount = 32;
ThreadID RunThread(void (*fn)(void *), void *arg);
void WaitForThreads(ThreadID[], unsigned);
int GetNumProcessors(void);



inline uint64_t rdtsc(void) {
	unsigned int hi, lo;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((uint64_t) lo) | (((uint64_t) hi) << 32);
}

const int64_t kCPUSpeed = 2000000000;

void doBench(void *);

pthread_barrier_t barrier;

size_t length;
int shm_id;
void *cxl_mem;
struct timeval starttime,endtime;

int main(int argc, char *argv[])
{
    clock_t startCPU;
	time_t startTime;
	double elapsedTime, cpuTime;

	uint64_t start_;
	uint64_t end_;

	if (argc >= 2) {
    	ulCallCount = atoi(argv[1]);
  	}

	if (argc >= 3) {
		uMinBlockSize = atoi(argv[2]);
	}

	if (argc >= 4) {
		uMaxBlockSize = atoi(argv[3]);
	}

	if (argc >= 5) {
		uThreadCount = atoi(argv[4]);
	}

	unsigned i;
	int *threadArg = (int*) malloc(uThreadCount*sizeof(int));
	ThreadID *tids;

	pthread_barrier_init(&barrier,NULL,uThreadCount);

#ifdef USE_CXL_DEV
	pm_init(length, &cxl_mem);
#else
    pm_init(length, shm_id);
#endif

    printf("params: call count: %u, min size: %u, max size: %u, threads: %u\n", ulCallCount, uMinBlockSize, uMaxBlockSize, uThreadCount);
	if (uThreadCount < 1)
		uThreadCount = 1;
	ulCallCount /= uThreadCount;

	
	// struct timeval starttime,endtime;
    if ((tids = (ThreadID*) malloc(sizeof(ThreadID) * uThreadCount)) != NULL){
		// startCPU = clock();
		// startTime = time(NULL);
		// start_ = rdtsc();
		for (i = 0;  i < uThreadCount;  i++){
			threadArg[i] = i;
			if (THREAD_EQ(tids[i] = 
				RunThread(doBench, &threadArg[i]),THREAD_NULL)){
				fprintf(stdout, "\nfailed to start thread #%d", i);
				break;
			}
		}
		WaitForThreads(tids, uThreadCount);
		free(tids);
	}
	if (threadArg)
		free(threadArg);

	gettimeofday(&endtime,NULL);
	// end_ = rdtsc();
	// elapsedTime = difftime(time(NULL), startTime);
	// cpuTime = (double)(clock()-startCPU) / (double)CLOCKS_PER_SEC;
	double timeuse = 1000000*(endtime.tv_sec - starttime.tv_sec) + (endtime.tv_usec - starttime.tv_usec);
	timeuse /= 1000000;

	// fprintf_silent(stdout, "\n");
	// fprintf(stdout, "\nTotal elapsed time"
	// 		  " for %d threads"
	// 		  ": %.2f (%.4f CPU)\n",
	// 		  uThreadCount,
	// 		  elapsedTime, cpuTime);

	// fprintf(stdout, "\nrdtsc time: %f\n", ((double)end_ - (double)start_)/kCPUSpeed);
	fprintf(stdout, "time use = %lf\n", timeuse);

	pm_close();
#ifndef USE_CXL_DEV
    shmctl(shm_id, IPC_RMID, NULL);
#endif
	return 0;
}

void doBench(void *arg)
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
	pthread_barrier_wait(&barrier);

#endif
	if(*(int*)arg == 0) gettimeofday(&starttime,NULL);

#ifdef USE_CXL_DEV
	cxl_shm* shm = new cxl_shm(length, cxl_mem);
#else
    cxl_shm* shm = new cxl_shm(length, shm_id);
#endif
    shm->thread_init();
    CXLRef *memory = (CXLRef *) malloc((ulCallCount+1) * sizeof(CXLRef));
    int	size_base, size, iterations;
	int	repeat = ulCallCount;
	// printf("[repeat]%d\n", repeat);
	CXLRef *mp = memory;
	CXLRef *mpe = memory + ulCallCount;
	CXLRef *save_start = mpe;
	CXLRef *save_end = mpe;
	int count = 0;
	// int counter = 0;
    while (repeat--) {
        for (size_base = uMinBlockSize;
		     size_base < uMaxBlockSize;
		     size_base = size_base * 3 / 2 + 1) {
            for (size = size_base; size >= uMinBlockSize; size /= 2) {
                iterations = 1;
                
                if (size < 10000)
                    iterations = 10;

                if (size < 1000)
                    iterations *= 5;

                if (size < 100)
                    iterations *= 5;

                while (iterations--) {
					// counter ++;
                    // *mp = new CXLRef(shm->cxl_malloc(size, 0));
					new(mp)CXLRef(shm->cxl_malloc(size, 0));
                    if(!memory || (mp->get_addr() == NULL)) {
                        printf("Out of memory\n");
					    _exit (1);
                    }
                    mp++;

                    if (mp == save_start)
                        mp = save_end;
                    
                    if (mp >= mpe) {
                        mp = memory;
                        save_start = save_end;
                        if (save_start >= mpe) save_start = mp;
                        save_end = save_start + (ulCallCount / 5);
                        if (save_end > mpe) save_end = mpe;

                        while (mp < save_start) {
                            // delete (*mp);
							mp->~CXLRef_s();
                            mp++;
                        }
                        mp = mpe;
                        while (mp > save_end) {
                            mp--;
                            // delete (*mp);
							mp->~CXLRef_s();
                        }
                        if(save_start == memory){
                            mp = save_end;
                        } else{
                            mp = memory;
                        }
                    }
                }

            }

        }
    }
    mpe = mp;
    mp = memory;
    while (mp < mpe){
        // delete (*mp);
		mp->~CXLRef_s();
        mp++;
    }
	// printf("[COUNTER]%d\n", counter);

	free(memory);
	delete shm;

}

// unsigned long promptAndRead(char *msg, unsigned long defaultVal, char fmtCh)
// {
// 	char *arg = NULL, *err;
// 	unsigned long result;
// 	{
// 		char buf[12];
// 		static char fmt[] = "\n%s [%lu]: ";
// 		fmt[7] = fmtCh;
// 		fprintf_silent(fout, fmt, msg, defaultVal);
// 		if (fgets(buf, 11, fin))
// 			arg = &buf[0];
// 	}
// 	if (arg && ((result = strtoul(arg, &err, 10)) != 0
// 					|| (*err == '\n' && arg != err))){
// 		return result;
// 	}
// 	else
// 		return defaultVal;
// }

ThreadID RunThread(void (*fn)(void *), void *arg)
{
	ThreadID result = THREAD_NULL;
	
	pthread_attr_t attr;
	pthread_attr_init(&attr);
    if (pthread_create(&result, &attr, (void *(*)(void *))fn, arg) == -1)
		return THREAD_NULL;
	return result;
}

/* wait for all benchmark threads to terminate */
void WaitForThreads(ThreadID tids[], unsigned tidCnt)
{
	while (tidCnt--)
		pthread_join(tids[tidCnt], NULL);
}

/* return the number of processors present */
int GetNumProcessors()
{
	return get_nprocs();
}