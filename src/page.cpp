#include "cxlmalloc.h"
#include "cxlmalloc-internal.h"


// slow path for cxl malloc, need to find a page with free block
// or find a new page, or a new segment 
cxl_block* cxl_shm::cxl_malloc_generic(cxl_page_queue_t* pq, cxl_page_t* &page)
{
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    POTENTIAL_FAULT
    memset(tls->redo, 0, sizeof(tls->redo));
    POTENTIAL_FAULT
    for(size_t i=0; i<REDO_CACHELINE_CNT; i++)
    {
        POTENTIAL_FAULT
        FLUSH(tls->redo + 64*i);
    }
    POTENTIAL_FAULT
    page = cxl_page_queue_find_free_ex(pq);
    POTENTIAL_FAULT
    if(page == NULL) 
    {
        return NULL;
    }
    else
    {
        POTENTIAL_FAULT
        return cxl_page_malloc(pq, page);
    }
}

// collect all frees to ensure up-to-date `used` count and find free block
void cxl_shm::cxl_page_free_collect(cxl_page_t* page)
{
    POTENTIAL_FAULT
    cxl_thread_free_collect(page);
    POTENTIAL_FAULT
    if(page->local_free != 0)
    {
        POTENTIAL_FAULT
        if(page->free == 0)
        {
            POTENTIAL_FAULT
            page->free = page->local_free;
            POTENTIAL_FAULT
            page->local_free = 0;
            POTENTIAL_FAULT
        }
    }
}

// Collect the local `thread_free` list using an atomic exchange.
void cxl_shm::cxl_thread_free_collect(cxl_page_t* page)
{
    POTENTIAL_FAULT
    cxl_segment_t* segment = cxl_ptr_segment(page);
    POTENTIAL_FAULT
    uint64_t idx = (get_offset_for_data(start, segment) - SEGMENTS_AREA_START) / SEGMENT_SIZE;
    POTENTIAL_FAULT
    cxl_segment_allocation_state_t* sas = (cxl_segment_allocation_state_t*)get_data_at_addr(start, idx * sizeof(cxl_segment_allocation_state_t));

    POTENTIAL_FAULT
    state_free_info info = sas->info.load();
    POTENTIAL_FAULT
    state_free_info new_info;
    POTENTIAL_FAULT
    if(info.thread_free == 0) return;
    POTENTIAL_FAULT
    do {
        POTENTIAL_FAULT
        info = sas->info.load();
        POTENTIAL_FAULT
        new_info = {info.state, 0};
        POTENTIAL_FAULT
    } while(!sas->info.compare_exchange_weak(info, new_info));
    POTENTIAL_FAULT

    uint64_t head_offset = info.thread_free;
    POTENTIAL_FAULT
    cxl_block* head = (head_offset == 0)? NULL : (cxl_block*) get_data_at_addr(start, head_offset);
    POTENTIAL_FAULT
    if(head == NULL) return;
    POTENTIAL_FAULT
    cxl_block* tail = head;
    POTENTIAL_FAULT
    cxl_block* next;
    while(tail != NULL)
    {
        POTENTIAL_FAULT
        next = tail->next == 0? NULL: (cxl_block*)get_data_at_addr(start, tail->next);
        POTENTIAL_FAULT
        cxl_page_t* p = cxl_ptr_page(tail);
        POTENTIAL_FAULT
        tail->next = p->local_free;
        POTENTIAL_FAULT
        p->local_free = get_offset_for_data(start, tail);
        POTENTIAL_FAULT
        p->used -= 1;
        POTENTIAL_FAULT
        tail = next;
        POTENTIAL_FAULT
    }

}

// Find a page with free blocks of `page->block_size`.
cxl_page_t* cxl_shm::cxl_page_queue_find_free_ex(cxl_page_queue_t* pq)
{
    POTENTIAL_FAULT
    cxl_page_t* page = pq->first == 0 ? NULL : (cxl_page_t*)get_data_at_addr(start, pq->first);
    POTENTIAL_FAULT
    while (page != NULL)
    {
        POTENTIAL_FAULT
        cxl_page_t* next = get_page_by_offset(page->next);
        POTENTIAL_FAULT
        cxl_page_free_collect(page);
        POTENTIAL_FAULT
        if(page->free != 0)
        {
            break;
        }
        POTENTIAL_FAULT
        page = next;
    }

    if(page == NULL)
    {
        POTENTIAL_FAULT
        page = cxl_page_fresh(pq);
        POTENTIAL_FAULT
    }

    return page;
}


