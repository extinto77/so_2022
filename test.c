#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>

int pidServidor;
int i=0;

void handlerAlarm(int num){
    if(getpid() == pidServidor){
        printf("[PAI]->%d\n", i);
        i++;
    }
    else{
        alarm(2);
    }
}

int main(int argc, char const *argv[]){
    if(signal(SIGALRM, handlerAlarm) == SIG_ERR){
        perror("sigint error");
        return -1;
    }


    pidServidor = getpid();
    int pidAlarm;
    if(!fork()){
        alarm(2);
        while (1){
            pause();//espera que o alarme acabe
            kill(pidServidor, SIGALRM);
        }
    }
    while (1){
        wait(0);
    }
    
    


    return 0;
}
