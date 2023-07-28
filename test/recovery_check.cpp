#include <stdio.h>
#include "cxlmalloc.h"
#include "cxlmalloc-internal.h"
#include "recovery.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <random>
#include <sys/time.h>

int main()
{
    size_t length = (ZU(1) << 33);
    #ifdef USE_CXL_DEV
    // todo: link cxl memory same as recovery_test_time
    void *cxl_mem = get_cxl_mm(length);
    monitor m (length, cxl_mem);
    #else
    int shm_id = shmget(ftok(".", 1), length, IPC_CREAT);
    if(shm_id == -1)
    {
        perror("shmget:");
    }
    monitor m (length, shm_id);
    #endif

    struct timeval starttime,endtime;
    gettimeofday(&starttime,NULL);
    m.single_recovery_loop();
    gettimeofday(&endtime,NULL);
    double timeuse = 1000000*(endtime.tv_sec - starttime.tv_sec) + (endtime.tv_usec - starttime.tv_usec);
	timeuse /= 1000000;
    fprintf(stdout, "time use = %lf\n", timeuse);
    m.check_recovery();
#ifndef USE_CXL_DEV
    shmctl(shm_id, IPC_RMID, NULL);
#endif
    return 0;
}