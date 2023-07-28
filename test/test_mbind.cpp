#include <numaif.h>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

int main(int argc, char **argv) {
    size_t length = (1ULL << 34);
    int shm_id = shmget(10, length, IPC_CREAT|0664);
    void* data = shmat(shm_id, NULL, 0);
    if (data == NULL) {
        printf( "mbind buf failed: %s\n", strerror(errno) );
        exit(1);
    } 

    printf("GET SHM MEM\n");

    int numaNode = 1;
    unsigned long nodemask = 0;
    nodemask |= 1 << numaNode;
    if(mbind(data, length, MPOL_BIND, &nodemask, sizeof(nodemask) * 8, 0) < 0) {
        printf( "mbind buf failed: %s\n", strerror(errno) );
        exit(1);
    }

    memset(data, 0, length);

    sleep(100);

    printf("BIND SHM MEM\n");
    shmdt(data);
    return 0;
}