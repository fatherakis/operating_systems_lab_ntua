#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

//run program, run riddle, type the 4-digits in script, type the letter riddle needs, done.
int  main(){
  int  fd;
  char  c[18];
  char  s[1];
  off_t   ptr;
  scanf("%s",&c);
  printf("%s",c);
  fd=openat(AT_FDCWD,("/temp/riddle-%s",c),O_RDWR);
  ftruncate(fd,111);
  ptr=lseek(fd,0,SEEK_END);
  scanf("%s",s);
  pwrite(fd,("/temp/riddle-%s",s),1,ptr);
  return  0;
}
