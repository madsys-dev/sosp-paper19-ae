#pragma once
#ifndef CXLMALLOC_TYPES_H
#define CXLMALLOC_TYPES_H

#include <stdio.h>
#include <stddef.h>   // ptrdiff_t
#include <stdint.h>   // uintptr_t, uint16_t, etc
#include <functional>
#include <iostream>
#include <atomic>


# define ZU(x)  x##ULL
# define ZI(x)  x##LL

# define INTPTR_SHIFT   (3)
# define PAGE_SHIFT     (13 + INTPTR_SHIFT)  // 64KiB
# define SEGMENT_SHIFT  (10 + PAGE_SHIFT)    // 64MiB

# define INTPTR_SIZE    (1<<INTPTR_SHIFT)
# define PAGE_SIZE      (ZU(1)<<PAGE_SHIFT)
# define SEGMENT_SIZE   (ZU(1)<<SEGMENT_SHIFT)
# define SEGMENT_MASK   (SEGMENT_SIZE - 1)

# define PAGES_PER_SEGMENT    (SEGMENT_SIZE / PAGE_SIZE) // 1024

# define SEGMENT_ALLOCATION_VEC_SIZE (ZU(1)<<22)
# define ERA_ARRAY_START             (ZU(1)<<22)
# define THREAD_LOCAL_VEC_START      (ZU(1)<<23)
# define HASH_TABLE_START            (ZU(1)<<26)
# define HASH_TABLE_SIZE             (ZU(1)<<20)
# define SEGMENTS_AREA_START         (ZU(1)<<27)

# define CXL_BIN_SIZE   (66U)

# define MESSAGE_BUFFER_SIZE (16)

# define MAX_THREAD (1024)
# define MAX_SEGMENT_NUM (4096)
# define REDO_CACHELINE_CNT (8)


class cxl_shm;
typedef class cxl_message_queue_s cxl_message_queue_t;

typedef struct cxl_block_s {
    // 组织 free list
    uint64_t next;
} cxl_block;


typedef struct CXLObj_s {
    uint64_t next;
    std::atomic<uint64_t> ref_info;          // 8 bytes for lcid(2), ref_cnt(2), lenum(4) 
    uint64_t embedded_ref_cnt;
} CXLObj;



typedef struct __attribute__ ((aligned(16))) RootRef_s {
    uint64_t pptr; 
    bool     in_use;
    uint16_t ref_cnt;
} RootRef;

typedef class CXLRef_s {
public:
    cxl_shm*       shm;
    uint64_t       tbr;
    uint64_t       data;
public:
    CXLRef_s(cxl_shm* _shm, uint64_t _tbr, uint64_t _data);
    CXLRef_s(const CXLRef_s& cxl_ref);
    CXLRef_s(const CXLRef_s&& cxl_ref);
    CXLRef_s& operator=(const CXLRef_s& cxl_ref);
    CXLRef_s& operator=(const CXLRef_s&& cxl_ref);
    ~CXLRef_s();
    void* get_addr();
    RootRef* get_tbr();
} CXLRef;

typedef struct cxl_page_s {
    uint64_t   free;
    uint64_t   local_free; 

    uint32_t   block_size;
    uint32_t   used;

    uint64_t   next;
    uint64_t   prev;

    bool       is_msg_queue_page;
} cxl_page_t;

typedef struct cxl_segment_s {
    uint16_t   thread_id;
    uint32_t   used;
    cxl_page_t meta_page[PAGES_PER_SEGMENT];
} cxl_segment_t;


typedef class cxl_page_queue_s {
public:
    uint64_t    first;
    uint64_t    last;
    size_t      block_size;
public:
    void cxl_page_queue_push(cxl_page_t* page, void* start);
    void cxl_page_queue_remove(cxl_page_t* page, void* start);
} cxl_page_queue_t;

enum redo_func_id {
    LINK_REF = 1,
    UNLINK_REF = 2,
};

struct redo_log {
    uint16_t func_id;
    uint16_t saved_ref_cnt;
    uint32_t cur_era;
    uint64_t ref;
    uint64_t refed;
    uint64_t old_refed;
};

typedef class __attribute__ ((aligned(64))) cxl_thread_local_state_s {
public:
    // redo_log redo;
    char redo[64*REDO_CACHELINE_CNT];
    std::atomic<uint64_t>   packed_machine_process_id; // 6 bytes for machine(mac), 2 bytes for process

    cxl_page_queue_t pages[CXL_BIN_SIZE]; 

    cxl_page_queue_t free_page; 

    uint64_t sender_queue;
    uint64_t receiver_queue;
public:
    cxl_page_queue_t* cxl_page_queue(bool is_tbr, uint64_t size);
} cxl_thread_local_state_t;


struct state_free_info
{
    uint64_t state;
    uint64_t thread_free;
};

typedef struct cxl_segment_allocation_state_s {
    std::atomic<uint32_t> thread_id;
    uint32_t ver;
    std::atomic<state_free_info> info;
} cxl_segment_allocation_state_t;

struct __attribute__ ((aligned(64))) cxl_message_queue_s {
    volatile size_t start;
    volatile size_t end;
    uint16_t sender_id;
    uint16_t receiver_id;
    uint64_t sender_next;
    uint64_t receiver_next;
    uint64_t buffer[MESSAGE_BUFFER_SIZE];
};

enum segment_state
{
    NORMAL = 0,
    ABANDON = 1,
    POTENTIAL_LEAKING = 2,
};

#endif