#ifndef SHIFTLAB_TXOPT_COMMON_H__
#define SHIFTLAB_TXOPT_COMMON_H__

#include "../txopt_cfg.hh"
#include "param.hh"



#define BASE_ADDR (8192UL)

// #define INIT_METADATA_CACHE_VADDR (STATUS_OUTPUT_VADDR+1024)
// #define INIT_METADATA_CACHE_PADDR (STATUS_OUTPUT_PADDR+1024)

#define MAX_THREADS 64

// add this value to any write's paddr
// the new address is the corresponding counter's paddr
#define COUNTER_ADDR_DIFF ((BASE_ADDR)*1024*1024 + 1)
// counter atomic write sends Ack to this address+paddr
// #define ACK_ADDR_DIFF (1024UL*1024*1024)

//to be assigned
#define VERIFICATION_ADDR_DIFF ((BASE_ADDR+1024)*1024*1024 + 1)

#define VERIFICATION_TREE_HEIGHT 12

#define NUM_WAY 16

#ifdef IDEAL
#define COUNTER_CACHE_LINE_SIZE 64
#else
#define COUNTER_CACHE_LINE_SIZE (64*8)
#endif

#define CACHE_LINE_SIZE 64

#define PAGE_SIZE_COMMON 4096

//counter queues
//#define COUNTER_READ_QUEUE_SIZE 16
#define COUNTER_WRITE_QUEUE_SIZE 16

#define VERIFICATION_CACHE_LINE_SIZE 64
#define VERIFICATION_WRITE_QUEUE_SIZE 16


// customized debug flags
#include "debug/myflag.hh"
#include "debug/myflag1.hh"
#include "debug/myflag2.hh"
#include "debug/myflag3.hh"
#include "debug/myflag_status.hh"

// some commonly used stl containers
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <vector>
#include <deque>


#endif // SHIFTLAB_TXOPT_COMMON_H__
