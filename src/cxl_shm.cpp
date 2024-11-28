#include "cxlmalloc-internal.h"
#include "cxlmalloc.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <x86intrin.h>
#include <fcntl.h>

void* cxl_shm::get_start()
{
    return start;
}

uint32_t cxl_shm::get_thread_id()
{
    return thread_id;
}

uint64_t cxl_shm::get_tls_offset() {
    return tls_offset;
};

cxl_shm::cxl_shm(uint64_t _size, int _shm_id)
{
    size = _size;
    shm_id = _shm_id;
    start = shmat(shm_id, NULL, 0);
    #ifdef FAULT_INJECTION
    mt_rand = std::mt19937(std::random_device{}());
    dis = std::uniform_int_distribution<int>(1, 1000);
    #endif
}

cxl_shm::cxl_shm(uint64_t _size, void *cxl_mem)
{
    size = _size;
    start = cxl_mem;
}

cxl_shm::~cxl_shm()
{
    shmdt(start);
}

void cxl_shm::thread_init()
{
    uint64_t machine_id = get_mac();
    uint16_t process_id = static_cast<uint16_t>(::syscall(SYS_gettid));
    uint64_t tls_id = pack_machine_process_id(machine_id, process_id);
    uint64_t tls_no_use = 0;
    
    // find the index of thread local state
    uint64_t offset = THREAD_LOCAL_VEC_START - sizeof(cxl_thread_local_state_t);
    cxl_thread_local_state_t* tls= nullptr;;
    do {
        //std::cout << "thread_init start  loop : " << std::endl;
        offset += sizeof(cxl_thread_local_state_t);
        tls_no_use = 0;
        //std::cout << "in loop: " <<  "offset " << offset << "sstart:" << start << std::endl;
        tls = (cxl_thread_local_state_t*)get_data_at_addr(start, offset);
        // Check if tls is a valid pointer
        if (tls == nullptr) {
            std::cerr << "Error: could not allocate thread local state" << std::endl;
            return;
        }
        if (offset >= size) {
            std::cerr << "Error: TLS offset out of range" << std::endl;
            // Handle the error appropriately
            return;
        }
        //std::cout << "thread_init end loop offset: " << offset << ", tls_no_use: " << tls_no_use << " ,tls: " << tls << " , tls->packed_machine_process_id: " << tls->packed_machine_process_id << " ,tls_id: " << tls_id << std::endl;
    } while (!tls->packed_machine_process_id.compare_exchange_weak(tls_no_use, tls_id));
    // std::cout << "thread_init end loop offset: " << offset << ", tls_no_use: " << tls_no_use << " ,tls: " << tls << " , tls->packed_machine_process_id: " << tls->packed_machine_process_id << " ,tls_id: " << tls_id << std::endl;
    

    this->tls_offset = offset;
    this->thread_id = (offset - THREAD_LOCAL_VEC_START) / sizeof(cxl_thread_local_state_t) + 1;
    tls->pages[0] = {0, 0, 16};
    tls->pages[1] = {0, 0, 16*((sizeof(cxl_message_queue_t)-1)>>4)+16};
    for(uint64_t i=2; i < CXL_BIN_SIZE; i++)
        tls->pages[i] = {0, 0, 16*(i-1)};
    tls->free_page = {0, 0, 0};
    tls->sender_queue = 0;
    tls->receiver_queue = 0;
    *era(thread_id, thread_id) += 1;
    return;
}


// first malloc thread base ref, set the in_use bit,and then find a free block for cxlobj,
// link thread base ref to the free block, at last malloc the cxlobj, and the cxl ref.
CXLRef cxl_shm::cxl_malloc(uint64_t data_size, uint32_t embedded_ref_cnt)
{
    POTENTIAL_FAULT
    RootRef* tbr = thread_base_ref_alloc();
    POTENTIAL_FAULT
    return cxl_ref_alloc(tbr, data_size + sizeof(CXLObj), embedded_ref_cnt);
}

