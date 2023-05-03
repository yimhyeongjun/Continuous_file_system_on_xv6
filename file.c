//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

// OS5 : printinfo에서 T_CS, T_FILE 사용
#include "stat.h"
#define ONEB 255
struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if(ff.type == FD_INODE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
int
filestat(struct file *f, struct stat *st)
{
  if(f->type == FD_INODE){
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

// Read from file f.
int
fileread(struct file *f, char *addr, int n)
{
  int r;

  if(f->readable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return piperead(f->pipe, addr, n);
  if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }
  panic("fileread");
}

//PAGEBREAK!
// Write to file f.
// OS5 : write 시스템 콜 호출 시 sys_write에서 호출하는 함수 : f == fd, addr == write할 내용이 담긴 buf의 주소, n은 write할 바이트의 수
int
filewrite(struct file *f, char *addr, int n)
{
  int r;

  if(f->writable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return pipewrite(f->pipe, addr, n);
  if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * 512;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      // OS5 : addr + i의 내용을 ip에 off만큼 띄고 n1만큼 write
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    return i == n ? n : -1;
  }
  panic("filewrite");
}

// OS5 : printinfo 시스템 콜 수행부
void 
do_printinfo(struct inode* ip, const char* fname)
{
//  cprintf("printinfo!\n");
  
  cprintf("FILE NAME: %s\n", fname);
  cprintf("INODE NUM: %d\n", ip->inum);
  if(ip->type == T_FILE){
    cprintf("FILE TYPE : FILE\n");
    cprintf("FILE_SIZE : %d Bytes\n", ip->size);
    cprintf("DIRECT BLOCK INFO:\n");
    for(int i=0; i<NDIRECT; i++){
      if(ip->addrs[i] != 0)
	cprintf("[%d] %d\n",i, ip->addrs[i]);
    }
    
  }else if(ip->type == T_CS){
  
    cprintf("FILE TYPE: CS\n");
    cprintf("FILE_SIZE : %d Bytes\n", ip->size);
    cprintf("DIRECT BLOCK INFO:\n");
    for(int i=0; i<NDIRECT; i++){
      uint num, length;

      if(ip->addrs[i] != 0){
	num = ip->addrs[i]>>8;
	length = (ip->addrs[i] & ONEB);
	cprintf("[%d] %d (num: %d, length: %d)\n", i, ip->addrs[i], num, length);
      
      }
    }
  }
 cprintf("\n");
}
