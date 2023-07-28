#include "common.h"

#include <fstream>
#include <iostream>
#include <map>

class PageRank: public MapReduce {
public:
    size_t mat_wid;
    size_t row_dis;
    PageRank(int map_num_=1, int reduce_num_=1, size_t mat_wid_=1):
        MapReduce(map_num_, reduce_num_), mat_wid(mat_wid_){
        row_dis = mat_wid / map_num;
    }

    int shuffle_func(uint64_t id) {
        // uint64_t hash = djb_hash(h);
        // printf("HASH: %d %d %d\n", hash, reduce_num, ret_id);
        return (id / row_dis) % map_num;
    }

    void map_func(void *map_data, int task_id, cxl_shm *shm, int data_length) {
        uint64_t *mat_data = (uint64_t *)map_data;
        // printf("%p\n", map_data);
        size_t mat_data_len = data_length / sizeof(uint64_t);
        size_t index = 0;
        while (index < mat_data_len) {
            size_t row_len = *(mat_data+1);
            mat_data += 2;
            double *P = (double *)mat_data;
            *P = *P / row_len;
            mat_data += 1;
            if (row_len > 875712) {
                printf("[D]%ld\n", row_len);
            }

            for (int i = 0; i < row_len; i ++ ) {
                int reduce_id = shuffle_func(mat_data[i]);
                // printf("%d %d %d %d\n", row_len, task_id,  reduce_id, mat_data[i]);
                emit_intermediate(shm, vec->at(get_vec_index(task_id, reduce_id)),  \
                    (char *)mat_data + sizeof(uint64_t) * i, sizeof(uint64_t));
                emit_intermediate(shm, vec->at(get_vec_index(task_id, reduce_id)),  \
                    (char *)P, sizeof(double)); 
            }
            index += row_len + 3;
        }
                // printf("Map %d Reduce %d: %s\n", task_id, reduce_id, word);
    }
    
    void reduce_func(int task_id, cxl_shm *shm) {
        double *row_hmap = (double *)malloc(sizeof(double) * row_dis);
        for (int map_id = 0; map_id < map_num; map_id ++ ) {
            std::list<imm_data> *inter = vec->at(get_vec_index(map_id, task_id));
            std::list<imm_data>::iterator it;
            int iter_count = 0;
            
            for (it = inter->begin(); it != inter->end(); it ++) {
                uint64_t *id_arr = (uint64_t *)it->data;
                double *p_arr = (double *)it->data;
                int data_length = it->count / (sizeof(uint64_t) + sizeof(double));
                // printf("%d %ld %lf\n",task_id, *id_arr, *(p_arr+1));
                for (int i = 0; i < data_length; i ++ ) {
                    row_hmap[id_arr[2*i] % row_dis] += p_arr[2*i+1];
                }
                
            }
        }
        // for (int i = 0; i < row_dis; i ++ ) {
        //     printf("%lf\n", row_hmap[i]);
        // }
    }

    void splice(char **data_arr, size_t *data_dis, char *map_data, int data_length) {
        uint64_t *mat_data = (uint64_t *)map_data;
        size_t mat_data_len = data_length / sizeof(uint64_t);
        size_t index = 0;
        size_t pre_index = 0;
        // int counter = 0;
        int phase = 0;
        while (index < mat_data_len) {
            uint64_t src = mat_data[index];
            size_t row_len = mat_data[index + 1];
            // counter ++;
            index += row_len + 3;
            if (src / row_dis == phase && src / row_dis != map_num) {
                data_dis[phase] = (index - pre_index) * sizeof(uint64_t);
                data_arr[phase] = map_data + pre_index * sizeof(uint64_t);
                pre_index = index;
                phase ++;
            }
        }
    }

};

void simple_test(int argc, char **argv) {

    char *str = (char *)malloc(8 * 8);
    double *p_arr = (double *)str;
    uint64_t *id_arr = (uint64_t *)str;
    p_arr[2] = 0.5;
    p_arr[6] = 0.4;
    id_arr[0] = 0ULL;
    id_arr[1] = 1ULL;
    id_arr[3] = 1ULL;
    id_arr[4] = 1ULL;
    id_arr[5] = 1ULL;
    id_arr[7] = 0ULL;

    PageRank *mp = new PageRank(2,2,2);
    mp->run_mr(str, 8 * 8);
}

void stress_test(int argc, char **argv) {

    int map_num = atoi(argv[1]);
    int reduce_num = atoi(argv[2]);
    int max_id = atoi(argv[3]);
    int line_num = atoi(argv[4]);
    char *file_name = argv[5];

    srand(time(0));

    std::vector<uint64_t> *arr = new std::vector<uint64_t>[max_id];
    std::map<uint64_t, uint64_t> mp;

    std::ifstream myfile;
	myfile.open(file_name);
    uint64_t src, dst;
    for (int i = 0; i < line_num; i ++ ) {
        myfile >> src >> dst;
        arr[src].push_back(dst);
    }

    size_t length = ((max_id * 3) + line_num) * sizeof(uint64_t);
    char *workload = nullptr;
    posix_memalign((void **)&workload, 4096, length);
    uint64_t *mem = (uint64_t *)workload;

    for (int i = 0; i < max_id; i ++ ) {
        mem[0] = i;
        int item_len = arr[i].size();
        mem[1] = item_len;
        ((double *)mem)[2] = 0.0; //1.0 * (rand() % 100) / 100;
        for (int j = 0; j < item_len; j ++ ) {
            mem[3 + j] = arr[i][j];
        }
        mem += item_len + 3;
    }

    PageRank *pr = new PageRank(map_num, reduce_num, max_id);
    pr->run_mr(workload, length);
}

int main(int argc, char **argv) {
    stress_test(argc, argv);
    return 0;
}