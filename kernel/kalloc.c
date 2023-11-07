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

struct kmem {
  struct spinlock lock;
  struct run *freelist;
};

// lab3
struct kmem kmems[NCPU];

void
kinit()
{
  // lab3
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmems[i].lock, "kmemY");
  }
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
  // lab3
  push_off();
  int idy = cpuid();
  pop_off();
  acquire(&kmems[idy].lock);
  r->next = kmems[idy].freelist;
  kmems[idy].freelist = r;
  release(&kmems[idy].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  //lab 3
  push_off();
  int idy = cpuid();
  pop_off();
  acquire(&kmems[idy].lock);
  r = kmems[idy].freelist;
  if(r)
    kmems[idy].freelist = r->next;
  else 
    for (int i = 0; i < NCPU; i++) {
        if (i != idy) {
            acquire(&kmems[i].lock);
            r = kmems[i].freelist;
            if (r)
                kmems[i].freelist = r->next;
            release(&kmems[i].lock);
        }
        if (r)
            break;
    }
  release(&kmems[idy].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
