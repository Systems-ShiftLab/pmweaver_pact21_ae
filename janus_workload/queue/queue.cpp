#include "queue.h"
#include "gem5/m5ops.h"
//#include "/home/smahar/git/transparent_txopt/helper.h"

unsigned num_op = 400;
item_t* new_locations;

Queue* q;

unsigned g_seed = 1312515;

// source: https://software.intel.com/en-us/articles/fast-random-number-generator-on-the-intel-pentiumr-4-processor/

inline unsigned fastrand() {
	g_seed = (214013 * g_seed + 2531011); 
	return (g_seed >> 16) & 0x7FFF;
} 

void *opFunc(void* a){
	int tid = *((int*)a);
	for (int i = 0; i < num_op; ++i) {
		unsigned num = fastrand();
		if (num % 2) {
			item_t temp_item;
			temp_item.val = num;
			//printf("enqueue,id=%d\n",id);
			q->enqueue(new_locations + i, &temp_item, tid);
		} else {
			//printf("dequeue,id=%d\n",id);
			q->dequeue();
		}
	}
	return NULL;
}

int main (int argc, char* argv[]) {
	int id;
	
	//printf("queue addr %lu\n", uint64_t(&q));
	q = (Queue*)aligned_malloc(64, sizeof(Queue));
	new (q) Queue();
	// enqueue first
	for (int j = 0; j < 500; ++j) {
		q->init_enqueue(j, 0);
	}

	
	id = 0;
	new_locations = (item_t*)aligned_malloc(64, sizeof(item_t) * num_op);
#ifdef GEM5	
	m5_work_begin(atoi(argv[1]),0);
#endif
	//	TraceBegin();
	opFunc(&id);
	//	TraceEnd();
#ifdef GEM5

	m5_work_end(atoi(argv[1]),0);
#endif
}
