#include "types.h"
#include "stat.h"
#include "user.h"

char buf[1];

void cat(int cnt, int fd) {
  int n;
  int k=0;
  while((n = read(fd, buf, 1)) > 0){
    write(1, buf, 1);
    if(buf[0] == '\n') k++;
    if(k==cnt) break;
  }
  return;
}

int main(int argc, char *argv[]) {
  int cnt, fd;
  
  if(argc <= 2) exit();
  cnt = atoi(argv[1]);

  if((fd = open(argv[2], 0)) < 0){
    printf(1, "hcat: cannot open %s\n", argv[2]);
    exit();
  }
  cat(cnt, fd);
  close(fd);
  exit();

}