CXLRef cxl_shm::cxl_malloc_wrc(uint64_t data_size, uint32_t embedded_ref_cnt)
{
    POTENTIAL_FAULT
    RootRef* tbr = thread_base_ref_alloc();
    //std::cout << "cxl_malloc_wrc: , tbr.get_tbr()->pptr:" << tbr->pptr << std::endl;
    POTENTIAL_FAULT
    return cxl_ref_alloc_wrc(tbr, data_size + sizeof(CXLObj), embedded_ref_cnt);
}

CXLRef cxl_shm::get_ref(uint64_t offset)
{
    RootRef* tbr = thread_base_ref_alloc();
    link_reference(tbr->pptr, offset);
    tbr->ref_cnt += 1;
    return CXLRef(this, get_offset_for_data(start, (void*) tbr), tbr->pptr + sizeof(CXLObj));
}

void cxl_shm::link_block_to_tbr(cxl_block* b, RootRef* tbr)
{
    POTENTIAL_FAULT
    tbr->pptr = get_offset_for_data(start, (void*) b);
    // std::cout << "link_block_to_tbr: , r1.get_tbr()->pptr:" << tbr->pptr << std::endl;
    POTENTIAL_FAULT
    tbr->ref_cnt += 1;
    POTENTIAL_FAULT
    return;
}

CXLObj* cxl_shm::block_to_cxlobj(cxl_block* b, cxl_page_t* page, uint64_t embedded_ref_cnt)
{
    // cxl_page_t* page = cxl_ptr_page((void*)b);
    POTENTIAL_FAULT
    page->used ++;
    POTENTIAL_FAULT
    page->free = b->next;
    POTENTIAL_FAULT
    CXLObj* cxl_obj = (CXLObj*) b;
    POTENTIAL_FAULT
    uint64_t info = pack_ref_info(thread_id, 1, *era(thread_id, thread_id));
    POTENTIAL_FAULT
    *era(thread_id, thread_id) += 1;
    POTENTIAL_FAULT
    cxl_obj->ref_info = info;
    POTENTIAL_FAULT
    cxl_obj->embedded_ref_cnt = embedded_ref_cnt;
    POTENTIAL_FAULT
    return cxl_obj;
}

CXLObj* cxl_shm::block_to_cxlobj_wrc(cxl_block* b, cxl_page_t* page, uint64_t embedded_ref_cnt)
{
    // cxl_page_t* page = cxl_ptr_page((void*)b);
    POTENTIAL_FAULT
    page->used ++;
    POTENTIAL_FAULT
    page->free = b->next;
    POTENTIAL_FAULT
    CXLObj* cxl_obj = (CXLObj*) b;
    POTENTIAL_FAULT
    uint64_t info = pack_ref_info(thread_id, 1, *era(thread_id, thread_id));
    POTENTIAL_FAULT
    *era(thread_id, thread_id) += 1;
    POTENTIAL_FAULT
    cxl_obj->ref_info = info;
    POTENTIAL_FAULT
    cxl_obj->embedded_ref_cnt = embedded_ref_cnt;
    POTENTIAL_FAULT
    //cxl_obj->writer_count = (uint64_t)0;
    POTENTIAL_FAULT
    //cxl_obj->reader_count = (uint64_t)0;
    POTENTIAL_FAULT
    return cxl_obj;
}


RootRef* cxl_shm::block_to_tbr(cxl_block* b, cxl_page_t* page)
{
    POTENTIAL_FAULT
    page->used ++;
    POTENTIAL_FAULT
    page->free = b->next;
    POTENTIAL_FAULT
    RootRef* tbr = (RootRef*) b;
    POTENTIAL_FAULT
    tbr->pptr = 0;
    POTENTIAL_FAULT
    tbr->in_use = 1;
    POTENTIAL_FAULT
    tbr->ref_cnt = 0;
    POTENTIAL_FAULT
    return tbr;
}


