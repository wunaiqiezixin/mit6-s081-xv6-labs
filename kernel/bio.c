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

enum { NBUCKET = 13 };

#define BUCKET_HASH(dev, blockno) ((((dev) << 27) | (blockno)) % (NBUCKET))

struct {
  struct spinlock eviction_lock;
  struct spinlock bucket_locks[NBUCKET];

  struct buf buf[NBUF];
  struct buf bufmap[NBUCKET];

} bcache;

void
binit(void)
{
  struct buf *b;

  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.bucket_locks[i], "bucket");
    bcache.bufmap[i].next = 0;
  }

  for (int i = 0; i < NBUF; i++) {
    b = &bcache.buf[i];
    initsleeplock(&bcache.buf[i].lock, "buffer");

    b->refcnt = 0;
    b->valid = 0;
    b->lastuse = 0;
    b->next = bcache.bufmap[0].next;
    bcache.bufmap[0].next = b;
  }

  initlock(&bcache.eviction_lock, "eviction");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint key = BUCKET_HASH(dev, blockno);
  // 检查哈希映射到的桶是否命中
  acquire(&bcache.bucket_locks[key]);
  for (b = bcache.bufmap[key].next; b; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.bucket_locks[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // 未命中，立即释放桶锁，避免形成死锁
  release(&bcache.bucket_locks[key]);
  // 获取"驱逐锁"，保证驱逐操作的串行性
  acquire(&bcache.eviction_lock);
  // 获取到"驱逐锁"后，再次检查哈希映射到的桶
  // 在当前进程获取锁的过程中，其它进程可能已经完成了驱逐操作
  acquire(&bcache.bucket_locks[key]);
  for (b = bcache.bufmap[key].next; b; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.bucket_locks[key]);
      release(&bcache.eviction_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_locks[key]);

  // 此时只持有"驱逐锁"，进行驱逐操作
  struct buf *before_least = 0; // LRU-buf 的前一个
  int holdingbucket = -1; // LRU-buf 所在桶的桶锁
  // 在每个桶中查找 LRU-buf
  for (int i = 0; i < NBUCKET; i++) {
    acquire(&bcache.bucket_locks[i]);
    int newfound = 0;
    for (b = &bcache.bufmap[i]; b->next; b = b->next) {
      if (b->next->refcnt == 0 &&
         (!before_least || b->next->lastuse < before_least->next->lastuse)) {
        before_least = b;
        newfound = 1;
      }
    }

    if (newfound) {
      if (holdingbucket != -1)
        release(&bcache.bucket_locks[holdingbucket]);
      holdingbucket = i;
    }
    else
      release(&bcache.bucket_locks[i]);
  }

  if (!before_least) // 没有空闲的 buf
    panic("bget: no buffers");

  b = before_least->next; // LRU-buf

  // 如果 LRU-buf 不在原哈希映射到的桶中
  // 进行驱逐操作，放入哈希映射到的桶中
  if (holdingbucket != key) {
    before_least->next = b->next;
    release(&bcache.bucket_locks[holdingbucket]);

    acquire(&bcache.bucket_locks[key]);
    b->next = bcache.bufmap[key].next;
    bcache.bufmap[key].next = b;
  }

  // 为找到的 LRU-buf 设置字段
  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  b->valid = 0;
  b->lastuse = 0;
  release(&bcache.bucket_locks[key]);
  // 释放驱逐锁
  release(&bcache.eviction_lock);

  acquiresleep(&b->lock);
  return b;
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

  releasesleep(&b->lock);

  uint key = BUCKET_HASH(b->dev, b->blockno);


  acquire(&bcache.bucket_locks[key]);
  b->refcnt--;
  if (!b->refcnt)
    b->lastuse = ticks;
  release(&bcache.bucket_locks[key]);
}

void
bpin(struct buf *b) {
  uint key = BUCKET_HASH(b->dev, b->blockno);
  acquire(&bcache.bucket_locks[key]);
  b->refcnt++;
  release(&bcache.bucket_locks[key]);
}

void
bunpin(struct buf *b) {
  uint key = BUCKET_HASH(b->dev, b->blockno);
  acquire(&bcache.bucket_locks[key]);
  b->refcnt--;
  release(&bcache.bucket_locks[key]);
}


