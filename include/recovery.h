#pragma once
#ifndef RECOVERY_H
#define RECOVERY_H

#include "cxlmalloc-types.h"

class monitor {
private:
    void* start;
    int   shm_id;
    uint64_t size;
private:
    std::atomic<uint32_t>* era(int x, int y);
    cxl_page_t* cxl_ptr_page(void* p);
    cxl_segment_t* cxl_ptr_segment(void* p);
public:
    friend class CXLRef_s;
    monitor(uint64_t size, int shm_id);
    monitor(uint64_t size, void* cxl_mem);
    ~monitor();

    // "recovery.cpp"
    void normal_garbage_collection();
    void redo_ref_cnt(uint64_t offset);
    void recovery_garbage_collection(uint64_t offset, uint64_t packed_machine_process_id);
    void recovery_test_free(uint64_t& ref, uint64_t offset);
    void recovery_unlink_reference(uint64_t& ref, uint64_t refed, uint64_t offset);
    void recovery_cxl_free(bool special, cxl_block* b, uint64_t offset);
    void recovery_cxl_page_free(cxl_page_t* page, cxl_page_queue_t* pq, uint64_t offset);
    void recovery_cxl_segment_free(cxl_segment_t* segment, uint64_t offset);
    void recovery_loop();
    void single_recovery_loop();
    bool check_tls(uint64_t id);
    bool check_recovery();

    cxl_page_t* get_page_by_offset(uint64_t page_offset);
    cxl_block* get_block_by_offset(uint64_t block_offset);
};

#endif