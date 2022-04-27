//client

#include "sharedFunction.h"


char* transformacoesNome[]={
    "nop",
    "bcompress",
    "bdecompress",
    "gcompress",
    "gdecompress",
    "encrypt",
    "decrypt",
    NULL
};

int verificarSintax(const char** transformacoes, int nTransf){//antes de enviar para server passar por isto
    int count=0;
    for (int i = 0; i < nTransf; i++){
        for (int j = 0; j < TRANS_NR; j++){
            if (!strcmp(transformacoes[i], transformacoesNome[j])){
                count++;
                break;
            }
        }
    }
    return count;
}

void handler(int s){
    if(s==SIGTERM){
        write(STDOUT_FILENO, SERVICE_ABORTED, strlen(SERVICE_ABORTED));
    }
}

int main(int argc, char const *argv[]){
    //signal(SIGTERM, handler);

    setbuf(stdout, NULL);
    int fd, n;
    int pid=getpid();

    if(argc==1){// ./sdstore
        char* info = "./sdstore status\n./sdstore proc-file priority input-filename output-filename transformation-id-1 transformation-id-2 ...\n";
        write(STDOUT_FILENO, info, strlen(info));
    }
    else if(argc==2 && !strcmp(argv[1],"status")){// ./sdstore status
        if ((fd=open("tmp/fifoWrite", O_WRONLY))==ERROR){
            perror("error opening fifoWrite");
            return ERROR;
        }
        char* buffer = malloc(1024);//make it non static
        sprintf(buffer, "%d status\n", pid);//pid deste processo antes do comando
        write(fd, buffer, strlen(buffer));
        printf("enviei para o fifo %d a msg:%s\n", fd, buffer);
        if ((fd=open("tmp/fifoRead", O_RDONLY))==ERROR){//adicionar o pid
            perror("error opening fifoRead");
            return ERROR;
        }
        while((n = read(fd,buffer,1024)) > 0){
            write(STDOUT_FILENO,buffer,n);
        }

        close(fd);

        free(buffer);
        
        if(n==ERROR){
            perror("error reading from fifo");
            return ERROR;
        }
    }
    else if(argc>=5 && !strcmp(argv[1], "proc-file")){// ./sdstore proc-file file-in file-out transf1...
        if(verificarSintax(&(argv[4]), argc-4)==argc-4){
            printf("abri o write\n");
            if ((fd=open("tmp/fifoWrite", O_WRONLY))==ERROR){
                perror("error opening fifoWrite");
                return ERROR;
            }
            char* buffer = malloc(1024);
            strcpy(buffer, "");
            for (int i = 1; i < argc; i++){
                strcpy(buffer, concatStrings(buffer, concatStrings((char*)argv[i], " ")));
            }
            
            char* pidStr = malloc(1024);
            sprintf(pidStr, "%d ", pid);
            strcpy(buffer, concatStrings(pidStr, buffer)); //pid deste processo antes do comando
            free(pidStr);
            
            write(fd, buffer, strlen(buffer));
            close(fd);//nao sei se devemos fazer
            
            char* newFifoName = malloc(1024);
            sprintf(newFifoName, "tmp/fifoRead%d", pid);//fifo especifico para receber mensagens só neste precesso
            //sleep(5);//fazer com que este sleep desapareça
            if ((fd=open(newFifoName, O_RDONLY))==ERROR){
                perror("error opening fifoRead(unico)");
                return ERROR;
            }

            while((n = read(fd,buffer,1024)) > 0){
                write(STDOUT_FILENO,buffer,n);
            }
            
            close(fd);
            remove(newFifoName);
            
            if(buffer) free(buffer);
            if(newFifoName) free(newFifoName);
        }
        else{
            perror("syntax error in the transformations");
            return ERROR;
        }
    }
    else{
        perror("error on the input arguments");
        return ERROR;
    }

    return 1;
}
