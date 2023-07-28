#include "cxlmalloc-internal.h"
#include "cxlmalloc.h"
#include "recovery.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <x86intrin.h>
#include <chrono>

monitor::monitor(uint64_t _size, int _shm_id)
{
    size = _size;
    shm_id = _shm_id;
    start = shmat(shm_id, NULL, 0);
}

monitor::monitor(uint64_t _size, void* cxl_mem)
{
    size = _size;
    start = cxl_mem;
}

monitor::~monitor()
{
    shmdt(start);
}

std::atomic<uint32_t>* monitor::era(int x, int y)
{
    std::atomic<uint32_t>* era_start = (std::atomic<uint32_t>*) get_data_at_addr(start, ERA_ARRAY_START);
    return era_start + (x-1)*MAX_THREAD + (y-1);
}

cxl_page_t* monitor::cxl_ptr_page(void* p)
{
    return cxl_segment_page_of(cxl_ptr_segment(p), p);
}

cxl_segment_t* monitor::cxl_ptr_segment(void* p)
{
    uint64_t offset = get_offset_for_data(start, p);
    uint64_t offset_aligned = offset & ~SEGMENT_MASK;
    return (cxl_segment_t*)get_data_at_addr(start, offset_aligned);
}


void cxl_shm::normal_garbage_collection()
{
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    
    // free every tbr page
    cxl_page_queue_t* tbr_pq = tls->cxl_page_queue(true, 16);
    cxl_page_t* tbr_page = get_page_by_offset(tbr_pq->first);
    while (tbr_page != NULL)
    {
        cxl_page_t* next = get_page_by_offset(tbr_page->next);
        void* page_area = cxl_page_start(cxl_ptr_segment(tbr_page), tbr_page);
        cxl_block* start_block = cxl_page_block_at(page_area, tbr_page->block_size, 0);
        cxl_block* last_block = cxl_page_block_at(page_area, tbr_page->block_size, PAGE_SIZE / tbr_page->block_size - 1);
        cxl_block* block = start_block;
        while (block <= last_block)
        {
            cxl_block* next = (cxl_block*)((char*)block + tbr_page->block_size);
            RootRef* tbr = (RootRef*) block;
            if(tbr->in_use == 1)
            {
                test_free(tbr->pptr);
                tbr->in_use = 0;
                bool flag = (tbr_page->used == 1);
                cxl_free(true, (cxl_block*) tbr);
                if(flag) break;
            }
            block = next;
        }
        tbr_page = next;
    }

    // free every msg queue page
    cxl_message_queue_t* sender_msg_queue = tls->sender_queue == 0? NULL : (cxl_message_queue_t*) get_data_at_addr(start, tls->sender_queue);
    while(sender_msg_queue != NULL)
    {
        cxl_message_queue_t* next = sender_msg_queue->sender_next == 0? NULL : (cxl_message_queue_t*) get_data_at_addr(start, sender_msg_queue->sender_next);
        size_t i = sender_msg_queue->end;
        do {
            if(sender_msg_queue->buffer[i] != 0)
            {
                test_free(sender_msg_queue->buffer[i]);
            }
            i = (i + 1) % MESSAGE_BUFFER_SIZE;
        } while (i != sender_msg_queue->start);

        if(sender_msg_queue->receiver_id == 0)
        {
            while (i != sender_msg_queue->end)
            {
                if(sender_msg_queue->buffer[i] != 0)
                {
                    test_free(sender_msg_queue->buffer[i]);
                }
                i = (i + 1) % MESSAGE_BUFFER_SIZE;
            }
        }
        sender_msg_queue->sender_id = 0;

        cxl_segment_t* segment = cxl_ptr_segment(sender_msg_queue);
        size_t idx = (get_offset_for_data(start, segment) - SEGMENTS_AREA_START) / SEGMENT_SIZE;
        cxl_segment_allocation_state_t* sas = (cxl_segment_allocation_state_t*)get_data_at_addr(start, idx * sizeof(cxl_segment_allocation_state_t));
        state_free_info info = sas->info.load();
        state_free_info new_info;
        do {
            info = sas->info.load();
            new_info = {ABANDON, info.thread_free};
        } while(!sas->info.compare_exchange_weak(info, new_info));

        sender_msg_queue = next;
    }

    cxl_message_queue_t* receiver_msg_queue = tls->receiver_queue == 0? NULL : (cxl_message_queue_t*) get_data_at_addr(start, tls->receiver_queue);
    while(receiver_msg_queue != NULL)
    {
        cxl_message_queue_t* next = receiver_msg_queue->receiver_next == 0? NULL : (cxl_message_queue_t*) get_data_at_addr(start, receiver_msg_queue->receiver_next);
        size_t i = receiver_msg_queue->start;
        while (i != receiver_msg_queue->end)
        {
            if(receiver_msg_queue->buffer[i] != 0)
            {
                test_free(receiver_msg_queue->buffer[i]);
            }
            i = (i + 1) % MESSAGE_BUFFER_SIZE;
        }
        receiver_msg_queue->receiver_id = 0;
        receiver_msg_queue = next;
    }

    // free kv area
    // todo, wait to slove ABA problem

    // add abandon state
    for(size_t i=2; i<CXL_BIN_SIZE; i++)
    {
        cxl_page_queue_t pq = tls->pages[i];
        cxl_page_t* page = get_page_by_offset(pq.first);
        while (page != NULL)
        {
            cxl_page_t* next = get_page_by_offset(page->next);
            cxl_thread_free_collect(page);
            if(page->used == 0) 
            {
                cxl_page_free(page, tls->cxl_page_queue(false, page->block_size));
                page = next;
                continue;
            }
            cxl_segment_t* segment = cxl_ptr_segment(page);
            size_t idx = (get_offset_for_data(start, segment) - SEGMENTS_AREA_START) / SEGMENT_SIZE;
            cxl_segment_allocation_state_t* sas = (cxl_segment_allocation_state_t*)get_data_at_addr(start, idx * sizeof(cxl_segment_allocation_state_t));
            state_free_info info = sas->info.load();
            state_free_info new_info;
            do {
                info = sas->info.load();
                new_info = {ABANDON, info.thread_free};
            } while(!sas->info.compare_exchange_weak(info, new_info));
            page = next;
        }
    }

    // recycle thread local state
    tls->pages[0] = {0, 0, 16};
    tls->pages[1] = {0, 0, 16*((sizeof(cxl_message_queue_t)-1)>>4)+16};
    for(uint64_t i=2; i < CXL_BIN_SIZE; i++)
        tls->pages[i] = {0, 0, 16*(i-1)};
    tls->free_page = {0, 0, 0};
    tls->sender_queue = 0;
    tls->receiver_queue = 0;
    
    uint64_t tls_no_use = 0;
    uint64_t id;
    do {
        tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
        id = tls->packed_machine_process_id;
        tls_no_use = 0;
    } while(!tls->packed_machine_process_id.compare_exchange_weak(id, tls_no_use));

}

