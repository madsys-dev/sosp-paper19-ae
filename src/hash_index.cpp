#include "cxlmalloc.h"
#include "cxlmalloc-internal.h"

void cxl_shm::put(uint64_t key, uint64_t value)
{
    uint64_t hash_offset = key & ((1ull << 17) - 1);
    uint64_t* entry = ((uint64_t*)get_data_at_addr(start, HASH_TABLE_START + (thread_id-1)*HASH_TABLE_SIZE)) + hash_offset;
    while(*entry != 0)
    {
        uint64_t* data = ((uint64_t*) get_data_at_addr(start, (*entry) + sizeof(CXLObj)));
        if(*data == key)
        {
            *(data+1) = value;
            return;
        }
        entry = data+2;
    }

    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    cxl_page_queue_t* pq = tls->cxl_page_queue(false, 3*sizeof(uint64_t) + sizeof(CXLObj));
    cxl_page_t* page = cxl_find_page(pq);
    cxl_block* block = cxl_page_malloc(pq, page);
    page->used ++;
    page->free = block->next;
    CXLObj* cxl_obj = (CXLObj*) block;
    cxl_obj->ref_info = pack_ref_info(thread_id, 0, *era(thread_id, thread_id));
    cxl_obj->embedded_ref_cnt = 0;
    uint64_t* data = (uint64_t*)(((char*) cxl_obj) + sizeof(CXLObj));
    *data = key;
    *(data+1) = value;
    *(data+2) = 0;
    link_reference(*entry, get_offset_for_data(start, cxl_obj));
}

bool cxl_shm::get(uint64_t key, uint64_t& value)
{
    uint64_t partion_id = (key >> 56);
    uint64_t hash_offset = key & ((ZU(1) << 17) - 1);
    uint64_t* entry = ((uint64_t*)get_data_at_addr(start, HASH_TABLE_START + partion_id*HASH_TABLE_SIZE)) + hash_offset;
    while(*entry != 0)
    {
        uint64_t* data = ((uint64_t*) get_data_at_addr(start, (*entry) + sizeof(CXLObj)));
        if(*data == key)
        {
            value = *(data+1);
            return true;
        }
        entry = data+2;
    }
    return false;
}