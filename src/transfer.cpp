#include "cxlmalloc.h"
#include "cxlmalloc-internal.h"

uint64_t cxl_shm::cxl_wrap(CXLRef& ref)
{
    return ref.get_tbr()->pptr;
}

bool cxl_shm::sent_to(uint64_t queue_offset, CXLRef& ref)
{
    POTENTIAL_FAULT
    uint64_t offset = ref.get_tbr()->pptr;
    POTENTIAL_FAULT
    cxl_message_queue_t* q = (cxl_message_queue_t*) get_data_at_addr(start, queue_offset);
    POTENTIAL_FAULT
    if(q->end + 1 == q->start || q->end - q->start == MESSAGE_BUFFER_SIZE - 1) 
    {
        return false;
    }
    

    // S1
    POTENTIAL_FAULT
    link_reference(q->buffer[q->end], offset);
    POTENTIAL_FAULT
    // S2
    q->end = (q->end + 1) % MESSAGE_BUFFER_SIZE;
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

uint64_t cxl_shm::create_msg_queue(uint16_t dst_id)
{
    cxl_message_queue_t* q = msg_queue_alloc(thread_id, dst_id);
    cxl_thread_local_state_t* tls = (cxl_thread_local_state_t*) get_data_at_addr(start, tls_offset);
    q->sender_next = tls->sender_queue;
    tls->sender_queue = get_offset_for_data(start, q);
    return get_offset_for_data(start, q);
}