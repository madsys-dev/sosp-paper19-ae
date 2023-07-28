#ifndef tatp_h
#define tatp_h

#include "AllocatorMacro.hpp"
#include "fred.h"

struct Subscriber{
	uint64_t s_id;
	long sub_nbr[2];  // char[15]
	long bit_x;   // bool[10]   0-9 bit is used;
	long hex_x;   	  // bit[10][4]
	long byte2_x[2];   // char[10]
	long msc_location;  //  int 
	std::atomic<long> vlr_location;  //  int
};

struct TatpBenchmark{
    cxl_shm* shm;
	long population;
    Subscriber* sr;
};

#endif