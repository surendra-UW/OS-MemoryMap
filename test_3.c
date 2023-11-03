#include "types.h"
#include "user.h"
#include "stat.h"
#include "mmap.h"

int main() {
    uint addr = 0x60020000;
    int len = 4000;
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_ANON | MAP_FIXED | MAP_SHARED;
    int fd = -1;

    /* mmap anon memory */
    void *mem = mmap((void *)addr, len, prot, flags, fd, 0);
    printf(1,"Returned addr %d\n",(uint)(mem));
    printf(1,"Original addr %d\n",(uint)(addr));
    if (mem == (void *)-1) {
        printf(1,"Error in mmap\n");
    }
    if (mem != (void *)addr) {
	     printf(1,"Error in mmap\n");
    } 
     printf(1,"Mmap is correct\n");
    /* Modify something */
    char *memchar = (char*) mem;
    memchar[0] = 'a'; memchar[1] = 'a';
     printf(1,"Updated\n");
    /* Clean and return */
    int ret = munmap(mem, len);
    if (ret < 0) {
	    printf(1,"Error in munmap\n");
    }

// success:
    printf(1, "MMAP\t SUCCESS\n");
    exit();

}