// Get a fresh page to use
cxl_page_t* cxl_shm::cxl_page_fresh(cxl_page_queue_t* pq)
{
    POTENTIAL_FAULT
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    POTENTIAL_FAULT
    bool special = false;
    POTENTIAL_FAULT
    if(pq == &tls->pages[0] || pq == &tls->pages[1]) special = true;
    POTENTIAL_FAULT
    cxl_page_t* page = cxl_segment_page_alloc(pq->block_size);
    POTENTIAL_FAULT
    cxl_page_init(special, page, pq->block_size);
    POTENTIAL_FAULT
    if(pq == &tls->pages[1])
        page->is_msg_queue_page = true;
    POTENTIAL_FAULT
    FLUSH(page);
    FENCE;
    POTENTIAL_FAULT
    pq->cxl_page_queue_push(page, start);
    POTENTIAL_FAULT
    return page;
}

// Initialize a fresh page
void cxl_shm::cxl_page_init(bool special, cxl_page_t* page, uint64_t block_size)
{
    POTENTIAL_FAULT
    page->local_free = 0;
    POTENTIAL_FAULT
    page->block_size = block_size;
    POTENTIAL_FAULT
    page->used = 0;
    POTENTIAL_FAULT
    page->next = 0;
    POTENTIAL_FAULT
    page->prev = 0;
    POTENTIAL_FAULT
    page->is_msg_queue_page = false;
    POTENTIAL_FAULT


    void* page_area = cxl_page_start(cxl_ptr_segment(page), page);
    POTENTIAL_FAULT
    cxl_block* start_block = cxl_page_block_at(page_area, block_size, 0);
    POTENTIAL_FAULT
    cxl_block* last_block = cxl_page_block_at(page_area, block_size, PAGE_SIZE / block_size - 1);
    POTENTIAL_FAULT
    cxl_block* block = start_block;
    POTENTIAL_FAULT
    while (block < last_block)
    {
        POTENTIAL_FAULT
        if(special) memset(block, 0, sizeof(block_size));
        else memset(block, 0, sizeof(CXLObj));
        POTENTIAL_FAULT
        cxl_block* next = (cxl_block*)((char*)block + block_size);
        POTENTIAL_FAULT
        block->next = get_offset_for_data(start, next);
        POTENTIAL_FAULT
        block = next;
        POTENTIAL_FAULT
    }
    POTENTIAL_FAULT
    last_block->next = page->free;
    POTENTIAL_FAULT
    page->free = get_offset_for_data(start, start_block);
    POTENTIAL_FAULT
}

// Free a page with no more free blocks
void cxl_shm::cxl_page_free(cxl_page_t* page, cxl_page_queue_t* pq)
{
    POTENTIAL_FAULT
    pq->cxl_page_queue_remove(page, start);
    FENCE;
    POTENTIAL_FAULT
    cxl_segment_t* segment = cxl_ptr_segment((void*) page);
    POTENTIAL_FAULT
    memset(page, 0, sizeof(cxl_page_t));
    POTENTIAL_FAULT
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    POTENTIAL_FAULT
    tls->free_page.cxl_page_queue_push(page, start);
    POTENTIAL_FAULT
    segment->used --;
    POTENTIAL_FAULT
    if(segment->used == 0)
    {
        POTENTIAL_FAULT
        cxl_segment_free(segment);
        POTENTIAL_FAULT
    }
    return;
}

cxl_page_t* cxl_shm::cxl_ptr_page(void* p)
{
    return cxl_segment_page_of(cxl_ptr_segment(p), p);
}

cxl_page_t* cxl_shm::get_page_by_offset(uint64_t page_offset)
{
    return (page_offset == 0 ? NULL : (cxl_page_t*) get_data_at_addr(start, page_offset));
}