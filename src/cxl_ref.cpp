#include "cxlmalloc.h"
#include "cxlmalloc-internal.h"

CXLRef_s::CXLRef_s(cxl_shm* _shm, uint64_t _tbr, uint64_t _data)
{
    shm = _shm;
    tbr = _tbr;
    data = _data;
}

CXLRef_s::CXLRef_s(cxl_shm* _shm, uint64_t _tbr, uint64_t _data, std::string str_content)
{
    shm = _shm;
    tbr = _tbr;
    data = _data;
    str_content = str_content; 
}

CXLRef_s::CXLRef_s(const CXLRef_s& cxl_ref)
{
    shm = cxl_ref.shm;
    tbr = cxl_ref.tbr;
    data = cxl_ref.data;
    if(tbr != 0)
    {
        RootRef* ref = (RootRef*) get_data_at_addr(shm->get_start(), tbr);
        ref->ref_cnt += 1;
    }
}

CXLRef_s::CXLRef_s(const CXLRef_s&& cxl_ref)
{
    shm = cxl_ref.shm;
    tbr = cxl_ref.tbr;
    data = cxl_ref.data;
}

CXLRef_s& CXLRef_s::operator=(const CXLRef_s& cxl_ref)
{
    shm = cxl_ref.shm;
    tbr = cxl_ref.tbr;
    data = cxl_ref.data;
    if(tbr != 0)
    {
        RootRef* ref = (RootRef*) get_data_at_addr(shm->get_start(), tbr);
        ref->ref_cnt += 1;
    }
    return *this;
}
CXLRef_s& CXLRef_s::operator=(const CXLRef_s&& cxl_ref)
{
    shm = cxl_ref.shm;
    tbr = cxl_ref.tbr;
    data = cxl_ref.data;
    return *this;
}

CXLRef_s::~CXLRef_s()
{   
    POTENTIAL_FAULT_REF
    if(tbr == 0 && data == 0) return;
    POTENTIAL_FAULT_REF
    RootRef* ref = (RootRef*) get_data_at_addr(shm->get_start(), tbr);
    POTENTIAL_FAULT_REF
    ref->ref_cnt --;
    POTENTIAL_FAULT_REF
    if(ref->ref_cnt)
    {
        return;
    }
    POTENTIAL_FAULT_REF
    shm->test_free(ref->pptr);
    POTENTIAL_FAULT_REF
    ref->in_use = 0;
    POTENTIAL_FAULT_REF
    shm->cxl_free(true, (cxl_block*) ref);
    this->tbr = 0;
    this->data = 0;
    this->shm = NULL;
}

void* CXLRef_s::get_addr()
{
    if(data == 0) return NULL;
    return get_data_at_addr(shm->get_start(), data);
}

RootRef* CXLRef_s::get_tbr()
{
    if(tbr == 0) return NULL;
    return (RootRef*)get_data_at_addr(shm->get_start(), tbr);
}
