#pragma once
#ifndef CXLMALLOC_INTERNAL_H
#define CXLMALLOC_INTERNAL_H

#include "cxlmalloc-types.h"
#include <string.h>
#include <net/if.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>


# define FLUSH(addr) asm volatile ("clwb (%0)" :: "r"(addr))
# define FENCE  asm volatile ("sfence" ::: "memory")

# define CXL_DAX_DEV "/dev/dax0.0"

#ifdef FAULT_INJECTION
    #define POTENTIAL_FAULT    \
        if(dis(mt_rand) == 1)  \
        {                      \
            FENCE;             \
            _exit(0);           \
            FENCE;             \
        }                      
#else
    #define POTENTIAL_FAULT
#endif

#ifdef FAULT_INJECTION
    #define POTENTIAL_FAULT_REF    \
        if(shm->dis(shm->mt_rand) == 1)  \
        {                      \
            FENCE;             \
            _exit(0);           \
            FENCE;             \
        }                      
#else
    #define POTENTIAL_FAULT_REF
#endif

static inline void *get_cxl_mm(size_t mmap_size) {

    int dev_fd = open(CXL_DAX_DEV, O_RDWR);
    if (dev_fd <= 0) {
	perror("file error\n");
    	exit(-1);
    }

    void *buf = NULL;
    buf = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, dev_fd, 0);
    if (buf == MAP_FAILED) {
        perror("mmap");
        printf("ERROR: %d\n", errno);
        exit(-1);
    }
    fprintf(stdout, "CXL DEV is open\n");
    return buf;
}

static inline void* get_data_at_addr(void* p, uint64_t offset) 
{
    return (void*)((char*)p + offset);
}
static inline uint64_t get_offset_for_data(void* p, void* data)
{
    return (char*) data - (char*) p;
}

static inline uint64_t get_mac() {
    struct ifreq ifreq;
    int sock;
    if ((sock = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror ("socket");
        return -1;
    }
    strcpy (ifreq.ifr_name, "eth0");    //Currently, only get eth0
    /*
    if (ioctl (sock, SIOCGIFHWADDR, &ifreq) < 0)
    {
        perror ("ioctl");
        return -1;
    }
    */
    char mac[18];
    snprintf (mac, sizeof(mac), "%X:%X:%X:%X:%X:%X", (unsigned char) ifreq.ifr_hwaddr.sa_data[0], (unsigned char) ifreq.ifr_hwaddr.sa_data[1], 
                                            (unsigned char) ifreq.ifr_hwaddr.sa_data[2], (unsigned char) ifreq.ifr_hwaddr.sa_data[3], 
                                            (unsigned char) ifreq.ifr_hwaddr.sa_data[4], (unsigned char) ifreq.ifr_hwaddr.sa_data[5]);

    unsigned u[6];
    sscanf(mac,"%x:%x:%x:%x:%x:%x",u,u+1,u+2,u+3,u+4,u+5);
    uint64_t result = 0;
    for(int i=0;i<6;i++)
    {
        result = (result<<8) + u[i]; 
    }
    return result;
}



static inline cxl_page_t* cxl_segment_page_of(cxl_segment_t* segment, void* p)
{
    ptrdiff_t diff = (uint8_t*)p - (uint8_t*)segment;
    size_t idx = (size_t)diff >> PAGE_SHIFT;
    return &segment->meta_page[idx];
}

static inline void* cxl_page_start(cxl_segment_t* segment, cxl_page_t* page)
{
    ptrdiff_t idx = page - segment->meta_page;
    return(void*)((char*)segment + (idx*PAGE_SIZE));
}

static inline cxl_block* cxl_page_block_at(void* page_start, uint64_t block_size, uint64_t i)
{
    return (cxl_block*)((char*)page_start + (i * block_size));
}

static inline uint64_t pack_machine_process_id(uint64_t machine_id, uint16_t process_id)
{
    return (machine_id << 16) + process_id;
}

static inline uint64_t pack_ref_info(uint64_t lcid, uint64_t ref_cnt, uint32_t lenum)
{
    return (lcid << 48) + (ref_cnt << 32) + lenum;
}

static inline uint64_t get_ref_cnt_from_info(uint64_t ref_info)
{
    uint64_t mask = (ZU(1)<<16) - 1;
    return (ref_info >> 32) & mask;
}

static inline uint64_t get_lenum_from_info(uint64_t ref_info)
{
    uint64_t mask = (ZU(1)<<32) - 1;
    return ref_info & mask;
}

static inline uint64_t get_lcid_from_info(uint64_t ref_info)
{
    uint64_t mask = (ZU(1)<<16) - 1;
    return (ref_info >> 48) & mask;
}
#endif