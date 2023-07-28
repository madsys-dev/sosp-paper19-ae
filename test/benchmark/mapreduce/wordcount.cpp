#include "common.h"

static void rand_string(char *randomString, size_t length) {

    static char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";        

    if (length) {

        if (randomString) {            
            for (int n = 0;n < length;n++) {            
                int key = rand() % (int)(sizeof(charset) -1);
                randomString[n] = charset[key];
            }

            randomString[length] = '\0';
        }
    }

    return;
}

static char *rand_txt(size_t length) {

    srand(time(0));

    char *workload = nullptr;
    posix_memalign((void **)&workload, 4096, length);
    char *cur = workload;
    int index = 0;
    do {
        size_t word_len = rand() % 10 + 2;
        rand_string(cur, word_len);
        cur += word_len + 1;
        index += word_len + 1;
    } while (index < length);
    return workload;
}

class WordCount: public MapReduce {
public:
    WordCount(int map_num_=1, int reduce_num_=1):
        MapReduce(map_num_, reduce_num_){

    }
    
    uint64_t hash_func(char *h) const
    {
        uint64_t v = 14695981039346656037ULL;
        while (*h != 0)
            v = (v ^ (size_t)(*(h++))) * 1099511628211ULL;
        return v;
    }

    uint64_t djb_hash(char* cp) const
    {
        uint64_t hash = 5381;
        while (*cp)
            hash = 33 * hash ^ (unsigned char) *cp++;
        return hash;
    }

    int shuffle_func(char *h) {
        // uint64_t hash = djb_hash(h);
        int ret_id = (djb_hash(h) / 3) % reduce_num;
        // printf("HASH: %d %d %d\n", hash, reduce_num, ret_id);
        return ret_id;
    }

    void emit_intermediate(cxl_shm *shm, std::list<imm_data> *inter, char *word, int len) {
        if (inter->empty() || inter->back().count + len + 1 > imm_data_size) {
            struct imm_data inter_en;
            inter_en.count = 0;
            inter_en.data = shm->cxl_malloc(imm_data_size, 0).get_addr();
            // printf("SHIT\n");
            inter->push_back(inter_en);
        }
        
        memcpy((char *)inter->back().data + inter->back().count, word, len);
        inter->back().count += len;
    }

    void map_func(void *map_data, int task_id, cxl_shm *shm, int data_length) {
        char *str_data = (char *)map_data;
        /*
        for (uint64_t i = 0; i < data_length; i++)
        {
            str_data[i] = toupper(str_data[i]);
        }
        */
        uint64_t i = 0;
        while(i < data_length)
        {            
            while(i < data_length && (str_data[i] < 'A' || str_data[i] > 'Z'))
                i++;
            uint64_t start = i;
            while(i < data_length && ((str_data[i] >= 'A' && str_data[i] <= 'Z') || str_data[i] == '\''))
                i++;
            if(i > start)
            {
                str_data[i] = 0;
                char* word = { str_data+start };
                
                int word_len = i - start;
                int reduce_id = shuffle_func(word);
                emit_intermediate(shm, vec->at(get_vec_index(task_id, reduce_id)), word, word_len + 1);
                // printf("Map %d Reduce %d: %s\n", task_id, reduce_id, word);
            }
        }
    }
    
    void reduce_func(int task_id, cxl_shm *shm) {
        for (int map_id = 0; map_id < map_num; map_id ++ ) {
            std::list<imm_data> *inter = vec->at(get_vec_index(map_id, task_id));
            std::list<imm_data>::iterator it;
            std::unordered_map<std::string, int> cal;
            int iter_count = 0;
            for (it = inter->begin(); it != inter->end(); it ++) {
                char *str_data = (char *)it->data;
                int data_length = it->count;
                // printf("iter: %d, len: %d\n", iter_count, data_length);
                
                int i =0;
                while (i < data_length) {          
                    while(i < data_length && (str_data[i] < 'A' || str_data[i] > 'Z'))
                        i++;
                    uint64_t start = i;
                    while(i < data_length && ((str_data[i] >= 'A' && str_data[i] <= 'Z') || str_data[i] == '\''))
                        i++;
                    if(i > start)
                    {
                        char* word = { str_data+start };
                        int word_len = i - start;
                        cal[std::string(word)] ++;
                        // printf("Map %d Reduce %d %d: %s\n", map_id, task_id, iter_count, word);
                    }
                }
                iter_count ++;
            }
    
        }
    }

    void splice(char **data_arr, size_t *data_dis, char *map_data, int data_length) {
        int map_dis = data_length / map_num;
        int final_dis = data_length - (map_num-1) * map_dis;
        for (int i = 0; i < map_num; i++ ) {
            data_dis[i] = (i == map_num - 1) ? final_dis : map_dis;
            data_arr[i] = map_data + i * map_dis;
        }
    }
};

void simple_test() {
    char a[] = {"xxxx xyyy zzzz tttt xxxx xyyy zzzz kaka pppp zzzz tttt iiii\n"};
    
    WordCount *mp = new WordCount(3,2);
    mp->run_mr(a, strlen(a));
}

void stress_test(int argc, char **argv) {
    
    if (argc < 4) {
        exit(1);
    }

    int map_num = atoi(argv[1]);
    int reduce_num = atoi(argv[2]);
    int data_length = atoi(argv[3]);

    char *str = rand_txt(data_length);

    WordCount *mp = new WordCount(map_num,reduce_num);
    mp->run_mr(str, data_length);
}

int main(int argc, char **argv) {
    stress_test(argc, argv);
    return 0;
}