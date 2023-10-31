#include "mmap.h"
#include "stddef.h"
#include "sys/types.h"


int mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    if(flags & MAP_FIXED) {
        return (int)addr;
    }
    return (int)addr;
}