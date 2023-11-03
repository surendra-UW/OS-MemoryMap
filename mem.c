#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "memlayout.h"
#include "mmap.h"


// #define -1 ((void *)-1)

int check_for_add(uint add, struct proc *p, int offset);
int find_index_to_insert_mmap(uint add, struct proc *p, int length);
int mappages(pde_t *, void *, uint, uint, int);
void* memmove(void*, const void*, uint);

int mmap(void *addr, int length, int prot, int flags, int fd, int offset)
{

    struct proc *p = myproc();
    if(p->active_vm_maps == 32) return -1;
    if((uint)addr < MMAP_BASE) return -1;
    if (flags & MAP_FIXED)
    {
        if(addr == (void *)0 || (uint)addr >= KERNBASE) {
            return -1;
        }
        uint rounded_addr = (uint) PGROUNDUP((uint)addr);
        if(rounded_addr + PGROUNDUP(length) >= KERNBASE) {
             return -1;
        }
        
        int index = check_for_add(rounded_addr, p, length);
        if(index == -1) return -1;
        p->vm_mappings[index].addr = rounded_addr;
        p->vm_mappings[index].length = length;
        p->vm_mappings[index].valid = 1;
        p->vm_mappings[index].flags = flags;
        p->vm_mappings[index].prot = prot;
        p->active_vm_maps++;
        
        char* page = kalloc();
        //kernel cannot allocate page
        if(!page) {
            return -1;
        }
        memset(page, 0, PGSIZE);
        int ret = mappages(p->pgdir, (void *) rounded_addr, length, V2P(page), prot|PTE_U);
        if(ret < 0) return -1;
        return (int)rounded_addr;
    }
    else
    {
        
    }
    return (int)addr;
}

int check_for_add(uint addr, struct proc *p, int length) {
    int index = find_index_to_insert_mmap(addr, p, length);
    if(index == -1) return -1;
    int j = p->active_vm_maps-1;
    while(j>=index) { 
        memmove(&p->vm_mappings[j+1], &p->vm_mappings[j], sizeof(p->vm_mappings[j]));
        j--;
    }
    return index;
}

int find_index_to_insert_mmap( uint addr, struct proc *p, int length) {
    uint start_add = MMAP_BASE;
    uint end_add = 0;
    int i;
    for(i=0;i<p->active_vm_maps;i++) {
        end_add = PGROUNDUP(p->vm_mappings[i].addr);
        if(start_add < addr && addr < end_add) {
            if(addr + PGROUNDUP(length) > end_add) return -1;
            break;
        }
        start_add = PGROUNDUP(p->vm_mappings[i].addr + p->vm_mappings[i].length);
    }
    return i;
}


int munmap(void *addr, int length)
{
    uint rounded_addr = (uint) PGROUNDUP((uint)addr);
    uint rounded_len = (uint) PGROUNDUP((uint)length);
    int i = 0;
    struct proc *p = myproc();
    for(;i<p->active_vm_maps;i++)
    {
        if(p->vm_mappings[i].addr==rounded_addr)
        {
            break;
        }
    }
    if(i==p->active_vm_maps)
    return -1;
    int j=0;
    for(;j<rounded_len;j+=PGSIZE)
    {
        pte_t *pte = walkpgdir(p->pgdir, (char *)(rounded_addr + j), 0);
        uint pa = PTE_ADDR(*pte);
        char *v = P2V(pa);
        kfree(v);
    }

    while (i < p->active_vm_maps) {
      memmove(&p->vm_mappings[i], &p->vm_mappings[i+1], sizeof(p->vm_mappings[i]));
      i++;
    }
    p->active_vm_maps--;
    return 0;


}


