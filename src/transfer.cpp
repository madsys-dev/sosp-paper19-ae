#include "cxlmalloc.h"
#include "cxlmalloc-internal.h"
#include "cxlmalloc-types.h"
#include <chrono>
#include <mutex>

#include <thread>

std::mutex g_pages_mutex1;
uint64_t cxl_shm::cxl_wrap(CXLRef& ref)
{
    return ref.get_tbr()->pptr;
}

bool cxl_shm::sent_to(uint64_t queue_offset, CXLRef& ref)
{
    // reset_flag 初始话
    POTENTIAL_FAULT
    uint64_t offset = ref.get_tbr()->pptr;
    POTENTIAL_FAULT
    cxl_message_queue_t* q = (cxl_message_queue_t*) get_data_at_addr(start, queue_offset);
    POTENTIAL_FAULT
    if(q->end + 1 == q->start || q->end - q->start == MESSAGE_BUFFER_SIZE - 1) 
    {
        //std::cout  << " sent_to return false q->start: " << q->start << " ,q->end： " << q->end << " ,q->end - q->start: " << q->end - q->start << std::endl;
        return false;
    }
    // S1
    POTENTIAL_FAULT
    // std::cout  << "before  link q->start" << q->start << ",q->end " << q->end << ",offset" <<offset<<",q->buffer[q->end]" <<q->buffer[q->end] << std::endl;
    link_reference(q->buffer[q->end], offset);
    
    POTENTIAL_FAULT
    // S2
    q->end = (q->end + 1) % MESSAGE_BUFFER_SIZE;
    //std::cout  << " sent_to after link q->start: " << q->start << " ,q->end： " << q->end << " ,ref.get_tbr()->pptr: " << offset <<" ,q->buffer[q->end]: " <<q->buffer[q->end] << " ,q->buffer[q->end-1]: " <<q->buffer[q->end-1] << " ,queue_offset: " << queue_offset << std::endl;
    POTENTIAL_FAULT
    return true;
}



CXLRef cxl_shm::cxl_unwrap(uint64_t offset)
{
    POTENTIAL_FAULT
    cxl_message_queue_t* q = (cxl_message_queue_t*) get_data_at_addr(start, offset);
    POTENTIAL_FAULT
    // update receiver queue
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    if(q->receiver_id == 0)
    {
        POTENTIAL_FAULT
        q->receiver_next = tls->receiver_queue;
        POTENTIAL_FAULT
        tls->receiver_queue = offset;
        POTENTIAL_FAULT
        q->receiver_id = thread_id;
    }
    POTENTIAL_FAULT
    std::cout  << " cxl_unwrap q->start: " << q->start << " ,q->end： " << q->end << " ,offset： " <<offset<<" ,q->buffer[q->end]: " <<q->buffer[q->end] << std::endl;
    
    if(q->start == q->end || q->buffer[q->start] == 0) return CXLRef(this, 0, 0);   
    POTENTIAL_FAULT
    // R1
    RootRef* tbr = thread_base_ref_alloc();
    POTENTIAL_FAULT
    link_reference(tbr->pptr, q->buffer[q->start]);
    POTENTIAL_FAULT
    tbr->ref_cnt += 1;
    POTENTIAL_FAULT
    // R2
    unlink_reference(q->buffer[q->start], q->buffer[q->start]);
    POTENTIAL_FAULT
    // R3
    q->start = (q->start + 1) % MESSAGE_BUFFER_SIZE;
    POTENTIAL_FAULT
    return CXLRef(this, get_offset_for_data(start, (void*) tbr), tbr->pptr + sizeof(CXLObj));

}


