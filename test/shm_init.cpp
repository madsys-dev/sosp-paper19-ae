#include <stdio.h>
#include "cxlmalloc.h"
#include "cxlmalloc-internal.h"
#include "recovery.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <vector>

int nthreads = 1;	

size_t length;
int shm_id;


int main()
{
    size_t length = (ZU(1) << 33);
    size_t length_queue = (ZU(1) << 17);

    int shm_id = shmget(ftok(".", 1), length, IPC_CREAT|0664);
    if(shm_id == -1)
    {
        perror("shmget:");
    }
    void* data = shmat(shm_id, NULL, 0);
    memset(data, 0, length);
    shmdt(data);

    int shm_id_queue = shmget(100, length_queue, IPC_CREAT|0664);
    if(shm_id_queue == -1)
    {
        perror("shmget2:");
    }
    void* queue_offset = shmat(shm_id_queue, NULL, 0);
    memset(queue_offset, 0, length_queue);
    shmdt(queue_offset);
    return 0;
}