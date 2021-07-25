#include <cstdlib>
#include <stdio.h>
// #include <pthread.h>
#include <new>
#include <cstring>
#include "../common/common.h"

#define DATASIZE 64

struct item_t {
	uint64_t val;
	char padding[DATASIZE - sizeof(uint64_t)];
};

struct hash_table_t {
	item_t item;
	//uint64_t val;
	uint64_t next;
	//char padding[DATASIZE - sizeof(DATASIZE)];

	//counter atomic
	//hash_table_t* next;
	
	hash_table_t() {
		item.val = 0;
		next = 0;
	}
};

class HashTable {
	public:
		HashTable();
		HashTable(hash_table_t* _start, unsigned _size/*, pthread_mutex_t* _locks*/);
		uint64_t hash(uint64_t _val);
		void insert(hash_table_t* new_item, item_t* temp_item);
		void init_insert(hash_table_t* new_item);
		void erase(uint64_t _val);

		void printHashTable();

		//pthread_mutex_t* locks;
		unsigned size;
		hash_table_t* start;
};

HashTable::HashTable(hash_table_t* _start, unsigned _size/*, pthread_mutex_t* _locks*/) {
	start = _start;
	size = _size;
	//locks = _locks;
}

HashTable::HashTable() {}

uint64_t HashTable::hash(uint64_t _val) {
	return (_val * 49979693UL) % size;
}

/*
uint64_t HashTable::hash(uint64_t _val) {
	uint64_t hash_rtn = 11235727;
	hash_rtn ^= (_val << 11) | (_val >> 3);
	//printf("hash(%lu)=%lu\n",_val, hash_rtn);
	//printf("size=%u\n",size);
	return hash_rtn % size;
}
*/

void HashTable::init_insert(hash_table_t* new_item) {
	uint64_t _val = new_item->item.val;
	//CounterAtomic next;
	//char padding[64 - sizeof(CounterAtomic)];
	uint64_t val_hash = hash(_val);
	hash_table_t* location = start + val_hash;
		
	while (true) {
		if (!location->next) {
			// need cache flush
			//CounterAtomic::s_barrier();
			//CounterAtomic::cache_flush((uint64_t)new_item);
			//CounterAtomic::s_barrier();
			//CounterAtomic::counter_cache_flush((uint64_t)new_item);
			//CounterAtomic::s_barrier();
			
			location->next = (uint64_t)new_item;
			break;
		} else {
			location = (hash_table_t*)(location->next);
		}
	}
}

void HashTable::insert(hash_table_t* new_item, item_t* temp_item) {
	uint64_t _val = temp_item->val;
	uint64_t val_hash = hash(_val);
	//printf("insert:%lu\n", val_hash);
	//printf("lock=%lu\n", val_hash);
	//pthread_mutex_lock(locks + val_hash);
	//printf("get lock=%lu\n", val_hash);
	hash_table_t* location = start + val_hash;
	new_item->item = *temp_item;
	// need cache flush
	//s_fence();
	cache_flush(&new_item->item, sizeof(item_t));
	//CounterAtomic::s_barrier();
	s_fence();
		
	while (true) {
		if (!location->next) {
			
			location->next = (uint64_t)new_item;
			cache_flush(&location->next, sizeof(location->next));
			s_fence();
			//pthread_mutex_unlock(locks + val_hash);
			//return;
			//printf("insert break=%lu\n", val_hash);
			break;
		} else {
			//printf("find next=%lu\n", val_hash);
			//location = &(*location)->next;

			// check duplicate ones
			//if (location->item.val == _val)
			//	break;
			location = (hash_table_t*)(location->next);
			//s_fence();
		}
	}
	//printf("unlock=%lu\n", val_hash);
	//pthread_mutex_unlock(locks + val_hash);
}


// remove the first val found
void HashTable::erase(uint64_t _val) {

	uint64_t val_hash = hash(_val);
	//printf("erase:%lu\n", val_hash);
//	pthread_mutex_lock(locks + val_hash);
	
	hash_table_t* location = (hash_table_t*)(start + val_hash)->next;
	hash_table_t* prev_location = start + val_hash;
	bool find = false;
	while (true) {
		if (!location) { 
			//pthread_mutex_unlock(locks + val_hash);
			//return;
			break;
		}
		//printf("%lu\n",(*location)->item.val);
		find = (location->item.val == _val);
		if (!find) {
			prev_location = location;
			//location = &(*location)->next;
			location = (hash_table_t*)location->next;
		} else {
			prev_location->next = (uint64_t)location->next;
			s_fence();
			break;
		}
	}
	//pthread_mutex_unlock(locks + val_hash);
}


void HashTable::printHashTable() {
	for (int i = 0; i < size; ++i) {
		printf("%d: ", i);
		hash_table_t* curr = start + i;
		while(true) {
			if(!curr) 
				break;
			printf("%lu->",curr->item.val);
			curr = (hash_table_t*)curr->next;
		}
		printf("\n");
	}
}

