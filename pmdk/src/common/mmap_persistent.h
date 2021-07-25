#ifndef MMAP_PERSISTENT_DEFINITION_HEADER__
#define MMAP_PERSISTENT_DEFINITION_HEADER__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#define INFO_SUYASH__
#include "../helper_suyash.h"


/* System call number as defined in gem5 */
const int SYSCALL_ID_MMAP_PERSISTENT = 314;
const int SYSCALL_ID_MMAP = 9;

char *expectedPmemPath = NULL;

/* SM: Hacks for -Wmissing-prototypes */
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

/* Simple C function for prefix matching */
int starts_with(const char *pre, const char *str) {
	size_t lenpre = strlen(pre), lenstr = strlen(str);
	return lenstr < lenpre ? 0 : memcmp(pre, str, lenpre) == 0;
}

void init_expected_path() {
	if (!expectedPmemPath) {
		expectedPmemPath = "/mnt/pmem0/";
	}
}

char* concat(const char *s1, const char *s2) {
    char *result = (char*)malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
	/* TODO: Check for malloc errors */
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

int warnedOnce = 0;

/**
 * Wrapper around syscall() for mmap and mmap_persistent, decides between the
 * two options during runtime  */
void* smart_mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset) {
	init_expected_path();
	// exit(1);
	if (!warnedOnce) {
		warn__("%s is not really a smart function, expecting all persistent under: %s ",__FUNCTION__, expectedPmemPath);
		info__("Expected path for persistent files: %s", expectedPmemPath);
		warnedOnce = 1;
	}
	/* Read the file path for the file descriptor */
	char filePath[PATH_MAX];
	char *fddir = (char*)"/proc/self/fd/";
	char *fdstr = (char*)malloc(7); // 6 of max value of fd and +1 for \0
	snprintf(fdstr, 7, "%d", fd);

	char *readlinkpath = concat(fddir, fdstr);
	free(fdstr);

	char *force_mmap_str = getenv("FORCE_MMAP");
	char *force_mmap_persistent_str = getenv("FORCE_MMAP_PERSISTENT");

	uint32_t force_mmap 				= 0; 
	uint32_t force_mmap_persistent 		= 0;

	info__("Value of force_mmap = %s", force_mmap_str);
	info__("Value of force_mmap_persistent = %s", force_mmap_persistent_str);

	if (force_mmap_str != NULL) {
		force_mmap = (uint32_t)atoi(force_mmap_str);
	}

	if (force_mmap_persistent_str != NULL){
		force_mmap_persistent = (uint32_t)atoi(force_mmap_persistent_str);
	}

	if (force_mmap && force_mmap_persistent) {
		error__("Both FORCE_MMAP and FORCE_MMAP_PERSISTENT cannot be declared");
	}

	if (readlink(readlinkpath, filePath, PATH_MAX) != -1) { /* File exists */
		info__("File exists, %d -> %s", fd, filePath);
		/* Check if the file is under /mnt/pmem0/ */
		if ((starts_with(expectedPmemPath, filePath) || force_mmap_persistent) && !force_mmap) {
			info__("Using mmap_persistent for `%s'", filePath);
			return (void *)syscall(SYSCALL_ID_MMAP_PERSISTENT, start, length, prot, flags, fd, offset);
		} 

		if ((~starts_with(expectedPmemPath, filePath) || force_mmap) && !force_mmap_persistent) { /* File is in persistent memory, call mmap instead */
			info__("Using mmap(2) for `%s'", filePath);
			return (void *)syscall(SYSCALL_ID_MMAP, start, length, prot, flags, fd, offset);
		}
	} else { /* File moved since then, throw error */
		warn__("Unable to resolve path for fd: %d, file deleted or moved (?)", fd);
		return (void *)syscall(SYSCALL_ID_MMAP, start, length, prot, flags, fd, offset);
	}
	return NULL;
}

#endif // MMAP_PERSISTENT_DEFINITION_HEADER__