void monitor::redo_ref_cnt(uint64_t offset)
{
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, offset);
    uint32_t tls_id = (offset - THREAD_LOCAL_VEC_START) / sizeof(cxl_thread_local_state_t) + 1;
    size_t redo_idx = 0;
    redo_log* redo = (redo_log*) tls->redo;
    for(size_t i=1; i<REDO_CACHELINE_CNT; i++)
    {
        redo_log* redo_t = (redo_log*)(tls->redo + 64*i);
        if(redo_t->cur_era > redo->cur_era)
            redo = redo_t;
    }
    if(redo->func_id == LINK_REF)
    {
        CXLObj* lo = (CXLObj*) get_data_at_addr(start, redo->refed);
        uint32_t lo_cid = get_lcid_from_info(lo->ref_info);
        uint32_t lo_era = get_lenum_from_info(lo->ref_info);
        bool need_redo = false;
        if(lo_cid == tls_id && lo_era == redo->cur_era)
        {
            need_redo = true;
        }
        else
        {
            uint32_t max_era = 0;
            for(size_t j=0; j<MAX_THREAD; j++)
            {
                if(j == tls_id) continue;
                if(*era(j, tls_id) > max_era)
                    max_era = *era(j, tls_id);

            }
            if(redo->cur_era <= max_era)
                need_redo = true;
        }
        if(need_redo)
        {
            *((uint64_t*)get_data_at_addr(start, redo->ref)) = redo->refed;
        }
    }
    else if(redo->func_id == UNLINK_REF)
    {
        CXLObj* lo = (CXLObj*) get_data_at_addr(start, redo->refed);
        uint32_t lo_cid = get_lcid_from_info(lo->ref_info);
        uint32_t lo_era = get_lenum_from_info(lo->ref_info);
        bool need_redo = false;
        if(lo_cid == tls_id && lo_era == redo->cur_era)
        {
            need_redo = true;
        }
        else
        {
            uint32_t max_era = 0;
            for(size_t j=0; j<MAX_THREAD; j++)
            {
                if(j == tls_id) continue;
                if(*era(j, tls_id) > max_era)
                    max_era = *era(j, tls_id);

            }
            if(redo->cur_era <= max_era)
                need_redo = true;
        }
        if(need_redo)
        {
            *((uint64_t*)get_data_at_addr(start, redo->ref)) = 0;
        }
        if(get_ref_cnt_from_info(redo->saved_ref_cnt) == 1)
        {
            cxl_segment_t* segment = cxl_ptr_segment(lo);
            size_t idx = (get_offset_for_data(start, segment) - SEGMENTS_AREA_START) / SEGMENT_SIZE;
            cxl_segment_allocation_state_t* sas = (cxl_segment_allocation_state_t*)get_data_at_addr(start, idx * sizeof(cxl_segment_allocation_state_t));
            state_free_info info = sas->info.load();
            state_free_info new_info;
            do {
                info = sas->info.load();
                new_info = {POTENTIAL_LEAKING, info.thread_free};
            } while(!sas->info.compare_exchange_weak(info, new_info));
        }

    }
}