cxl_message_queue_t* cxl_shm::block_to_msg_queue(cxl_block* b, uint16_t sender_id, uint16_t receiver_id)
{
    cxl_page_t* page = cxl_ptr_page((void*) b);
    page->used ++;
    page->free = b->next;
    cxl_message_queue_t* q = (cxl_message_queue_t*) b;
    q->sender_id = sender_id;
    q->receiver_id = 0;
    q->start = 0;
    q->end = 0;
    q->sender_next = 0;
    q->receiver_next = 0;
    memset(q->buffer, 0, sizeof(q->buffer));
    return q;
}

void cxl_shm::link_reference(uint64_t& _ref, uint64_t _refed)
{
    // IncRefCnt
    POTENTIAL_FAULT
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    POTENTIAL_FAULT
    CXLObj* refed = (CXLObj*) get_data_at_addr(start, _refed);
    refed->str_content = "test";
    POTENTIAL_FAULT
    uint64_t ref_info;
    uint64_t new_ref_info;
    uint16_t ref_info_cnt;
    std::string str_content;
    do {
        POTENTIAL_FAULT
        ref_info = refed->ref_info;
        POTENTIAL_FAULT
        ref_info_cnt = get_ref_cnt_from_info(ref_info);

        str_content = refed->str_content;
        POTENTIAL_FAULT
        uint32_t saw_cid = get_lcid_from_info(ref_info);
        POTENTIAL_FAULT
        uint32_t saw_era = get_lenum_from_info(ref_info);
        POTENTIAL_FAULT
        std::atomic<uint32_t>* ele = era(thread_id, saw_cid);
        POTENTIAL_FAULT
        uint32_t cur_era = *era(thread_id, thread_id);
        POTENTIAL_FAULT
        if(saw_era > *ele) 
        {
            POTENTIAL_FAULT
            *ele = saw_era;
            POTENTIAL_FAULT
            FLUSH(ele);
        }
        // todo redo flush redo log
        POTENTIAL_FAULT
        char* redo = tls->redo + 64*(cur_era & (REDO_CACHELINE_CNT - 1));
        POTENTIAL_FAULT
        *((redo_log*) redo) = redo_log{LINK_REF, ref_info_cnt, cur_era, get_offset_for_data(start, &_ref), _refed, 0};
        POTENTIAL_FAULT
        new_ref_info = pack_ref_info(thread_id, ref_info_cnt+1, cur_era); 
        POTENTIAL_FAULT
        FLUSH(redo);
    } while(!refed->ref_info.compare_exchange_weak(ref_info, new_ref_info));

    //refed->str_content = str_content;
    //refed->str_content = str_content;    //放到上面更合适，和modCnt放在一起
    POTENTIAL_FAULT
    // std::cout<<"link_reference  _refed: " << _refed << std::endl;
    // std::cout<<"link_reference  _ref: " << _ref << std::endl;
    _ref = _refed;
    POTENTIAL_FAULT

    *era(thread_id, thread_id) += 1;

}

