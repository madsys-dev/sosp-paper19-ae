#include "cxlmalloc.h"
#include "cxlmalloc-internal.h"

cxl_page_t* cxl_shm::cxl_segment_page_alloc(uint64_t block_size)
{
    // find a free page
    POTENTIAL_FAULT
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    POTENTIAL_FAULT
    cxl_page_t* page = get_page_by_offset(tls->free_page.first);
    POTENTIAL_FAULT
    if(page == NULL)
    {
        // no free page, allocate a new segment and try again
        POTENTIAL_FAULT
        if(cxl_segment_alloc() == NULL)
        {
            POTENTIAL_FAULT//thread 2走了这个
            //std::cout<<"cxl_segment_page_alloc  cxl_segment_alloc() == NULL,  tls->free_page.first:" << tls->free_page.first <<std::endl;
            return NULL;
        }
        else
        {
            // otherwise try again,thread 1走了这个，然后调用了最后的return page
            POTENTIAL_FAULT
            //std::cout<<"cxl_segment_page_alloc  cxl_segment_alloc():segment != NULL, page:" << page  <<  ", tls->free_page.first:" << tls->free_page.first <<std::endl;
            return cxl_segment_page_alloc(block_size);
        }
    }
    POTENTIAL_FAULT
    tls->free_page.cxl_page_queue_remove(page, start);
    POTENTIAL_FAULT
    cxl_segment_t* segment = cxl_ptr_segment((void*) page);
    POTENTIAL_FAULT
    segment->used ++;
    POTENTIAL_FAULT
    //std::cout<<"cxl_segment_page_alloc  segment->used，return了page:" << segment->used  << ", tls->free_page.first:" << tls->free_page.first <<std::endl;
    return page;
}


