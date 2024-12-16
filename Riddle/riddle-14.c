#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

//we could not fork, we use ns_last_pid aka we can run ./riddle simply
pid_t pid;
int main(){
  int status; //part 14
  pid = fork();
  if (pid == 0){
    execl("./riddle","./riddle",NULL);
    return 0;
    //im the child
  }
  wait(&status);
  return 0;
}