void monitor::recovery_garbage_collection(uint64_t offset, uint64_t packed_machine_process_id)
{
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, offset);
    uint32_t tls_id = (offset - THREAD_LOCAL_VEC_START) / sizeof(cxl_thread_local_state_t) + 1;

    // free every tbr page
    cxl_page_queue_t* tbr_pq = tls->cxl_page_queue(true, 16);
    cxl_page_t* tbr_page = get_page_by_offset(tbr_pq->first);
    while (tbr_page != NULL)
    {
        cxl_page_t* next = get_page_by_offset(tbr_page->next);
        void* page_area = cxl_page_start(cxl_ptr_segment(tbr_page), tbr_page);
        cxl_block* start_block = cxl_page_block_at(page_area, tbr_page->block_size, 0);
        cxl_block* last_block = cxl_page_block_at(page_area, tbr_page->block_size, (PAGE_SIZE / tbr_page->block_size) - 1);
        cxl_block* block = start_block;
        while (block <= last_block)
        {
            cxl_block* next = (cxl_block*)((char*)block + tbr_page->block_size);
            RootRef* tbr = (RootRef*) block;
            if(tbr->in_use == 1)
            {
                if(tbr->pptr !=0)
                {
                    cxl_block* refed = (cxl_block*) get_data_at_addr(start, tbr->pptr);
                    if(tbr->pptr != cxl_ptr_page(refed)->free && !(refed >= start_block && refed <= last_block))
                    {
                        if(((CXLObj*) refed)->embedded_ref_cnt != 0) std::cout<<"error"<<std::endl;
                        recovery_test_free(tbr->pptr, offset);
                    }
                }
                tbr->in_use = 0;
                recovery_cxl_free(true, (cxl_block*) tbr, offset);
            }
            block = next;
        }
        recovery_cxl_page_free(tbr_page, tbr_pq, offset);
        tbr_page = next;
    }

    // free every msg queue page
    cxl_message_queue_t* sender_msg_queue = tls->sender_queue == 0? NULL : (cxl_message_queue_t*) get_data_at_addr(start, tls->sender_queue);
    while(sender_msg_queue != NULL)
    {
        cxl_message_queue_t* next = sender_msg_queue->sender_next == 0? NULL : (cxl_message_queue_t*) get_data_at_addr(start, sender_msg_queue->sender_next);
        size_t i = sender_msg_queue->end;
        do {
            if(sender_msg_queue->buffer[i] != 0)
            {
                recovery_test_free(sender_msg_queue->buffer[i], offset);
            }
            i = (i + 1) % MESSAGE_BUFFER_SIZE;
        } while (i != sender_msg_queue->start);

        if(sender_msg_queue->receiver_id == 0)
        {
            while (i != sender_msg_queue->end)
            {
                if(sender_msg_queue->buffer[i] != 0)
                {
                    recovery_test_free(sender_msg_queue->buffer[i], offset);
                }
                i = (i + 1) % MESSAGE_BUFFER_SIZE;
            }
        }
        sender_msg_queue->sender_id = 0;

        cxl_segment_t* segment = cxl_ptr_segment(sender_msg_queue);
        size_t idx = (get_offset_for_data(start, segment) - SEGMENTS_AREA_START) / SEGMENT_SIZE;
        cxl_segment_allocation_state_t* sas = (cxl_segment_allocation_state_t*)get_data_at_addr(start, idx * sizeof(cxl_segment_allocation_state_t));
        state_free_info info = sas->info.load();
        state_free_info new_info;
        do {
            info = sas->info.load();
            new_info = {ABANDON, info.thread_free};
        } while(!sas->info.compare_exchange_weak(info, new_info));

        sender_msg_queue = next;
    }


    cxl_message_queue_t* receiver_msg_queue = tls->receiver_queue == 0? NULL : (cxl_message_queue_t*) get_data_at_addr(start, tls->receiver_queue);
    while(receiver_msg_queue != NULL)
    {
        cxl_message_queue_t* next = receiver_msg_queue->receiver_next == 0? NULL : (cxl_message_queue_t*) get_data_at_addr(start, receiver_msg_queue->receiver_next);
        size_t i = receiver_msg_queue->start;
        while (i != receiver_msg_queue->end)
        {
            if(receiver_msg_queue->buffer[i] != 0)
            {
                recovery_test_free(receiver_msg_queue->buffer[i], offset);
            }
            i = (i + 1) % MESSAGE_BUFFER_SIZE;
        }
        receiver_msg_queue->receiver_id = 0;
        receiver_msg_queue = next;
    }


    // free every obj page
    for(size_t i=2; i<CXL_BIN_SIZE; i++)
    {
        cxl_page_queue_t* pq = tls->pages+i;
        cxl_page_t* page = pq->first == 0 ? NULL : (cxl_page_t*) get_data_at_addr(start, pq->first);
        while(page != NULL)
        {
            cxl_page_t* next = get_page_by_offset(page->next);
            CXLObj* page_free = (CXLObj*) get_block_by_offset(page->free);
            if(page_free != NULL) page_free->ref_info = 0;

            size_t live_block_cnt = 0;
            void* page_area = cxl_page_start(cxl_ptr_segment(page), page);
            cxl_block* start_block = cxl_page_block_at(page_area, page->block_size, 0);
            cxl_block* last_block = cxl_page_block_at(page_area, page->block_size, PAGE_SIZE / page->block_size - 1);
            cxl_block* block = start_block;
            while(block <= last_block)
            {
                cxl_block* next = (cxl_block*)((char*)block + page->block_size);
                CXLObj* o = (CXLObj*) block;
                if(get_ref_cnt_from_info(o->ref_info) > 0)
                {
                    live_block_cnt ++;
                    break;
                }
                block = next;
            }

            if(live_block_cnt == 0)
            {
                recovery_cxl_page_free(page, pq, offset);
            }
            else
            {
                cxl_segment_t* segment = cxl_ptr_segment(page);
                size_t idx = (get_offset_for_data(start, segment) - SEGMENTS_AREA_START) / SEGMENT_SIZE;
                cxl_segment_allocation_state_t* sas = (cxl_segment_allocation_state_t*)get_data_at_addr(start, idx * sizeof(cxl_segment_allocation_state_t));
                state_free_info info = sas->info.load();
                state_free_info new_info;
                do {
                    info = sas->info.load();
                    new_info = {ABANDON, info.thread_free};
                } while(!sas->info.compare_exchange_weak(info, new_info));
            }

            page = next;
        }
    }


    // free all not abandon segments
    for(size_t i=0; i<MAX_SEGMENT_NUM; i++)
    {
        cxl_segment_allocation_state_t* sas = (cxl_segment_allocation_state_t*)get_data_at_addr(start, i * sizeof(cxl_segment_allocation_state_t));
        cxl_segment_t* segment = (cxl_segment_t*) get_data_at_addr(start, SEGMENTS_AREA_START + i*SEGMENT_SIZE);
        if(sas->thread_id.load() == tls_id && sas->info.load().state != ABANDON)
        {
            recovery_cxl_segment_free(segment, offset);
        }
    }

    // free tls
    tls->pages[0] = {0, 0, 16};
    tls->pages[1] = {0, 0, 16*((sizeof(cxl_message_queue_t)-1)>>4)+16};
    for(uint64_t i=2; i < CXL_BIN_SIZE; i++)
        tls->pages[i] = {0, 0, 16*(i-1)};
    tls->free_page = {0, 0, 0};
    tls->sender_queue = 0;
    tls->receiver_queue = 0;

    uint64_t tls_no_use = 0;
    uint64_t pack_id;
    do {
        tls = (cxl_thread_local_state_t*) get_data_at_addr(start, offset);
        pack_id = packed_machine_process_id;
        tls_no_use = 0;
    } while(!tls->packed_machine_process_id.compare_exchange_weak(pack_id, tls_no_use));


}

