#include <cstdlib>
#include <iostream>
#include <cstring>
#include <cassert>
#include <pthread.h>
#include "../common/common.h"

#define DATASIZE 64
struct item_t {
	uint64_t val;
	item_t* next;
	char padding[DATASIZE - sizeof(uint64_t) - sizeof(item_t*)];
};

class Queue {
private:
	char padding1[DATASIZE - sizeof(uint64_t)];
	uint64_t tail;
	char padding2[DATASIZE - sizeof(uint64_t)];
	uint64_t head;
	uint64_t tailValid;
	
	char padding3[DATASIZE - sizeof(uint64_t)];
	pthread_mutex_t head_lock;
	pthread_mutex_t tail_lock;
	//unsigned len;
	//uint64_t tail;

public:
	void enqueue(item_t* new_location, item_t* temp_item, int id);
	void init_enqueue(uint64_t _val, item_t* new_location);
	void dequeue();
	void print_queue(); 
	Queue();
};

Queue::Queue() {
	tail = 0;
	head = 0;
	tailValid = 0;
}

void
Queue::enqueue(item_t* new_location, item_t* temp_item, int id) {
	memcpy(new_location, temp_item, sizeof(item_t));
	((item_t*)(tail))->next = new_location;

	flush_caches(new_location, sizeof(item_t));
	s_fence();

	tail = (uint64_t)new_location;
	flush_caches(&tail, sizeof(tail));
	s_fence();
}

void
Queue::init_enqueue(uint64_t _val, item_t* new_location) {
	// if first item
	item_t* new_item;
	if (new_location)
		new_item = new_location;
	else
		new_item = (item_t*)aligned_malloc(64, sizeof(item_t)); 

	new_item->val = _val;
	if (!head) {
		head = (uint64_t)new_item;
	} else {
		((item_t*)(tail))->next = new_item;
	}
	
	tail = (uint64_t)new_item;
}



void
Queue::dequeue() {
	//pthread_mutex_lock(&head_lock);
	item_t* victim = (item_t*)(head);
	head = (uint64_t)(victim->next);
    flush_caches(&head, sizeof(head));
	s_fence();
	//pthread_mutex_unlock(&head_lock);
	//free(victim);
}


void
Queue::print_queue() {
	item_t* curr = (item_t*)(head);
	while (tail != (uint64_t)curr) {
		std::cout << curr->val << "->";
		curr = curr->next;
	}
	std::cout << curr->val << std::endl;
}
