#include "common.h"

#include <fstream>
#include <iostream>
#include <map>

const static size_t max_value = 10000;

void generate_points(uint64_t *pts, int size, int dim, bool padding) {   
   int i, j;
   
   int dis = dim;
   if (padding) dis += 1; 
   for (i = 0; i < size; i ++) {
      for (j = 0; j < dim; j++) {
         pts[dis * i + j] = rand() % max_value;
      }
   }
}


class KMeans: public MapReduce {
public:
    size_t num_points; // number of vectors
    size_t dim;       // Dimension of each vector
    size_t num_means; // number of clusters
    size_t grid_size; // size of each dimension of vector space
    size_t row_dis;
    uint64_t *cluster_data;
    KMeans(int map_num_=1, int reduce_num_=1, \
        size_t num_points_=1, size_t dim_=1, size_t num_means_=1): \
        MapReduce(map_num_, reduce_num_), \
        num_points(num_points_), dim(dim_), num_means(num_means_) {
        row_dis = num_means / reduce_num;
        cluster_data = (uint64_t *)malloc(sizeof(uint64_t) * dim * num_means);
        generate_points(cluster_data, num_means, dim, false);
    }

    int shuffle_func(uint64_t id) {
        return 0;
    }

    void map_func(void *map_data, int task_id, cxl_shm *shm, int data_length) {
        uint64_t *mat_data = (uint64_t *)map_data;
        // printf("%p\n", map_data);
        size_t mat_data_len = data_length / sizeof(uint64_t);
        for(int i = 0; i < mat_data_len; i += dim + 1) {
            int cluster_id = 0;
            uint64_t min_dis = 0xffffffffffffULL;
            for (int j = 0; j < num_means; j ++ ) {
                uint64_t cur_dis = 0;
                for (int k = 0; k < dim; k ++) {
                    int x = mat_data[i+k];
                    int y = cluster_data[j*dim+k];
                    cur_dis += (x-y)*(x-y);
                }
                if (cur_dis < min_dis) {
                    cluster_id = j;
                    min_dis = cur_dis;
                    // printf("MIN: %d %ld\n", cluster_id, min_dis);
                }
            }
            mat_data[i + dim] = cluster_id;
            int reduce_id = cluster_id / row_dis <= reduce_num - 1 ? \
               cluster_id / row_dis : reduce_num - 1;
            // printf("Reduce %d\n", reduce_id);
            emit_intermediate(shm, vec->at(get_vec_index(task_id, reduce_id)),  \
            (char *)mat_data + sizeof(uint64_t) * i, sizeof(uint64_t) * (dim + 1) );
        }
    }
    
    void reduce_func(int task_id, cxl_shm *shm) {
        uint64_t *row_hmap = (uint64_t *)malloc(sizeof(uint64_t) * dim * row_dis);
        for (int map_id = 0; map_id < map_num; map_id ++ ) {
            std::list<imm_data> *inter = vec->at(get_vec_index(map_id, task_id));
            std::list<imm_data>::iterator it;
            int iter_count = 0;
            
            for (it = inter->begin(); it != inter->end(); it ++) {
                // printf("%d\n", task_id);
                uint64_t *data = (uint64_t *)it->data;
                int data_length = it->count / sizeof(uint64_t);
                // printf("LEN: %d\n", data_length);
                // printf("%d %ld %lf\n",data, *id_arr, *(p_arr+1));
                for (int i = 0; i < data_length; i += dim + 1 ) {
                    int pos = (data[i + dim] % row_dis) * dim;
                    for (int j = 0; j < dim; j ++ ) {
                        // printf("%d %d %d %d %d\n", map_id, task_id, i, j, data[i + j]);
                        row_hmap[pos + j] += data[i + j];
                    }
                }
                
            }
        }
        // for (int i = 0; i < row_dis; i ++ ) {
        //     printf("%ld\n", row_hmap[i]);
        // }
    }

    void splice(char **data_arr, size_t *data_dis, char *map_data, int data_length) {
        int item_w = (sizeof(uint64_t) * (dim + 1));
        int po_length = data_length / (sizeof(uint64_t) * (dim + 1));
        int po_dis = po_length / map_num;
        int final_dis = po_length - (map_num-1) * po_dis;
        for (int i = 0; i < map_num; i++ ) {
            data_dis[i] = ((i == map_num - 1) ? final_dis : po_dis) * item_w;
            data_arr[i] = map_data + i * po_dis * item_w;
        }
    }

};

void simple_test(int argc, char **argv) {

    char *str = (char *)malloc(8 * 3 * 2);
    uint64_t *p_arr = (uint64_t *)str;
    p_arr[0] = 10;
    p_arr[1] = 200;
    p_arr[3] = 100;
    p_arr[4] = 20;

    KMeans *mp = new KMeans(2,2,2,2,2);
    mp->run_mr(str, 8 * 6);
}

void stress_test(int argc, char **argv) {

    int map_num = atoi(argv[1]);
    int reduce_num = atoi(argv[2]);
    int num_points = atoi(argv[3]);
    int dim = atoi(argv[4]);
    int num_means = atoi(argv[5]);

    srand(time(0));
    
    char *workload = nullptr;
    int length = sizeof(uint64_t) * num_points * (dim + 1);
    posix_memalign((void **)&workload, 4096, length);

    generate_points((uint64_t *)workload, num_points, dim, true);

    KMeans *pr = new KMeans(map_num, reduce_num, num_points, dim, num_means);
    pr->run_mr(workload, length);
}

int main(int argc, char **argv) {
    stress_test(argc, argv);
    return 0;
}