void monitor::recovery_test_free(uint64_t& _ref, uint64_t offset)
{
    if(_ref == 0) return;

    CXLObj* refed = (CXLObj*) get_data_at_addr(start, _ref);
    uint64_t ref_info = refed->ref_info;
    uint64_t ref_cnt = get_ref_cnt_from_info(ref_info);
    if(ref_cnt == 0) return;
    if(ref_cnt-1 > 0)
    {
        recovery_unlink_reference(_ref, _ref, offset);
        return;
    }
    for(size_t i=0; i<refed->embedded_ref_cnt; i++)
    {
        uint64_t* embedded_ref_offset = (uint64_t*) get_data_at_addr(refed, sizeof(CXLObj) + i*sizeof(uint64_t));
        recovery_test_free(*embedded_ref_offset, offset);
    }
    recovery_unlink_reference(_ref, _ref, offset);
}

void monitor::recovery_unlink_reference(uint64_t& _ref, uint64_t _refed, uint64_t offset)
{
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, offset);
    uint32_t tls_id = (offset - THREAD_LOCAL_VEC_START) / sizeof(cxl_thread_local_state_t) + 1;
    CXLObj* refed = (CXLObj*) get_data_at_addr(start, _refed);
    
    uint64_t ref_info;
    uint64_t new_ref_info;
    uint16_t ref_info_cnt;
    do {
        ref_info = refed->ref_info;
        ref_info_cnt = get_ref_cnt_from_info(ref_info);
        uint32_t saw_cid = get_lcid_from_info(ref_info);
        uint32_t saw_era = get_lenum_from_info(ref_info);
        std::atomic<uint32_t>* ele = era(tls_id, saw_cid);
        uint32_t cur_era = *era(tls_id, tls_id);
        if(saw_era > *ele) 
        {
            *ele = saw_era;
            FLUSH(ele);
        }
        new_ref_info = pack_ref_info(tls_id,  ref_info_cnt-1, cur_era);
    } while(!refed->ref_info.compare_exchange_weak(ref_info, new_ref_info));

    _ref = 0;
    *era(tls_id, tls_id) += 1;

    if(get_ref_cnt_from_info(ref_info)-1 > 0) return;
    recovery_cxl_free(false, (cxl_block*) refed, offset);
}

