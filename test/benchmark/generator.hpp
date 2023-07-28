#ifndef RANDOM_GEN_H
#define RANDOM_GEN_H

#include <algorithm>
#include <ctime>
#include <random>
#include <vector>
#include <fstream>
#include <iostream>
#include <cassert>
#include <cstdarg>
#include <cstring>

#define forceinline		inline __attribute__((always_inline))
#define packed          __attribute__((packed))
#define CACHE_LINE_SIZE 64

#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */

void REDLOG(const char *format, ...)
{   
    va_list args;

    char buf1[1000], buf2[1000];
    memset(buf1, 0, 1000);
    memset(buf2, 0, 1000);

    va_start(args, format);
    vsnprintf(buf1, 1000, format, args);

    snprintf(buf2, 1000, "\033[31m%s\033[0m", buf1);
    printf("%s", buf2);
    //write(2, buf2, 1000);
    va_end( args );
}

class RandomGen {
public:
	std::random_device req_rd;
	uint64_t *random_req_data;
	std::vector<int> shuffle_req_data;
	int shuffle_req_data_len;
	int shu_counter;
	int ran_counter;
	int max_len;
	bool use_skew;
	RandomGen(int max_len_): max_len(max_len_), ran_counter(0) {
		char *res = getenv("USE_SKEW");
		if (res != nullptr) {
			use_skew = (atoi(res) == 0 ? false : true);
		}
		if (use_skew) {
			// REDLOG("[USE SKEW DATA]\n");
			skew();
		} else {
			// REDLOG("[USE UNIFORM DATA]\n");
			uniform();
		}
	}
	RandomGen(int max_len_, int max_level, double P): max_len(max_len_), ran_counter(0), shu_counter(0) {

	}
	~RandomGen() {

	}

	void gen_shuffle_req_data(int max_gen_num) {
		shuffle_req_data.resize(max_gen_num);
		std::mt19937 req_dis(req_rd());
		shuffle_req_data_len = max_gen_num;
		shu_counter = 0;
		for (int i = 1 ; i <= max_gen_num; i ++ ) {
			shuffle_req_data[i - 1] = i;
		}
		std::shuffle(shuffle_req_data.begin(), shuffle_req_data.end(), req_dis);
	}
	uint64_t get_shuffle_req_num() {
		assert(shu_counter < shuffle_req_data_len);
		return static_cast<uint64_t>(shuffle_req_data[shu_counter ++]);
	}

    uint64_t get_shuffle_req_num_inf() {
		return static_cast<uint64_t>(shuffle_req_data[shu_counter ++ % max_len]);
	}

	uint64_t get_ran_op() {
		assert(shu_counter + 1 < shuffle_req_data_len);
		return static_cast<uint64_t>(shuffle_req_data[shu_counter + 1]);
	}

	void skew() {
		random_req_data = new uint64_t[max_len];
		std::ifstream myfile;
	    myfile.open("./tool/YCSB/src/data/0.dat");
	    long temp;
		for (int i = 0; i < max_len; i ++ ) {
			myfile >> temp;
			random_req_data[i] = (int)(temp % (1 << 30));	
		}	
	}
	void uniform() {

		random_req_data = new uint64_t[max_len];
		for (int i = 0; i < max_len; i ++ ) {
			random_req_data[i] = req_rd(); 
			// std::cout<<random_req_data[i]<<std::endl;
		}	
	}

	forceinline uint64_t get_num() {
		ran_counter ++;
		ran_counter %= max_len;
		return random_req_data[ran_counter ];
	}
	forceinline uint64_t get_num(int mod) {
		ran_counter ++;
		ran_counter %= max_len;
		return random_req_data[ran_counter ] % mod;
	}
};

#endif