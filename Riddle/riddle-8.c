#include <stdlib.h>
#include <stdio.h>
#include <unistd.h> //execl
#include <fcntl.h> //chmod
#include <sys/stat.h> //chmod
int main(){
  int counter=0;  //part 8
  char path[5];
  off_t hope;
  int fd;
  while(counter<=9){
    sprintf(path,"bf0%d",counter);
    fd = open(path, O_RDWR|O_CREAT,666);
      if(fd == -1){
        printf("There was a file error");
        return -1;
      }
    truncate(path,1073741824);
    hope = lseek(fd,0,SEEK_END);
    pwrite(fd,"l",1,hope);
    close(fd);
    chmod(path,777);
    counter++;
  }
  execl("./riddle","./riddle",NULL);
  return 0;
}