void monitor::recovery_cxl_free(bool special, cxl_block* b, uint64_t offset)
{
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, offset);
    uint32_t tls_id = (offset - THREAD_LOCAL_VEC_START) / sizeof(cxl_thread_local_state_t) + 1;
    cxl_page_t* page = cxl_ptr_page((void*) b);
    cxl_segment_t* segment = cxl_ptr_segment((void*) b);

    if(!special) memset(b, 0, sizeof(CXLObj));
    else
    {
        if(page->block_size == 16) memset(b, 0, sizeof(RootRef));
        else memset(b, 0, sizeof(cxl_message_queue_t));
    }

    if(segment->thread_id != tls_id)
    {
        uint64_t i = (get_offset_for_data(start, segment) - SEGMENTS_AREA_START) / SEGMENT_SIZE;
        cxl_segment_allocation_state_t* sas = (cxl_segment_allocation_state_t*)get_data_at_addr(start, i*sizeof(cxl_segment_allocation_state_t));
        state_free_info info = sas->info.load();
        state_free_info new_info;
        uint64_t b_offset = get_offset_for_data(start, b);
        do {
            info = sas->info.load();
            b->next = info.thread_free;
            new_info = {info.state, b_offset};
        } while (!sas->info.compare_exchange_weak(info, new_info));
    }
}

