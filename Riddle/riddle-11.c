#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>//execl
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

//Run this program, while awaiting input run riddle and done.
int  main (){
  int fd;
  fd=open("secret_number",O_RDONLY);
  char buff[4096];
  int  s;
  scanf("%d",&s); //await any input
  read(fd,buff,1024);
  printf("%s \n",buff);
  return 0;
}