CXLRef cxl_shm::cxl_unwrap_mend(uint64_t offset)
{
    POTENTIAL_FAULT
    cxl_message_queue_t* q = (cxl_message_queue_t*) get_data_at_addr(start, offset);
    POTENTIAL_FAULT
    // update receiver queue
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    if(q->receiver_id == 0)
    {
        POTENTIAL_FAULT
        q->receiver_next = tls->receiver_queue;
        POTENTIAL_FAULT
        tls->receiver_queue = offset;
        POTENTIAL_FAULT
        q->receiver_id = thread_id;
    }
    POTENTIAL_FAULT
    // std::cout  << " \n cxl_unwrap_mend before loop q->start: " << q->start << " ,q->end： " << q->end << " ,offset： " <<offset<< " ,q->buffer[q->start]:" << q->buffer[q->start] <<" ,q->buffer[q->end]: " <<q->buffer[q->end] << std::endl;
    
    //std::cout << "before cxl_unwrap_mend loop  q->start: " << q->start << " ,q->end： " << q->end << " ,offset： " <<offset<< " ,q->buffer[q->start]:" << q->buffer[q->start] <<" ,q->buffer[q->end]: " <<q->buffer[q->end] << std::endl;
    while( q->buffer[q->start] == 0) {  
        //return CXLRef(this, 0, 0);   
        
        //std::this_thread::sleep_for(std::chrono::milliseconds(100));
        //std::cout  << "cxl_unwrap_mend loop q->start: " << q->start << " ,q->end： " << q->end << " ,offset： " <<offset<< " ,q->buffer[q->start]:" << q->buffer[q->start] <<" ,q->buffer[q->end]: " <<q->buffer[q->end]  <<" ,q->buffer[q->start]: " <<  q->buffer[q->start]<< std::endl;
    };
    //std::cout << "after cxl_unwrap_mend loop   q->start: " << q->start << " ,q->end： " << q->end << " ,offset： " <<offset<< " ,q->buffer[q->start]:" << q->buffer[q->start] <<" ,q->buffer[q->end]: " <<q->buffer[q->end] << std::endl;
    // std::cout  << " \n cxl_unwrap_mend after loop q->start: " << q->start << " ,q->end： " << q->end << " ,offset： " <<offset<< " ,q->buffer[q->start]:" << q->buffer[q->start] <<" ,q->buffer[q->end]: " <<q->buffer[q->end] << std::endl;
    POTENTIAL_FAULT
    // R1
    RootRef* tbr = thread_base_ref_alloc();
    POTENTIAL_FAULT
    
    // std::cout  << " \n cxl_unwrap_mend before link_reference:  q->buffer[q->start]: " << q->buffer[q->start] << std::endl;
    // std::cout  << " \n cxl_unwrap_mend before link_reference:  tbr " << tbr << std::endl;
    // std::cout  << " \n cxl_unwrap_mend before link_reference:  tbr->pptr: " << tbr->pptr << std::endl;
    link_reference(tbr->pptr, q->buffer[q->start]);
    POTENTIAL_FAULT
    tbr->ref_cnt += 1;
    POTENTIAL_FAULT
    // R2
    unlink_reference(q->buffer[q->start], q->buffer[q->start]);
    POTENTIAL_FAULT
    // R3
    q->start = (q->start + 1) % MESSAGE_BUFFER_SIZE;
    //std::cout << "after: q->start  reset: " << q->start   << std::endl;
    
    POTENTIAL_FAULT
    // std::cout  << "cxl_unwrap_mend return CXLRef(this, get_offset_for_data(start, (void*) tbr), tbr->pptr + sizeof(CXLObj));  "  << std::endl;   
    return CXLRef(this, get_offset_for_data(start, (void*) tbr), tbr->pptr + sizeof(CXLObj));

}


CXLRef cxl_shm::cxl_unwrap_mend(uint64_t offset, cxl_message_queue_t* q, RootRef* tbr)
{
    //std::lock_guard<std::mutex> guard(g_pages_mutex1);
    POTENTIAL_FAULT
    // cxl_message_queue_t* q = (cxl_message_queue_t*) get_data_at_addr(start, offset);
    // POTENTIAL_FAULT
    // // update receiver queue
    // cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    // if(q->receiver_id == 0)
    // {
    //     POTENTIAL_FAULT
    //     q->receiver_next = tls->receiver_queue;
    //     POTENTIAL_FAULT
    //     tls->receiver_queue = offset;
    //     POTENTIAL_FAULT
    //     q->receiver_id = thread_id;
    // }
    POTENTIAL_FAULT
    //std::cout  << " \n cxl_unwrap_mend before loop q->start: " << q->start << " ,q->end： " << q->end << " ,offset： " <<offset<< " ,q->buffer[q->start]:" << q->buffer[q->start] <<" ,q->buffer[q->end]: " <<q->buffer[q->end] << std::endl;
    
    while(q->start == q->end || q->buffer[q->start] == 0) {  
        //return CXLRef(this, 0, 0);   
    }
    //std::cout  << " \n cxl_unwrap_mend after loop q->start: " << q->start << " ,q->end： " << q->end << " ,offset： " <<offset<< " ,q->buffer[q->start]:" << q->buffer[q->start] <<" ,q->buffer[q->end]: " <<q->buffer[q->end] << std::endl;
    POTENTIAL_FAULT
    // R1
    //RootRef* tbr = thread_base_ref_alloc();
    POTENTIAL_FAULT
    
    // std::cout  << " \n cxl_unwrap_mend before link_reference:  q->buffer[q->start]: " << q->buffer[q->start] << std::endl;
    // std::cout  << " \n cxl_unwrap_mend before link_reference:  tbr " << tbr << std::endl;
    // std::cout  << " \n cxl_unwrap_mend before link_reference:  tbr->pptr: " << tbr->pptr << std::endl;
    link_reference(tbr->pptr, q->buffer[q->start]);
    POTENTIAL_FAULT
    tbr->ref_cnt += 1;
    POTENTIAL_FAULT
    // R2
    unlink_reference(q->buffer[q->start], q->buffer[q->start]);
    POTENTIAL_FAULT
    // R3
    q->start = (q->start + 1) % MESSAGE_BUFFER_SIZE;
    POTENTIAL_FAULT
    // std::cout  << "cxl_unwrap_mend return CXLRef(this, get_offset_for_data(start, (void*) tbr), tbr->pptr + sizeof(CXLObj));  "  << std::endl;   
    return CXLRef(this, get_offset_for_data(start, (void*) tbr), tbr->pptr + sizeof(CXLObj));

}