void monitor::recovery_cxl_page_free(cxl_page_t* page, cxl_page_queue_t* pq, uint64_t offset)
{
    memset(page, 0, sizeof(cxl_page_t));
}

void monitor::recovery_cxl_segment_free(cxl_segment_t* segment, uint64_t offset)
{
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, offset);
    uint32_t tls_id = (offset - THREAD_LOCAL_VEC_START) / sizeof(cxl_thread_local_state_t) + 1;
    memset(segment, 0, sizeof(cxl_segment_t));
    uint64_t idx = (get_offset_for_data(start, (void*) segment) - SEGMENTS_AREA_START) / SEGMENT_SIZE;
    void* data = get_data_at_addr(start, idx * sizeof(cxl_segment_allocation_state_t));
    uint32_t sas_no_use = 0;
    uint32_t otid = tls_id;
    do {
        otid = tls_id;
    } while(!std::atomic_compare_exchange_weak_explicit((std::atomic<uint32_t> *) data, &otid, sas_no_use, std::memory_order_release, std::memory_order_relaxed));
}

void monitor::recovery_loop()
{
    while(true)
    {
        single_recovery_loop();
    }
}

void monitor::single_recovery_loop()
{
    // check tls
    for(size_t i=0; i<MAX_THREAD; i++)
    {
        uint64_t offset = THREAD_LOCAL_VEC_START + i*sizeof(cxl_thread_local_state_t);
        cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, offset);
        uint64_t id = tls->packed_machine_process_id.load();
        if(check_tls(id))
        {
            std::cout<<"thread "<<i+1<<" exit"<<std::endl;
            redo_ref_cnt(offset);
            recovery_garbage_collection(offset, id);
        }
    }

    uint32_t counter = 0;
    double sum = 0;
    // check segment
    for(size_t i=0; i<MAX_SEGMENT_NUM; i++)
    {
        cxl_segment_allocation_state_t* sas = (cxl_segment_allocation_state_t*)get_data_at_addr(start, i * sizeof(cxl_segment_allocation_state_t));
        if(sas->thread_id.load() == 0) continue;
        // if(sas->info.load().state == ABANDON || sas->info.load().state == POTENTIAL_LEAKING)
        if(true)
        {
            auto begin_time = std::chrono::system_clock::now();  
            cxl_segment_t* segment = (cxl_segment_t*) get_data_at_addr(start, SEGMENTS_AREA_START + i*SEGMENT_SIZE);
            size_t live_page_cnt = 0;
            for(size_t j=0; j<PAGES_PER_SEGMENT; j++)
            {
                cxl_page_t* page = segment->meta_page + j;
                if(page->block_size == 0) continue;
                void* page_area = cxl_page_start(segment, page);
                cxl_block* start_block = cxl_page_block_at(page_area, page->block_size, 0);
                cxl_block* last_block = cxl_page_block_at(page_area, page->block_size, PAGE_SIZE / page->block_size - 1);
                cxl_block* block = start_block;
                size_t live_block_cnt = 0;
                while(block <= last_block)
                {
                    cxl_block* next = (cxl_block*)((char*)block + page->block_size);
                    if(page->is_msg_queue_page == false)
                    {
                        CXLObj* o = (CXLObj*) block;
                        if(get_ref_cnt_from_info(o->ref_info) > 0)
                        {
                            live_block_cnt ++;
                            break;
                        }
                    }
                    else
                    {
                        cxl_message_queue_t* q = (cxl_message_queue_t*) block;
                        if(q->receiver_id != 0 || q->sender_id != 0)
                        {
                            live_block_cnt ++;
                            break;
                        }
                        else
                        {
                            memset(q, 0, sizeof(cxl_message_queue_t));
                        }
                    }
                    block = next;
                }
                if(live_block_cnt == 0)
                {
                    memset(page, 0, sizeof(cxl_page_t));
                }
                else
                {
                    live_page_cnt ++;
                    break;
                }
            }
            if(live_page_cnt == 0)
            {
                memset(segment, 0, sizeof(cxl_segment_t));
                state_free_info info = sas->info.load();
                state_free_info new_info;
                do {
                    info = sas->info.load();
                    new_info = {NORMAL, 0};
                } while(!sas->info.compare_exchange_weak(info, new_info));

                uint32_t otid;
                do {
                    otid = sas->thread_id.load();
                } while(!sas->thread_id.compare_exchange_weak(otid, 0));
            }
            auto end_time = std::chrono::system_clock::now();
            std::chrono::duration<double> diff = end_time - begin_time;
            sum += diff.count();
            counter ++;
        }
    }
    printf("SUM %lf, Times %d, per %lf\n", sum, counter, sum / counter);
}

