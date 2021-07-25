#include "singly_linked_hash.h"
#include "gem5/m5ops.h"

#define SIZE 4007  // size of hash table needs to be a prime
//#define SIZE 2003  // size of hash table needs to be a prime
//#define SIZE 101

HashTable ht;
unsigned num_op = 500;
unsigned init_size = 1000;

unsigned g_seed = 1312515;

hash_table_t* new_locations;

//hash_table_t new_locations[num_op];
//hash_table_t table[SIZE];
//hash_table_t init_table[init_size];


// source: https://software.intel.com/en-us/articles/fast-random-number-generator-on-the-intel-pentiumr-4-processor/
/*
inline unsigned fastrand() {
	g_seed = (179423891 * g_seed + 2038073749); 
	return (g_seed >> 8) & 0x7FFFFFFF;
} 
*/
 
void *opFunc(void* a) {
	int tid = *((int*)a);
	//printf("thread %d\n", id);
	for (int i = 0; i < num_op; ++i) {
		uint64_t num = rand() % (init_size * 2);
		//uint64_t num = i;
		//if (i % 2) {
			//(new_locations[id] + i)->item.val = num;
			item_t temp_item;
			temp_item.val = num;
			ht.insert(new_locations + i, &temp_item);
		//} else {
		//	ht.erase(num);
		//}
	}
	return NULL;
}

int main (int argc, char *argv[]) {
	int id;

	//printf("malloc new locations\n");
	id = 0;
	new_locations = (hash_table_t*)aligned_malloc(64, sizeof(hash_table_t) * num_op);
	//printf("new locations[%d] %lu\n", i, (uint64_t)new_locations[i]);
	for (int j = 0; j < num_op; ++j) {
		new(new_locations + j) hash_table_t();
	}
	
	//printf("malloc init table\n");

	hash_table_t* init_table = (hash_table_t*)aligned_malloc(64, init_size * sizeof(hash_table_t));
	for (int i = 0; i < init_size; ++i) {
		new(init_table + i) hash_table_t();
	}

	//printf("malloc table\n");	
	hash_table_t* table = (hash_table_t*)aligned_malloc(64, SIZE * sizeof(hash_table_t));
	for (int i = 0; i < SIZE; ++i) {
		new(table + i) hash_table_t();
	}

	// initialize hash table
	ht.start = table;
	ht.size = SIZE;
	//ht.locks = locks;
	for (int j = 0; j < init_size; ++j) {
		(init_table + j)->item.val = j;
		ht.init_insert(init_table + j);
	}

	//exit(1);
	int coreid = atoi(argv[1]);
	m5_work_begin(coreid,0);
	opFunc(&id);
	m5_work_end(coreid,0);
	return 0;  //  opFunc((void*)(&id[0]));
	//ht.printHashTable();
}
