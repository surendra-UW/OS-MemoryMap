#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "memlayout.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "mmap.h"

int check_for_add(uint add, struct proc *p, int offset);
int find_addr(struct proc *p, int length);
void right_shift(struct proc *p, int index);
int mappages(pde_t *, void *, uint, uint, int);
void* memmove(void*, const void*, uint);
int read_from_file(struct file *f, char* va, int offset, int length);
int map_page_to_mapgrowsup(int index);
void unmap_for_exit();

int mmap(void *addr, int length, int prot, int flags, struct file* f, int offset)
{

    struct proc *p = myproc();
    int index;
    if(p->active_vm_maps == 32) return -1;
    
    if (flags & MAP_FIXED)
    {
        if((uint)addr < MMAP_BASE) return -1;
        if(addr == (void *)0 || (uint)addr >= KERNBASE) {
            return -1;
        }
        uint rounded_addr = (uint) PGROUNDUP((uint)addr);
        if(rounded_addr + PGROUNDUP(length) >= KERNBASE) {
             return -1;
        }
        
        index = check_for_add(rounded_addr, p, length);
        if(index == -1) return -1;
        p->vm_mappings[index].addr = rounded_addr;
        p->vm_mappings[index].length = length;
    }
    else
    {
        index = find_addr(p,length);
        if(index < 0) return -1;
    }
    char* page = kalloc();
    
    if(!page) {
        return -1;
    }
    memset(page, 0, PGSIZE);
    int ret = mappages(p->pgdir, (void *) p->vm_mappings[index].addr, p->vm_mappings[index].length, V2P(page), prot|PTE_U);
    if(ret < 0) return -1;

    if(!(flags & MAP_ANONYMOUS))
    {
        // filedup(p->ofile[fd]);
        if(read_from_file(f,(char *)p->vm_mappings[index].addr,offset,length)<0)
        return -1;
    }
     p->vm_mappings[index].valid = 1;
     p->vm_mappings[index].f = f;
    p->vm_mappings[index].flags = flags;
    p->vm_mappings[index].prot = prot;
    p->vm_mappings[index].offset = offset;
    p->active_vm_maps++;
    return (int)p->vm_mappings[index].addr;
}

int read_from_file(struct file *f, char* va, int offset, int length) {
  ilock(f->ip);
  int n = readi(f->ip, va, offset, length);
  iunlock(f->ip);
  return n;
} 

int find_addr(struct proc *p, int length)
{
   
    int i;
    uint start_add = MMAP_BASE;
    uint end_add = 0;
    for(i=0;i<p->active_vm_maps;i++) {
        end_add = PGROUNDUP(p->vm_mappings[i].addr);
        if(end_add - start_add > length) {
            break;
        }
        start_add = PGROUNDUP(p->vm_mappings[i].addr + p->vm_mappings[i].length);
    }
    if(i==p->active_vm_maps)
    {
        if(start_add+length>KERNBASE)
        return -1;
    }
    right_shift(p,i);
    p->vm_mappings[i].addr = start_add;
    p->vm_mappings[i].length = length;
    return i;
    
}


int check_for_add(uint addr, struct proc *p, int length) {
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
    right_shift(p,i);
    return i;
}
void right_shift(struct proc *p, int index)
{
    int j = p->active_vm_maps-1;
    while(j>=index) { 
        memmove(&p->vm_mappings[j+1], &p->vm_mappings[j], sizeof(p->vm_mappings[j]));
        j--;
    }
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
    if(!(p->vm_mappings[i].flags & MAP_ANONYMOUS)&&p->vm_mappings[i].flags&MAP_SHARED)
    {
        // cprintf("i:%d\n",i);
        // cprintf("\tBuffer to write: %s\n",p->vm_mappings[i].addr);
        // cprintf("addr:%d\n",p->vm_mappings[i].addr);
        // cprintf("offset:%d\n",p->vm_mappings[i].offset);
        // cprintf("length:%d\n",p->vm_mappings[i].length);
        if (filewrite(p->vm_mappings[i].f, (char *)p->vm_mappings[i].addr,p->vm_mappings[i].length) < 0) {
        return -1;
        }
        // ilock(p->vm_mappings[i].f->ip);
        // int n = writei(p->vm_mappings[i].f->ip, (char *)p->vm_mappings[i].addr, p->vm_mappings[i].offset,p->vm_mappings[i].length);
        // iunlock(p->vm_mappings[i].f->ip);
        // if(n<0)
        // return -1;
    
    }
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

int map_page_to_mapgrowsup(int index)
{
    struct proc *p = myproc();
    char *page = kalloc();
    if (!page)
    {
        return -1;
    }
    memset(page, 0, PGSIZE);
    int start_add = p->vm_mappings[index].addr + PGROUNDUP(p->vm_mappings[index].length);
    int ret = mappages(p->pgdir, (void *)start_add, PGSIZE, V2P(page), p->vm_mappings[index].prot | PTE_U);
    if (ret < 0)
        return -1;
    p->vm_mappings[index].length = p->vm_mappings[index].length + PGSIZE;
    return 0;
}

void unmap_for_exit() {
    struct proc *p = myproc();
    for(int i=0;i<p->active_vm_maps;i++) {
        if(p->vm_mappings[i].flags & MAP_SHARED && p->vm_mappings[i].reference == 1) continue;
        munmap((void *)p->vm_mappings[i].addr, p->vm_mappings[i].length);
    }
}

int copy_vmmaps_fork(struct proc *np)
{
    struct proc *p = myproc();
    for (int i = 0; i < p->active_vm_maps; i++)
    {
        cprintf("succefully copied\n");
        np->active_vm_maps = p->active_vm_maps;
        np->vm_mappings[i].addr = p->vm_mappings[i].addr;
        np->vm_mappings[i].length = p->vm_mappings[i].length;
        np->vm_mappings[i].prot = p->vm_mappings[i].prot;
        np->vm_mappings[i].flags = p->vm_mappings[i].flags;
        // memmove(&np->vm_mappings[i], &p->vm_mappings[i], sizeof(p->vm_mappings[i]));
        cprintf("total mappings %d\n", np->active_vm_maps);

        if (np->vm_mappings[i].flags & MAP_PRIVATE)
        {
            char *page = kalloc();

            if (!page)
            {
                return -1;
            }

            memset(page, 0, PGSIZE);
            return mappages(np->pgdir, (void *)np->vm_mappings[i].addr, np->vm_mappings[i].length, V2P(page), np->vm_mappings[i].prot | PTE_U);
        }
        else if (np->vm_mappings[i].flags & MAP_SHARED)
        {
            int start;
            int end_add = np->vm_mappings[i].addr + PGROUNDUP(np->vm_mappings[i].length);
            for (start = np->vm_mappings[i].addr; start < end_add; start += PGSIZE)
            {
                pte_t *pte = walkpgdir(p->pgdir, (char *)(start), 0);
                uint pa = PTE_ADDR(*pte);
                // char *v = P2V(pa);
                if(mappages(np->pgdir, (void *) np->vm_mappings[i].addr, PGSIZE, pa, np->vm_mappings[i].prot | PTE_U) < 0) {
                    return -1;
                }
            }
            np->vm_mappings[i].reference = 1;
        }
    }
    return 0;
}