#include "arr_swap.h"
#include "../m5ops.h"
//#include "/home/smahar/git/transparent_txopt/helper.h"

ArraySwap* as;
unsigned size = 10000;
unsigned num_op = 1000;

unsigned g_seed = 1312515;
char padding[48];
// source: https://software.intel.com/en-us/articles/fast-random-number-generator-on-the-intel-pentiumr-4-processor/
inline unsigned fastrand() {
    g_seed = (179423891 * g_seed + 2038073749); 
    return (g_seed >> 8) & 0x7FFFFFFF;
} 

void swapFunc() {
    for (int i = 0; i < num_op; ++i) {
	unsigned idx1 = fastrand() % size;
	unsigned idx2 = fastrand() % size;
	as->swap(idx1, idx2);
    }
}

int main(int argc, char* argv[]) {
    item_t* array = (item_t*) aligned_malloc(64UL, size * sizeof(item_t));
    backup_t* backup = (backup_t*) aligned_malloc(64UL, 2 * sizeof(item_t));

    for (int i = 0; i < size; ++i) {
	array[i].val = i;
    }
	
    as = (ArraySwap*)aligned_malloc(64UL, sizeof(ArraySwap));
    // as = new ArraySwap(array, backup, size);
    as->start = array;
    as->backup = backup;
    as->size = size;

    m5_work_begin(atoi(argv[1]),0);
    // TraceBegin();
    swapFunc();
    // TraceEnd();
    m5_work_end(atoi(argv[1]),0);
    //fprintf(stderr, "done\n");
}
