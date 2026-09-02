// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];  // 给每个 CPU 一个独立的空闲链表和一把锁

// 每个 CPU 持有的锁的名称(NCPU=8)
char *kmem_lock_names[] = {
  "kmem_lock_cpu0",
  "kmem_lock_cpu1",
  "kmem_lock_cpu2",
  "kmem_lock_cpu3",
  "kmem_lock_cpu4",
  "kmem_lock_cpu5",
  "kmem_lock_cpu6",
  "kmem_lock_cpu7",
};

void
kinit()
{
//  initlock(&kmem.lock, "kmem");
  // 初始化锁
  for (int i = 0; i < NCPU; i++)
    initlock(&kmem[i].lock, kmem_lock_names[i]);
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();

  int cpu = cpuid();

  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;  // 待返回的页
  struct run *rr;
/*
  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);
*/

  push_off();

  int cpu = cpuid();
  acquire(&kmem[cpu].lock);

  // 如果当前 CPU 的空闲链表为空，"窃取"
  if(!kmem[cpu].freelist){
    int steal_sz = 64;
    for (int i = 0; i < NCPU && steal_sz; i++){
      if (i == cpu)
        continue;

      acquire(&kmem[i].lock);
      if(!kmem[i].freelist){
        release(&kmem[i].lock);
        continue;
      }

      while((rr = kmem[i].freelist) && steal_sz){
        kmem[i].freelist = rr->next;
        rr->next = kmem[cpu].freelist;
        kmem[cpu].freelist = rr;
        steal_sz--;
      }
      release(&kmem[i].lock);
    }
  }

  r = kmem[cpu].freelist;

  if(r)
    kmem[cpu].freelist = r->next;

  release(&kmem[cpu].lock);

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