// Allocate a segment from the shm 
cxl_segment_t* cxl_shm::cxl_segment_alloc()
{
    // find the addr of segment allocation state
    POTENTIAL_FAULT
    void* data;
    POTENTIAL_FAULT
    uint64_t offset = 0;
    POTENTIAL_FAULT
    uint32_t sas_no_use = 0;
    POTENTIAL_FAULT
    uint64_t count = 0;
    POTENTIAL_FAULT
    //std::cout << "cxl_segment_alloc  before loop data:" << data << "  ,((std::atomic<uint32_t> *) data)->load(std::memory_order_relaxed): " << ((std::atomic<uint32_t> *) data)->load(std::memory_order_relaxed) << std::endl;
    
    do {
        POTENTIAL_FAULT
        count++;

        if(SEGMENTS_AREA_START + count*SEGMENT_SIZE > size)
        {
            std::cout<<"cxl_segment_alloc break loop memory alloc fail for no more segment , data: "  << data << "  ,((std::atomic<uint32_t> *) data)->load(std::memory_order_relaxed): " << ((std::atomic<uint32_t> *) data)->load(std::memory_order_relaxed)<<std::endl;
            return NULL;
        }
        POTENTIAL_FAULT
        offset += sizeof(cxl_segment_allocation_state_t);
        POTENTIAL_FAULT
        sas_no_use = 0;
        POTENTIAL_FAULT
        data = get_data_at_addr(start, offset - sizeof(cxl_segment_allocation_state_t));
        POTENTIAL_FAULT
        // std::cout << "cxl_segment_alloc  COUNT:" << count << "  ,data: " << data
        //  << "  ,((std::atomic<uint32_t> *) data)->load(std::memory_order_relaxed): " 
        //  << ((std::atomic<uint32_t> *) data)->load(std::memory_order_relaxed) 
        //  << "  ,&saf_no_use: " << &sas_no_use << " ,sas_no_use:" << sas_no_use 
        //  <<"  ,thread_id: "  << thread_id 
        // //<< " ,std::memory_order_relaxed: "<< std::memory_order_relaxed
        //  //<<" ,std::memory_order_release: " << std::memory_order_release
        //   << " ,SEGMENTS_AREA_START + count*SEGMENT_SIZE: " << SEGMENTS_AREA_START + count*SEGMENT_SIZE << ", SEGMENTS_AREA_START:" << SEGMENTS_AREA_START << 
        //  ", SEGMENT_SIZE:" << SEGMENT_SIZE << " , size: " << size << std::endl;
         
    } while(!std::atomic_compare_exchange_weak_explicit((std::atomic<uint32_t> *) data, &sas_no_use, thread_id, std::memory_order_release, std::memory_order_relaxed));
    if(count > MAX_SEGMENT_NUM) {
        // Allocation failed
        std::atomic_store_explicit((std::atomic<uint32_t> *) data, sas_no_use, std::memory_order_release);
        return NULL;
    }
    // std::cout << "cxl_segment_alloc  after loop data:" << data << "  ,((std::atomic<uint32_t> *) data)->load(std::memory_order_relaxed): " << ((std::atomic<uint32_t> *) data)->load(std::memory_order_relaxed) << (std::atomic<uint32_t> *) data << std::endl;
        
    POTENTIAL_FAULT
    cxl_segment_allocation_state_t* sas = (cxl_segment_allocation_state_t*) data;
    POTENTIAL_FAULT
    sas->ver = NORMAL;
    POTENTIAL_FAULT
    sas->info = {0, 0};
    POTENTIAL_FAULT

    POTENTIAL_FAULT
    cxl_segment_t* segment =(cxl_segment_t*) get_data_at_addr(start, SEGMENTS_AREA_START + (count - 1)*SEGMENT_SIZE);
    POTENTIAL_FAULT
    segment->thread_id = thread_id;
    POTENTIAL_FAULT
    segment->used = 0;
    POTENTIAL_FAULT
   
    // init the free page queue
    POTENTIAL_FAULT
    cxl_page_t* page_start = &segment->meta_page[1];
    POTENTIAL_FAULT
    cxl_page_t* page = &segment->meta_page[PAGES_PER_SEGMENT - 1];
    POTENTIAL_FAULT
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    POTENTIAL_FAULT
    while (page >= page_start)
    {
        POTENTIAL_FAULT
        tls->free_page.cxl_page_queue_push(page, start);
        POTENTIAL_FAULT
        page = page - 1;
        POTENTIAL_FAULT
    }
    POTENTIAL_FAULT
    //std::cout << "cxl_segment_alloc  segment:" << segment << std::endl;
    return segment;
}

void cxl_shm::cxl_segment_free(cxl_segment_t* segment)
{
    POTENTIAL_FAULT
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    POTENTIAL_FAULT
    for(size_t i = 1; i < PAGES_PER_SEGMENT; i++)
    {
        POTENTIAL_FAULT
        tls->free_page.cxl_page_queue_remove(&segment->meta_page[i], start);
        POTENTIAL_FAULT
    }
    POTENTIAL_FAULT
    memset(segment, 0, sizeof(cxl_segment_t));
    POTENTIAL_FAULT
    uint64_t idx = (get_offset_for_data(start, (void*) segment) - SEGMENTS_AREA_START) / SEGMENT_SIZE;
    POTENTIAL_FAULT
    void* data = get_data_at_addr(start, idx * sizeof(cxl_segment_allocation_state_t));
    POTENTIAL_FAULT
    uint32_t sas_no_use = 0;
    POTENTIAL_FAULT
    uint32_t otid = thread_id;
    POTENTIAL_FAULT
    do {
        POTENTIAL_FAULT
        otid = thread_id;
        POTENTIAL_FAULT
    } while(!std::atomic_compare_exchange_weak_explicit((std::atomic<uint32_t> *) data, &otid, sas_no_use, std::memory_order_release, std::memory_order_relaxed));
}

cxl_segment_t* cxl_shm::cxl_ptr_segment(void* p)
{
    uint64_t offset = get_offset_for_data(start, p);
    uint64_t offset_aligned = offset & ~SEGMENT_MASK;
    return (cxl_segment_t*)get_data_at_addr(start, offset_aligned);
}