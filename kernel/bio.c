// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"


// lab3

#define NBUCKETS 13

struct {
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;

  struct buf hashbucket[NBUCKETS];
} bcache;

void
binit(void)
{
  struct buf *b;

  //initlock(&bcache.lock, "bcacheY");

  for(int i = 0; i < NBUCKETS; i++){
    initlock(&bcache.lock[i], "bcacheY");

    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];

    for(b = bcache.buf + i; b < bcache.buf+NBUF; b = b + NBUCKETS){
        b->next = bcache.hashbucket[i].next;
        b->prev = &bcache.hashbucket[i];
        initsleeplock(&b->lock, "buffer");
        bcache.hashbucket[i].next->prev = b;
        bcache.hashbucket[i].next = b;
    }
  }


  // Create linked list of buffers
  //bcache.head.prev = &bcache.head;
  //bcache.head.next = &bcache.head;
  //for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //  b->next = bcache.head.next;
  //  b->prev = &bcache.head;
  //  initsleeplock(&b->lock, "buffer");
  //  bcache.head.next->prev = b;
  //  bcache.head.next = b;
  //}
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int i = blockno % NBUCKETS;
  acquire(&bcache.lock[i]);

  // Is the block already cached?
  //for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //  if(b->dev == dev && b->blockno == blockno){
  //    b->refcnt++;
  //    release(&bcache.lock);
  //    acquiresleep(&b->lock);
  //    return b;
  //  }
  //}

  // Cached
  for(b = bcache.hashbucket[i].next; b != &bcache.hashbucket[i]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
        b->refcnt++;
        release(&bcache.lock[i]);
        acquiresleep(&b->lock);
        return b;
    }
  }


  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  //for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //  if(b->refcnt == 0) {
  //    b->dev = dev;
  //    b->blockno = blockno;
  //    b->valid = 0;
  //    b->refcnt = 1;
  //    release(&bcache.lock);
  //    acquiresleep(&b->lock);
  //    return b;
  //  }
  //}

  // Not cached
  for(b = bcache.hashbucket[i].prev; b != &bcache.hashbucket[i]; b = b->prev){
    if(b->refcnt == 0){
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache.lock[i]);
        acquiresleep(&b->lock);
        return b;
    }
  }

  // No more bcache in this hashbucket
  for(int j = NBUCKETS-1; j > 0; j--){
    if(j != i){
        acquire(&bcache.lock[j]);
        for(b = bcache.hashbucket[j].prev; b != &bcache.hashbucket[j]; b = b->prev){
            if(b->refcnt == 0){
                b->next->prev = b->prev;
                b->prev->next = b->next;
                b->next = bcache.hashbucket[i].next;
                b->prev = &bcache.hashbucket[i];
                bcache.hashbucket[i].next->prev = b;
                bcache.hashbucket[i].next = b;

                b->dev = dev;
                b->blockno = blockno;
                b->valid = 0;
                b->refcnt = 1;

                release(&bcache.lock[j]);
                release(&bcache.lock[i]);
                acquiresleep(&b->lock);
                return b;
            }
        }
        release(&bcache.lock[j]);
    }
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");


  int i = b->blockno % NBUCKETS;


  releasesleep(&b->lock);

  //acquire(&bcache.lock);
  //b->refcnt--;
  //if (b->refcnt == 0) {
  //  // no one is waiting for it.
  //  b->next->prev = b->prev;
  //  b->prev->next = b->next;
  //  b->next = bcache.head.next;
  //  b->prev = &bcache.head;
  //  bcache.head.next->prev = b;
  //  bcache.head.next = b;
  //}


  acquire(&bcache.lock[i]);

  b->refcnt--;
  if (b->refcnt == 0) {
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[i].next;
    b->prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next->prev = b;
    bcache.hashbucket[i].next = b;
  }


  //release(&bcache.lock);
  release(&bcache.lock[i]);
}

void
bpin(struct buf *b) {
  //acquire(&bcache.lock);
  int i = b->blockno % NBUCKETS;
  acquire(&bcache.lock[i]);

  b->refcnt++;
  //release(&bcache.lock);
  release(&bcache.lock[i]);
}

void
bunpin(struct buf *b) {
  //acquire(&bcache.lock);
  int i = b->blockno % NBUCKETS;
  acquire(&bcache.lock[i]);

  b->refcnt--;
  //release(&bcache.lock);
  release(&bcache.lock[i]);
}


