// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
static void itrunc(struct inode*);
// there should be one superblock per disk device, but we run with
// only one device
struct superblock sb; 

// Read the super block.
// 슈퍼블록 읽기 (sb가 가리키는 곳이 슈퍼블록이 됨)
void
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;

  bp = bread(dev, 1); // 버퍼 읽어서 bp에 저장
  memmove(sb, bp->data, sizeof(*sb)); // Super block 크기만큼 읽어서 sb가 가리키는 곳에 옮김
  brelse(bp);
}


// Zero a block.
// 블록 없애기 == 블록의 데이터 0으로 만들고 release
static void
bzero(int dev, int bno)
{
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}

// Blocks.

// Allocate a zeroed disk block.
// 블록 새롭게 할당
static uint
balloc(uint dev)
{
  int b, bi, m;
  struct buf *bp;

  bp = 0;

  // bmap 검사 -> 0인 경우 비트 연산으로 사용 중인 블록으로 바꿈.
  for(b = 0; b < sb.size; b += BPB){ // BPB : Bitmap bit Per Block : 한 블록의 비트 수 
    bp = bread(dev, BBLOCK(b, sb)); // b가 몇 번째 블록인지, 그 주소를 bp가 가리키도록
    for(bi = 0; bi < BPB && b + bi < sb.size; bi++){ // 한 블록에 대해(bi<BPB인 동안), b+bi가 블록 사이즈를 넘지 않는 동안 
      m = 1 << (bi % 8); // 비트 단위이므로, 바이트 수준에서 하나의 블록이 비었는지 검사하기 위한 m값
      if((bp->data[bi/8] & m) == 0){  // Is block free?  // 사용 중이지 않은 data block이라면
        bp->data[bi/8] |= m;  // Mark block in use. // 비트 채워 넣음(== 사용 중인 블록으로 새롭게 마킹)
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  // 여기까지 왔다면 모든 블록이 사용 중인 상태. 빈 블록 없음.
  panic("balloc: 빈 블록 없음\n");
}

// Free a disk block.
// 디스크 블록 해제
static void
bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;

  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8); // balloc과 같은 코드 : 1바이트에 대한 마스킹 비트
  if((bp->data[bi/8] & m) == 0) // 마스킹 연산한 비트가 0임 == 이미 비워져 있는 데이터 부분임
    panic("freeing free block");
  bp->data[bi/8] &= ~m; // Not bit 연산을 통해 1 -> 0으로 변환
  log_write(bp);
  brelse(bp);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at
// sb.startinode. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a cache of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The cached
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->valid.
//
// An inode and its in-memory representation go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, and iput() frees if
//   the reference and link counts have fallen to zero.
//
// * Referencing in cache: an entry in the inode cache
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() finds or
//   creates a cache entry and increments its ref; iput()
//   decrements ref.
//
// * Valid: the information (type, size, &c) in an inode
//   cache entry is only correct when ip->valid is 1.
//   ilock() reads the inode from
//   the disk and sets ip->valid, while iput() clears
//   ip->valid if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays cached and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.
//
// The icache.lock spin-lock protects the allocation of icache
// entries. Since ip->ref indicates whether an entry is free,
// and ip->dev and ip->inum indicate which i-node an entry
// holds, one must hold icache.lock while using any of those fields.
//
// An ip->lock sleep-lock protects all ip-> fields other than ref,
// dev, and inum.  One must hold ip->lock in order to
// read or write that inode's ip->valid, ip->size, ip->type, &c.

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} icache;

void
iinit(int dev)
{
  int i = 0;
  
  initlock(&icache.lock, "icache");
  for(i = 0; i < NINODE; i++) {
    initsleeplock(&icache.inode[i].lock, "inode");
  }

  readsb(dev, &sb);
  cprintf("sb: size %d nblocks %d ninodes %d nlog %d logstart %d\
 inodestart %d bmap start %d\n", sb.size, sb.nblocks,
          sb.ninodes, sb.nlog, sb.logstart, sb.inodestart,
          sb.bmapstart);
}

static struct inode* iget(uint dev, uint inum);

//PAGEBREAK!
// Allocate an inode on device dev.
// Mark it as allocated by  giving it type type.
// Returns an unlocked but allocated and referenced inode.
// inode를 새롭게 할당 <- create 시스템콜(open 시스템콜에서 호출됨) 에서 호출
struct inode*
ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;

  for(inum = 1; inum < sb.ninodes; inum++){ // 슈퍼블록에 기록된 inode개수만큼 inode 번호 순회
    bp = bread(dev, IBLOCK(inum, sb)); // 블록 정보 읽어서

    // dip = 데이터 블록의 주소 + 해당 inum이 거기서 얼만큼 떨어져 있는지
    // -> dip = inode가 가리키는 주소
    dip = (struct dinode*)bp->data + inum%IPB; // IPB : Inode Per Block
    if(dip->type == 0){  // a free inode : 비어 있는 inode인 경우
      memset(dip, 0, sizeof(*dip)); // 초기화
      dip->type = type; // 타입 할당 
      log_write(bp);   // mark it allocated on the disk
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  // 여기까지 왔으면 비어 있는 inode가 없다는 의미
  panic("ialloc: no inodes");
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk, since i-node cache is write-through.
// Caller must hold ip->lock.
void
iupdate(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode*)bp->data + ip->inum%IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log_write(bp);
  brelse(bp);
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
// inode 찾기
static struct inode*
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;

  acquire(&icache.lock);

  // Is the inode already cached?
  empty = 0;
  
  // inode cache 순회
  for(ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){ // 찾으면 해당 inode pointer 리턴
      ip->ref++; // LRU 기반 -> referenct count 증가
      release(&icache.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode cache entry.
  if(empty == 0)
    panic("iget: no inodes");

  ip = empty; // 비어 있다면, 빈 곳을 inode pointer가 가리키게 함
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0; // 이제 비어있지 않음

  release(&icache.lock);

  return ip;  
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode*
idup(struct inode *ip)
{
  acquire(&icache.lock);
  ip->ref++;
  release(&icache.lock);
  return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void
ilock(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  if(ip == 0 || ip->ref < 1)
    panic("ilock");

  acquiresleep(&ip->lock);

  if(ip->valid == 0){
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum%IPB;
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;
    if(ip->type == 0)
      panic("ilock: no type");
  }
}

// Unlock the given inode.
void
iunlock(struct inode *ip)
{
  if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void
iput(struct inode *ip)
{
  acquiresleep(&ip->lock);
  if(ip->valid && ip->nlink == 0){
    acquire(&icache.lock);
    int r = ip->ref;
    release(&icache.lock);
    if(r == 1){
      // inode has no links and no other references: truncate and free.
      itrunc(ip);
      ip->type = 0;
      iupdate(ip);
      ip->valid = 0;
    }
  }
  releasesleep(&ip->lock);

  acquire(&icache.lock);
  ip->ref--;
  release(&icache.lock);
}

// Common idiom: unlock, then put.
void
iunlockput(struct inode *ip)
{
  iunlock(ip);
  iput(ip);
}

//PAGEBREAK!
// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
// readi()와 writei() 함수에서 호출됨
// n번째 블록에 있는 inode의 pointer를 리턴 (blocknum에 해당하는 ip) 
static uint
bmap(struct inode *ip, uint bn) // 현재 파일 상의 block number를 인자로 받음 -> 디스크 상의 블록 넘버로 리턴해주는 함수
{ //  bn = offset / BSIZE (블록 하나의 크기인 512를 기준, 몇 개의 블록을 차지하는지)
  uint addr = 0;
  uint *a;
  struct buf *bp;

  // 기존 파일 시스템
  if(ip->type != T_CS) {
    if (bn < NDIRECT) {
      if ((addr = ip->addrs[bn]) == 0)          // 빈 공간 찾아서
        ip->addrs[bn] = addr = balloc(ip->dev); // 새로운 블록 할당 
      return addr;                              // 그리고 리턴
    }
    bn -= NDIRECT;
    // bn값에서 NDIRECT를 뺐을 때 양수 -> bn이 INDIRECT에 속함

    // 간접 포인터 연산
    if (bn < NINDIRECT) {
      // Load indirect block, allocating if necessary.
      if ((addr = ip->addrs[NDIRECT]) == 0)
        ip->addrs[NDIRECT] = addr = balloc(ip->dev);
      bp = bread(ip->dev, addr);
      a = (uint *)bp->data;
      if ((addr = a[bn]) == 0) {
        a[bn] = addr = balloc(ip->dev);
        log_write(bp);
      }
      brelse(bp);
      return addr;
    }
    // bn에서 NDIRECT를 뺐음에도 NINDIRECT를 초과한 경우 -> bmap의 범위 초과
    panic("bmap: out of range");
  }

  // CS 파일 시스템
  if(ip->type == T_CS) {
    int cumulative_block_sum = 0; // inode에 있는 연속 블록 수 누적합
    uint NUM_3byte, LEN_1byte; // 상위 3바이트(번호), 하위 1바이트 (길이)
    // ex) (600, 5) -> (1200, 10) -> (16450, 22) : cumulative_block_num = 37

    // 전체 direct inode block 개수만큼 돌면서, 하위 8bit masking(길이 추출)
    for(int i=0; i<NDIRECT; i++) {
      if(ip->addrs[i] == 0) continue;
      LEN_1byte = (ip->addrs[i] & 255); // 하위 8bit -> 비트 마스킹
      cumulative_block_sum += LEN_1byte;
    }

    // 찾고자 하는 bn이 inode table에 등록된 블록 개수보다 큰 경우 : 새 블록 할당
    //cprintf("bn: %d, c_block_sum: %d\n", bn, cumulative_block_sum);
    if(cumulative_block_sum <= bn){
      addr = balloc(ip->dev); // addr : 32bit 기존 파일시스템 블록넘버
      if(addr == 0) return 0;
  
      for(int i=0; i< NDIRECT; i++){
        if(ip->addrs[i] == 0) continue;
        if(ip->addrs[i] == 1) ip->addrs[i] = 0;

        NUM_3byte = ip->addrs[i] >> 8;
        LEN_1byte = ip->addrs[i] & 255;

        if(LEN_1byte == 255 && ip->addrs[i+1] == 0) {
          ip->addrs[i+1]=1;
          return addr;
        }
        if(NUM_3byte + LEN_1byte == addr) {
          ip->addrs[i]++;
          return addr;
        }
      }
      for(int i=0; i<NDIRECT; i++){
        if(ip->addrs[i] == 0){
          ip->addrs[i] = (addr<<8) + 1;
          return addr;
        }
      }
      return 0;
    } 
    else {
      // 새 블록을 할당하지 않는 경우
      for(int i=0; i<NDIRECT; i++){
        if(ip->addrs[i] == 0) continue;
        cumulative_block_sum -= (ip->addrs[i] & 255);
        if(cumulative_block_sum <= 0) {
          return (ip->addrs[i]>>8) + bn;
        }
        bn = cumulative_block_sum;
      }
    }
    return 0;
  }
  panic("!!!!\n");
}

// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).
static void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;
  if(ip->type != T_CS){
    for(i = 0; i < NDIRECT; i++){
      if(ip->addrs[i]){
        bfree(ip->dev, ip->addrs[i]);
        ip->addrs[i] = 0;
      }
    }

    if(ip->addrs[NDIRECT]){
      bp = bread(ip->dev, ip->addrs[NDIRECT]);
      a = (uint*)bp->data;
      for(j = 0; j < NINDIRECT; j++){
        if(a[j])
          bfree(ip->dev, a[j]);
      }
      brelse(bp);
      bfree(ip->dev, ip->addrs[NDIRECT]);
      ip->addrs[NDIRECT] = 0;
    }
  }
  
  if(ip->type == T_CS){
    int LEN_1byte;
    for(i=0; i<NDIRECT; i++){
      if(ip->addrs[i] == 0) continue;

      LEN_1byte =(ip->addrs[i] & 255);
      for(j=0; j<LEN_1byte; j++)
        bfree(ip->dev, (ip->addrs[i]>>8) + j);
      
      ip->addrs[i] = 0;
    }
  }

  ip->size = 0;
  iupdate(ip);
}

// Copy stat information from inode.
// Caller must hold ip->lock.
void
stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

//PAGEBREAK!
// Read data from inode.
// Caller must hold ip->lock.
int
readi(struct inode *ip, char *dst, uint off, uint n)
{
  // cprintf("readi 진입: ip->inum : %d\n", ip->inum);
  uint tot, m;
  struct buf *bp;

  if(ip->type == T_DEV){
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read)
      return -1;
    return devsw[ip->major].read(ip, dst, n);
  }

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    bp = bread(ip->dev, bmap(ip, off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(dst, bp->data + off%BSIZE, m);
    brelse(bp);
  }
  return n;
}

// PAGEBREAK!
// Write data to inode.
// Caller must hold ip->lock.
int
writei(struct inode *ip, char *src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;
  uint blockNum;

  if(ip->type == T_DEV){
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].write)
      return -1;

    return devsw[ip->major].write(ip, src, n);
  }

  if(off > ip->size || off + n < off)
    return -1;

  // CS 파일시스템인 경우, 초과되기 전까지는 데이터 씀 (<-> 기존 파일시스템은 범위 넘어가면 안 쓰고 에러처리)
  // if(ip->type != T_CS && off + n > MAXFILE*BSIZE) // CS인 경우 종료되지 않도록
  //   return -1;
  if(off + n > MAXFILE*BSIZE && ip->type!=T_CS)
    return -1;

  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    blockNum = bmap(ip, off/BSIZE);

    if(blockNum == 0)
      return -1;
    
    bp = bread(ip->dev, blockNum);
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(bp->data + off%BSIZE, src, m);
    log_write(bp);
    brelse(bp);
  }

  if(n > 0 && off > ip->size){
    ip->size = off;
    iupdate(ip);
  }
  return n;
}

//PAGEBREAK!
// Directories

int
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode*
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  if(dp->type != T_DIR)
    panic("dirlookup not DIR");

  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    if(de.inum == 0)
      continue;
    if(namecmp(name, de.name) == 0){
      // entry matches path element
      if(poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int
dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.
  if((ip = dirlookup(dp, name, 0)) != 0){
    iput(ip);
    return -1;
  }

  // Look for an empty dirent.
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("dirlink");

  return 0;
}

//PAGEBREAK!
// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode*
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  if(*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(myproc()->cwd);

  while((path = skipelem(path, name)) != 0){
    ilock(ip);
    if(ip->type != T_DIR){
      iunlockput(ip);
      return 0;
    }
    if(nameiparent && *path == '\0'){
      // Stop one level early.
      iunlock(ip);
      return ip;
    }
    if((next = dirlookup(ip, name, 0)) == 0){
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if(nameiparent){
    iput(ip);
    return 0;
  }
  return ip;
}

struct inode*
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}

struct inode*
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}
