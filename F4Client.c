#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#define PLAYER1 "Player1"
#define PLAYER2 "Player2"


int main(){
    int fd=open(PLAYER1, O_WRONLY | O_NONBLOCK);
    if (fd==-1){
      if (errno==ENOENT){
        printf("FIFO not exist, Server not running correctly\n");
        exit(0);
      }
      else if(errno==ENXIO){
        printf("Player1 is ready!\n");
      }
      fd=open(PLAYER2, O_WRONLY | O_NONBLOCK);
      if (fd==-1){
        if (errno==ENOENT){
          printf("FIFO not exist, Server not running correctly\n");
          exit(0);
        }
        else if(errno==ENXIO){
          printf("Game already started, retry soon\n");
        }
      }
    }

    printf("Send pid to server...");
    char str_pid[8];
    pid_t pid = getpid();
    sprintf(str_pid,"%d",(int)(pid));
    printf("Pid current process: %s\n",str_pid);

    if (write(fd,str_pid, strlen(str_pid)+1) == -1){
	printf("Error write in a fifo!\n");
	exit(0);
    }

    close(fd);


    return 0;

}

