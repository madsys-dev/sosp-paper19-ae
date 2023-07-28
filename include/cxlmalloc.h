#pragma once
#ifndef CXLMALLOC_H
#define CXLMALLOC_H

#include "cxlmalloc-types.h"
#ifdef FAULT_INJECTION
#include <random>
#endif

class cxl_shm {
private:
    void* start;
    int shm_id;
    uint64_t size;
    uint64_t tls_offset;
    uint32_t thread_id;
    #ifdef FAULT_INJECTION
    std::mt19937 mt_rand;
    std::uniform_int_distribution<int> dis;
    #endif
public:
    // "cxl_shm.cpp"
    friend class CXLRef_s;
    cxl_shm(uint64_t size, int shm_id);
    cxl_shm(uint64_t size, void *cxl_mem);
    ~cxl_shm();
    void thread_init();
    CXLRef cxl_malloc(uint64_t, uint32_t);
    CXLRef get_ref(uint64_t);
    void* get_start();
    uint32_t get_thread_id();
    void cxl_free(bool special, cxl_block* b);
    void link_reference(uint64_t& ref, uint64_t refed);
    void unlink_reference(uint64_t& ref, uint64_t refed);
    void change_reference(uint64_t& ref, uint64_t refed);

    // "transfer.cpp"
    uint64_t cxl_wrap(CXLRef& ref);
    // uint64_t sent_to(uint16_t dst, uint64_t offset);
    bool sent_to(uint64_t queue_offset, CXLRef& ref);
    CXLRef cxl_unwrap(uint64_t offset);
    uint64_t create_msg_queue(uint16_t dst_id);

    // "hash_index.cpp"
    void put(uint64_t key, uint64_t value);
    bool get(uint64_t key, uint64_t& value);
private:
    // "cxl_shm.cpp"
    void link_block_to_tbr(cxl_block* b, RootRef* tbr);
    CXLObj* block_to_cxlobj(cxl_block* b, cxl_page_t* page, uint64_t embedded_ref_cnt);
    RootRef* block_to_tbr(cxl_block* b, cxl_page_t* page);
    cxl_message_queue_t* block_to_msg_queue(cxl_block* b, uint16_t sender_id, uint16_t receiver_id);
    std::atomic<uint32_t>* era(int x, int y);
    void test_free(uint64_t& ref);
    void change_reference_pure(uint64_t& ref, uint64_t refed);

    // "alloc.cpp"
    cxl_page_t* cxl_find_page(cxl_page_queue_t* pq);
    cxl_block* cxl_page_malloc(cxl_page_queue_t* pq, cxl_page_t* &page);
    RootRef* thread_base_ref_alloc(void);
    CXLRef cxl_ref_alloc(RootRef* ref, uint64_t block_size, uint64_t embedded_ref_cnt);
    cxl_message_queue_t* msg_queue_alloc(uint16_t sender_id, uint16_t receiver_id);
    cxl_block* get_block_by_offset(uint64_t block_offset);

    // "page.cpp"
    cxl_block* cxl_malloc_generic(cxl_page_queue_t* pq, cxl_page_t* &page);
    void cxl_page_free_collect(cxl_page_t* page);
    void cxl_thread_free_collect(cxl_page_t* page);
    cxl_page_t* cxl_page_queue_find_free_ex(cxl_page_queue_t* pq);
    cxl_page_t* cxl_page_fresh(cxl_page_queue_t* pq);
    void cxl_page_init(bool special, cxl_page_t* page, uint64_t block_size);
    void cxl_page_free(cxl_page_t* page, cxl_page_queue_t* pq);
    cxl_page_t* cxl_ptr_page(void* p);
    cxl_page_t* get_page_by_offset(uint64_t page_offset);

    // "segment.cpp"
    cxl_page_t* cxl_segment_page_alloc(uint64_t block_size);
    cxl_segment_t* cxl_segment_alloc(void);
    void cxl_segment_free(cxl_segment_t* segment);
    cxl_segment_t* cxl_ptr_segment(void* p);

    // "transfer.cpp"
    cxl_message_queue_t* find_msg_queue(uint16_t dst_id);

    // "recovery.cpp"
    void normal_garbage_collection();
};

#endif