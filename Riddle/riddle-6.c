#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>//execl
int main(){
  int p1[2],p2[2];  //part 6
  pipe(p1);
  pipe(p2);
  dup2(p1[0],33);
  dup2(p1[1],34);
  dup2(p2[0],53);
  dup2(p2[1],54);
  execl("./riddle","./riddle",NULL);
  return 0;
}