bool monitor::check_tls(uint64_t id)
{
    // simple implement for recovery test
    // todo for user with specific machine、process and thread
    if(id == 0) return false;
    return true;
}

bool monitor::check_recovery()
{
    for(size_t i=0; i<MAX_THREAD; i++)
    {
        uint64_t offset = THREAD_LOCAL_VEC_START + i*sizeof(cxl_thread_local_state_t);
        cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, offset);
        uint64_t id = tls->packed_machine_process_id.load();
        if(id != 0 )
        {
            std::cout<<"error tls id = "<<id<<std::endl;
            return false;
        }
    }

    for(size_t i=0; i<MAX_SEGMENT_NUM; i++)
    {
        cxl_segment_allocation_state_t* sas = (cxl_segment_allocation_state_t*)get_data_at_addr(start, i * sizeof(cxl_segment_allocation_state_t));
        if(sas->thread_id.load() != 0)
        {
            std::cout<<"error segment occupied by thread id = "<<sas->thread_id.load()<<" index = "<<get_offset_for_data(start, sas) / sizeof(cxl_segment_allocation_state_t)<<std::endl;
            return false;
        }
    }

    std::cout<<"free all successfully!"<<std::endl;
    return true;
}

cxl_page_t* monitor::get_page_by_offset(uint64_t page_offset)
{
    return (page_offset == 0 ? NULL : (cxl_page_t*) get_data_at_addr(start, page_offset));
}

cxl_block* monitor::get_block_by_offset(uint64_t block_offset)
{
    return (block_offset == 0 ? NULL : (cxl_block*) get_data_at_addr(start, block_offset));
}
