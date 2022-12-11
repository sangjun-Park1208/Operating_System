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

#define BIT_255 255

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


// 파일 타입과 관계없이 공통으로 출력
//   파일의 inode번호, 타입, 크기, direct 블록에 저장된 내용

// 파일 타입에 따라 다르게 출력
//    CS타입인 경우 : (번호, 길이) 추가로 출력

// file struct -> ip(inode pointer) -> inode struct 필요
void fileprintinfo(struct file* f, char* fname){

  cprintf("FILE NAME: %s\n", fname);
  cprintf("INODE NUM: %d\n", f->ip->inum);

  switch (f->ip->type){
    case 1:
      cprintf("FILE TYPE: DIR\n");
      break;
    case 2:
      cprintf("FILE TYPE: FILE\n");
      break;
    case 3:
      cprintf("FILE TYPE: DEV\n");
      break;
    case 4:
      cprintf("FILE TYPE: CS\n");
      break;
  }

  cprintf("FILE SIZE: %d Bytes\n", f->ip->size);
  cprintf("DIRECT BLOCK INFO: \n");
  
  // FILE 인 경우
  if(f->ip->type == 2){
    for(int i=0; i<NDIRECT; i++) 
      cprintf("[%d] %d\n", i, f->ip->addrs[i]);
  }

  // CS 인 경우
  if(f->ip->type == 4){
    for(int i=0; i<NDIRECT; i++){
      // [idx] block (num: num, length: length)  <-- 출력
      cprintf("[%d] %d (num: %d, length: %d)\n", i, f->ip->addrs[i], f->ip->addrs[i]>>8, f->ip->addrs[i]&BIT_255); // num, length 값 수정
    }
  }
  cprintf("\n");
  
}