void cxl_shm::unlink_reference(uint64_t& _ref, uint64_t _refed)
{
    POTENTIAL_FAULT
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    POTENTIAL_FAULT
    CXLObj* refed = (CXLObj*) get_data_at_addr(start, _refed);
    POTENTIAL_FAULT
    uint64_t ref_info;
    uint64_t new_ref_info;
    uint16_t ref_info_cnt;
    do {
        POTENTIAL_FAULT
        ref_info = refed->ref_info;
        POTENTIAL_FAULT
        ref_info_cnt = get_ref_cnt_from_info(ref_info);
        POTENTIAL_FAULT
        uint32_t saw_cid = get_lcid_from_info(ref_info);
        POTENTIAL_FAULT
        uint32_t saw_era = get_lenum_from_info(ref_info);
        POTENTIAL_FAULT
        std::atomic<uint32_t>* ele = era(thread_id, saw_cid);
        POTENTIAL_FAULT
        if(saw_era > *ele) 
        {
            POTENTIAL_FAULT
            *ele = saw_era;
            POTENTIAL_FAULT
            FLUSH(ele);
        }
        POTENTIAL_FAULT
        uint32_t cur_era = *era(thread_id, thread_id);
        POTENTIAL_FAULT
        // tls->redo = redo_log{1, ref_info_cnt, cur_era, &_ref, _refed, 0};
        char* redo = tls->redo + 64*(cur_era & (REDO_CACHELINE_CNT - 1));
        POTENTIAL_FAULT
        *((redo_log*) redo) = redo_log{UNLINK_REF, ref_info_cnt, cur_era, get_offset_for_data(start, &_ref), _refed, 0};
        POTENTIAL_FAULT
        new_ref_info = pack_ref_info(thread_id,  ref_info_cnt-1, cur_era);
        POTENTIAL_FAULT
        FLUSH(redo);
        POTENTIAL_FAULT
    } while(!refed->ref_info.compare_exchange_weak(ref_info, new_ref_info));

    POTENTIAL_FAULT
    _ref = 0;
    POTENTIAL_FAULT
    *era(thread_id, thread_id) += 1;
    POTENTIAL_FAULT
    if(get_ref_cnt_from_info(ref_info)-1 > 0) return;
    POTENTIAL_FAULT
    cxl_free(false, (cxl_block*) refed);

}

// thread id count from 1
std::atomic<uint32_t>* cxl_shm::era(int x, int y)
{
    std::atomic<uint32_t>* era_start = (std::atomic<uint32_t>*) get_data_at_addr(start, ERA_ARRAY_START);
    return era_start + (x-1)*MAX_THREAD + (y-1);
}

void cxl_shm::test_free(uint64_t& _ref)
{   
    POTENTIAL_FAULT
    if(_ref == 0) return;
    POTENTIAL_FAULT

    CXLObj* refed = (CXLObj*) get_data_at_addr(start, _ref);
    POTENTIAL_FAULT
    uint64_t ref_info = refed->ref_info;
    POTENTIAL_FAULT
    if(get_ref_cnt_from_info(ref_info)-1 > 0)
    {
        POTENTIAL_FAULT
        unlink_reference(_ref, _ref);
        POTENTIAL_FAULT
        return;
    }
    POTENTIAL_FAULT
    for(size_t i=0; i<refed->embedded_ref_cnt; i++)
    {
        POTENTIAL_FAULT
        uint64_t* embedded_ref_offset = (uint64_t*) get_data_at_addr(refed, sizeof(CXLObj) + i*sizeof(uint64_t));
        POTENTIAL_FAULT
        test_free(*embedded_ref_offset);
        POTENTIAL_FAULT
    }
    POTENTIAL_FAULT
    unlink_reference(_ref, _ref);
}


// find size_class bin
cxl_page_queue_t* cxl_thread_local_state_s::cxl_page_queue(bool special, uint64_t size)
{
    if(special)
    {
        if(size == 16)
        {
            auto result = &pages[0];
            //std::cout << "   0cxl_page_queue size: "  << size <<" ,final size: " << ((size-1)>>4)+2 << " , result: "<<  result << std::endl;
            return result;
        }
        else if(size == sizeof(cxl_message_queue_t))
        {
            auto result = &pages[1];
            //std::cout << "    1cxl_page_queue size: "  << size <<" ,final size: " << ((size-1)>>4)+2 << " , result: "<<  result << std::endl;
            return result;
        }
    }
    auto result = &pages[((size-1)>>4)+2];
    //std::cout << "   2cxl_page_queue size: "  << size <<" ,final size: " << ((size-1)>>4)+2 << " , result: "<<  result << std::endl;
    return result;
}