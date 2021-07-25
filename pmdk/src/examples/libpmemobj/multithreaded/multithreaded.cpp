#include "../map/data_store.h"
#include "../linkedlist/fifo_bulk.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

#ifdef GEM5
	#include "gem5/m5ops.h"
#endif

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage:\n\t%s iterations\n", argv[0]);
    }
    
    char *iterations = argv[1];

    struct params {
        char *path;
        char *name;
        char *pool;
        char *iter;

        params(char path[], char name[], char pool[], char iter[]) {
            this->name = name;
            this->pool = pool;
            this->iter = iter;
        }
    };
    
    std::vector<params> mapParameters = {
        params((char*)"dummy-path", (char*)"hashmap_rp", (char*)"/mnt/pmem0/data_gen_hashmap_rp", iterations),
        params((char*)"dummy-path", (char*)"btree", (char*)"/mnt/pmem0/data_gen_btree", iterations),
        params((char*)"dummy-path", (char*)"rbtree", (char*)"/mnt/pmem0/data_gen_rbtree", iterations),
        params((char*)"dummy-path", (char*)"ctree", (char*)"/mnt/pmem0/data_gen_ctree", iterations),
        params((char*)"dummy-path", (char*)"hashmap_tx", (char*)"/mnt/pmem0/data_gen_hashmap_tx", iterations),
        params((char*)"dummy-path", (char*)"skiplist", (char*)"/mnt/pmem0/data_gen_skiplist", iterations),
        params((char*)"dummy-path", (char*)"hashmap_atomic", (char*)"/mnt/pmem0/data_gen_hashmap_atomic", iterations)
    };

    params fifoParameters = params((char*)"dummy-path", (char*)"/mnt/pmem0/data_gen_fifo_bulk", iterations, (char*)"hashmap_atomic");
    
#if defined(GEM5)
		TraceBegin();
		m5_work_begin(0,0);
#endif

    for (auto param : mapParameters) {
        main_datastore(sizeof(params)/sizeof(char*), (char**)&param);
    }    
    main_fifo_bulk(sizeof(params)/sizeof(char*) - 1, (char**)&fifoParameters);
    
#if defined(GEM5) and not defined(SHIFTLAB_MULTITHREADED)
    TraceEnd();
    m5_work_end(0,0);
#endif

}