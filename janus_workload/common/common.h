#include <x86intrin.h>
#include <immintrin.h>
#include <stdint.h>

#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>


#ifdef GEM5
const int MMAP_PERSISTENT = 314;
#else
const int MMAP_PERSISTENT = 9;
#endif

void *mmap_persistent(void *start, size_t length, int prot, int flags, int fd, off_t offset);
void *pmalloc(size_t length);

void* aligned_malloc(size_t alignment, size_t size);

void cache_flush(void* addr, size_t size);

void flush_caches(void* addr, size_t size);

void s_fence(void);
