#include "cxlmalloc.h"
#include "cxlmalloc-internal.h"

cxl_page_t* cxl_shm::cxl_find_page(cxl_page_queue_t* pq)
{
    POTENTIAL_FAULT
    cxl_page_t* page = get_page_by_offset(pq->first);
    POTENTIAL_FAULT
    if(page != NULL)
    {
        POTENTIAL_FAULT
        cxl_block* const block = get_block_by_offset(page->free);
        POTENTIAL_FAULT
        if(block != NULL) return page;
        else return NULL;
    }
    return NULL;
}

cxl_block* cxl_shm::cxl_page_malloc(cxl_page_queue_t* pq, cxl_page_t* &page)
{
    POTENTIAL_FAULT
    if(page == NULL)
    {
        return cxl_malloc_generic(pq, page);
    }
    POTENTIAL_FAULT
    cxl_block* block = get_block_by_offset(page->free);
    POTENTIAL_FAULT
    return block;
}

RootRef* cxl_shm::thread_base_ref_alloc()
{
    POTENTIAL_FAULT
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    POTENTIAL_FAULT
    cxl_page_queue_t* pq = tls->cxl_page_queue(true, 16);
    POTENTIAL_FAULT
    cxl_page_t* page = cxl_find_page(pq);
    POTENTIAL_FAULT
    cxl_block* block = cxl_page_malloc(pq, page);
    POTENTIAL_FAULT
    if(block == NULL) return NULL;
    POTENTIAL_FAULT
    RootRef* tbr = block_to_tbr(block, page);
    POTENTIAL_FAULT
    return tbr;
}

CXLRef cxl_shm::cxl_ref_alloc(RootRef* ref, uint64_t block_size, uint64_t embedded_ref_cnt)
{
    POTENTIAL_FAULT
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    POTENTIAL_FAULT
    cxl_page_queue_t* pq = tls->cxl_page_queue(false, block_size);
    POTENTIAL_FAULT
    cxl_page_t* page = cxl_find_page(pq);
    POTENTIAL_FAULT
    cxl_block* block = cxl_page_malloc(pq, page);
    POTENTIAL_FAULT
    uint64_t tbr_offset = get_offset_for_data(start, (void*) ref);
    POTENTIAL_FAULT
    link_block_to_tbr(block, ref);
    POTENTIAL_FAULT
    FLUSH(ref);
    FENCE;
    CXLObj* cxl_obj = block_to_cxlobj(block, page, embedded_ref_cnt);
    POTENTIAL_FAULT
    uint64_t obj_offset = get_offset_for_data(start, (void*) cxl_obj) + sizeof(CXLObj);
    POTENTIAL_FAULT
    return CXLRef(this, tbr_offset, obj_offset);
}

void cxl_shm::cxl_free(bool special, cxl_block* b)
{
    POTENTIAL_FAULT
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    POTENTIAL_FAULT
    cxl_page_t* page = cxl_ptr_page((void*) b);
    POTENTIAL_FAULT
    cxl_segment_t* segment = cxl_ptr_segment((void*) b);
    POTENTIAL_FAULT
    if(!special) memset(b, 0, sizeof(CXLObj));
    else
    {
        POTENTIAL_FAULT
        if(page->block_size == 16) memset(b, 0, sizeof(RootRef));
        else memset(b, 0, sizeof(cxl_message_queue_t));
    }
    POTENTIAL_FAULT
    if(segment->thread_id == thread_id)
    {
        POTENTIAL_FAULT
        b->next = page->local_free;
        POTENTIAL_FAULT
        page->local_free = get_offset_for_data(start, b);
        POTENTIAL_FAULT
        page->used--;
        POTENTIAL_FAULT
        if(page->used == 0)
        {
            POTENTIAL_FAULT
            cxl_page_free(page, tls->cxl_page_queue(special, page->block_size));
            POTENTIAL_FAULT
        }
    }
    else
    {
        POTENTIAL_FAULT
        uint64_t i = (get_offset_for_data(start, segment) - SEGMENTS_AREA_START) / SEGMENT_SIZE;
        POTENTIAL_FAULT
        cxl_segment_allocation_state_t* sas = (cxl_segment_allocation_state_t*)get_data_at_addr(start, i*sizeof(cxl_segment_allocation_state_t));
        POTENTIAL_FAULT
        state_free_info info = sas->info.load();
        POTENTIAL_FAULT
        state_free_info new_info;
        POTENTIAL_FAULT
        uint64_t b_offset = get_offset_for_data(start, b);
        POTENTIAL_FAULT
        do {
            info = sas->info.load();
            POTENTIAL_FAULT
            if(info.state == POTENTIAL_LEAKING) break;
            POTENTIAL_FAULT
            b->next = info.thread_free;
            POTENTIAL_FAULT
            new_info = {info.state, b_offset};
            POTENTIAL_FAULT
        } while (!sas->info.compare_exchange_weak(info, new_info));
        POTENTIAL_FAULT
    }
}

cxl_message_queue_t* cxl_shm::msg_queue_alloc(uint16_t sender_id, uint16_t receiver_id)
{
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    cxl_page_queue_t* pq = tls->cxl_page_queue(true, sizeof(cxl_message_queue_t));
    cxl_page_t* page = cxl_find_page(pq);
    cxl_block* block = cxl_page_malloc(pq, page);
    if(block == NULL)
    {
        return NULL;
    }
    cxl_message_queue_t* q = block_to_msg_queue(block, sender_id, receiver_id);
    return q;
}

cxl_block* cxl_shm::get_block_by_offset(uint64_t block_offset)
{
    return (block_offset == 0 ? NULL : (cxl_block*) get_data_at_addr(start, block_offset));
}