#ifndef SHIFTLAB_PARAM_H__
#define SHIFTLAB_PARAM_H__

// Additional latency added to atomic counter packets
#define  COUNTER_ATOMIC_LATENCY (20000UL)
// add latency to metadata cache misses
#define METADATA_CACHE_MISS_LATENCY (40000UL)
// #define FLUSH_LATENCY 20000
// #define COMPRESSION_LATENCY 300000
#define IV_HASH_LATENCY (40000UL)
#define DE_DUP_HASH_LATENCY (300000UL)

#define ENCRYPTION_LATENCY (40000UL)

// Korakit
// latency for data only backend ops, addr only backend ops and addr+data backend ops
// need to be changed
#define DATA_WRITE_BACKEND_LATENCY_STATIC (40000UL)
#define ADDR_WRITE_BACKEND_LATENCY_STATIC (40000UL)
#define ADDR_AND_DATA_WRITE_BACKEND_LATENCY_STATIC (30000UL)
#define WEAR_LEVELLING (0UL)

#define BLOCK_READ_LATENCY (100000UL)

//#define DUP_RATE 50

#ifdef COMPRESSION
#define COMPRESSION_RATIO 0.5
#else
#define COMPRESSION_RATIO 1
#endif


#endif // SHIFTLAB_PARAM_H__