#include "cxlmalloc.h"
#include "cxlmalloc-internal.h"

void cxl_page_queue_s::cxl_page_queue_push(cxl_page_t* page, void* start)
{
    #ifdef FAULT_INJECTION
    std::mt19937 mt_rand = std::mt19937(std::random_device{}());
    std::uniform_int_distribution<int> dis = std::uniform_int_distribution<int>(1, 50);
    #endif
    POTENTIAL_FAULT
    uint64_t page_offset = get_offset_for_data(start, page);
    POTENTIAL_FAULT
    page->next = first;
    POTENTIAL_FAULT
    page->prev = 0;
    POTENTIAL_FAULT
    cxl_page_t* first_page = first == 0 ? NULL : (cxl_page_t*) get_data_at_addr(start, first);
    POTENTIAL_FAULT
    cxl_page_t* last_page = last == 0 ? NULL : (cxl_page_t*) get_data_at_addr(start, last);
    POTENTIAL_FAULT
    if(first_page != NULL)
    {
        POTENTIAL_FAULT
        first_page->prev = page_offset;
        POTENTIAL_FAULT
        first = page_offset;
        POTENTIAL_FAULT
    }
    else
    {
        POTENTIAL_FAULT
        first = page_offset;
        POTENTIAL_FAULT
        last = page_offset;
        POTENTIAL_FAULT
    }
}

void cxl_page_queue_s::cxl_page_queue_remove(cxl_page_t* page, void* start)
{
    #ifdef FAULT_INJECTION
    std::mt19937 mt_rand = std::mt19937(std::random_device{}());
    std::uniform_int_distribution<int> dis = std::uniform_int_distribution<int>(1, 50);
    #endif
    POTENTIAL_FAULT
    uint64_t page_offset = get_offset_for_data(start, page);
    POTENTIAL_FAULT
    cxl_page_t* prev_page = page->prev == 0 ? NULL : (cxl_page_t*) get_data_at_addr(start, page->prev);
    POTENTIAL_FAULT
    cxl_page_t* next_page = page->next == 0 ? NULL : (cxl_page_t*) get_data_at_addr(start, page->next);
    POTENTIAL_FAULT
    if(prev_page != NULL) 
    {
        prev_page->next = page->next;
    }
    POTENTIAL_FAULT
    if(next_page != NULL) next_page->prev = page->prev;
    POTENTIAL_FAULT
    if(page_offset == last) 
    {
        POTENTIAL_FAULT
        last = page->prev;
        POTENTIAL_FAULT
    }
    POTENTIAL_FAULT
    if(page_offset == first) 
    {
        POTENTIAL_FAULT
        first = page->next;
        POTENTIAL_FAULT
    }
    POTENTIAL_FAULT
    page->next = 0;
    POTENTIAL_FAULT
    page->prev = 0;
    POTENTIAL_FAULT
}