CXLRef cxl_shm::cxl_unwrap_wrc(uint64_t offset, cxl_message_queue_t* q, cxl_thread_local_state_t* tls, RootRef* tbr)
{
    // POTENTIAL_FAULT
    //auto t_start = std::chrono::high_resolution_clock::now();
    // cxl_message_queue_t* q = (cxl_message_queue_t*) get_data_at_addr(start, offset);
    // std::cout << "inside of cxl_unwrap_wrc: get_data_at_addr" << get_duration(std::chrono::high_resolution_clock::now(), t_start) << std::endl;
    // POTENTIAL_FAULT
    // // update receiver queue
    // cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);

    // if(q->receiver_id == 0)
    // {
    //     POTENTIAL_FAULT
    //     q->receiver_next = tls->receiver_queue;
    //     POTENTIAL_FAULT
    //     tls->receiver_queue = offset;
    //     POTENTIAL_FAULT
    //     q->receiver_id = thread_id;
    // }
    // POTENTIAL_FAULT
    // t_start = std::chrono::high_resolution_clock::now();
    // std::cout  << "before unwrap while time:" <<  static_cast<uint64_t>(time(NULL)) << ",start" << q->start << ",end " << q->end << ",q->buffer[q->start]" << q->buffer[q->start] << std::endl;
    while(q->start == q->end || q->buffer[q->start] == 0) {
        
        //std::cout << "waiting " << "q->start: " << q->start << ", q->end: " << q->end <<"q->buffer[q->start]" << q->buffer[q->start] << std::endl;
    }
    // std::cout << "inside of cxl_unwrap_wrc: while" << get_duration(std::chrono::high_resolution_clock::now(), t_start) << std::endl;
    
    // std::cout  << "after unwrap while time:" << static_cast<uint64_t>(time(NULL)) << ",start" << q->start << ",end " << q->end << ",q->buffer[q->start]" << q->buffer[q->start] << std::endl;
    POTENTIAL_FAULT
    // R1
    //t_start = std::chrono::high_resolution_clock::now();
    //RootRef* tbr = thread_base_ref_alloc(tls);
    // std::cout << "inside of cxl_unwrap_wrc: thread_base_ref_alloc" << get_duration(std::chrono::high_resolution_clock::now(), t_start) << std::endl;
    
    POTENTIAL_FAULT

    // std::cout  << "tbr:" << tbr << std::flush;
    //t_start = std::chrono::high_resolution_clock::now();
    
    std::cout << "cxl_unwrap_wrc before link_reference, 0 q: " << q << std::endl;
    std::cout << "cxl_unwrap_wrc before link_reference, 00 q: " << q <<" ,tbr:" << tbr << std::endl;
    std::cout << "cxl_unwrap_wrc before link_reference, 1tbr->pptr: " << tbr->pptr << " ,q: " << q << std::endl;
    std::cout << "cxl_unwrap_wrc before link_reference, 2tbr->pptr: " << tbr->pptr << " ,q->start: " << q->start << std::endl;
    std::cout << "cxl_unwrap_wrc before link_reference, 3q->buffer[q->start]: " << q->buffer[q->start] << std::endl;
    link_reference(tbr->pptr, q->buffer[q->start]);
    // std::cout << "inside of cxl_unwrap_wrc: link_reference" << get_duration(std::chrono::high_resolution_clock::now(), t_start) << std::endl;
    
    POTENTIAL_FAULT
    tbr->ref_cnt += 1;
    POTENTIAL_FAULT
    // R2
    //t_start = std::chrono::high_resolution_clock::now();
    auto a = q->buffer[q->start];
    std::cout << "cxl_unwrap_wrc before unlink_reference, q->buffer[q->start]: " << a << " ,q->start: " << q->start << std::endl;

    unlink_reference(q->buffer[q->start], q->buffer[q->start]);
    // std::cout << "inside of cxl_unwrap_wrc: unlink_reference" << get_duration(std::chrono::high_resolution_clock::now(), t_start) << std::endl;

    // std::cout  << "unwrap info q->start" << q->start << ",q->end " << q->end << ",offset" <<offset<<",q->buffer[q->end]" <<q->buffer[q->end] << std::endl;
    POTENTIAL_FAULT
    // R3
    q->start = (q->start + 1) % MESSAGE_BUFFER_SIZE;
    POTENTIAL_FAULT
    return CXLRef(this, get_offset_for_data(start, (void*) tbr), tbr->pptr + sizeof(CXLObj));
}

uint64_t cxl_shm::create_msg_queue(uint16_t dst_id)
{
    cxl_message_queue_t* q = msg_queue_alloc(thread_id, dst_id);
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    q->sender_next = tls->sender_queue;
    tls->sender_queue = get_offset_for_data(start, q);
    return get_offset_for_data(start, q);
}