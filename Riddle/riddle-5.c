#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>//execl
// or run "exec 99>&1; ./riddle"
int main(){
  dup2(1,99);
  execl("./riddle","./riddle",NULL);
  return 0;
}
