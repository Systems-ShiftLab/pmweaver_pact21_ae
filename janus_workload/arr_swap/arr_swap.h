#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <cassert>
#include <iostream>
//#include <pthread.h>
#include "../common/common.h"

#define DATASIZE 64
struct item_t {
	uint64_t val;
	// Korakit use def for data size
	char padding[DATASIZE - sizeof(uint64_t)];
};

struct backup_t {
	item_t backup_item;
	unsigned backup_location; 
};

class ArraySwap {
	public:
		// Korakit, Switched from counteratomic to normal commit var
		uint64_t backup_valid;
		item_t* start;
		unsigned size;
		backup_t* backup;
		
        void swap(unsigned a, unsigned b);
		void setValue(unsigned idx, uint64_t _val);
		//ArraySwap(item_t* _start, item_t* _backup, unsigned _size);
};

/*
ArraySwap::ArraySwap(item_t* _start, item_t* _backup, unsigned _size) {
	size = _size;
	start = _start;
	backup = _backup;
	backup_valid = 0;
}
*/

void
ArraySwap::setValue(unsigned idx, uint64_t _val) {
	(start + idx)->val = _val;
}

void
ArraySwap::swap(unsigned a, unsigned b) {
		// make backup
	backup[0].backup_item = start[a];
	backup[1].backup_item = start[b];
	backup[0].backup_location = a;
	backup[1].backup_location = b;
	cache_flush(backup, 2 * sizeof(backup_t));
	s_fence();

	backup_valid = 1;
	cache_flush(&backup_valid, sizeof(backup_valid));
	s_fence();
	// swap
	item_t temp;
	temp = start[a];
	start[a] = start[b];
	start[b] = temp;
	// flush counter cache and set commit
	
	//s_fence();
	cache_flush(&start[a], sizeof(item_t));
	cache_flush(&start[b], sizeof(item_t));
	s_fence();
	
	backup_valid = 0;
	cache_flush(&backup_valid, sizeof(backup_valid));
	s_fence();
}
