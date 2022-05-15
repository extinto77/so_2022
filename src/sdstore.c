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
    int fd, n, res;
    int pid=getpid();
    
    if(argc==1){// ./sdstore
        char* info = "./sdstore status\n./sdstore proc-file priority input-filename output-filename transformation-id-1 transformation-id-2 ...\n";
        write(STDOUT_FILENO, info, strlen(info));
    }
    else if((argc==2 && !strcmp(argv[1],"status")) || (argc>=5 && !strcmp(argv[1], "proc-file"))){// ./sdstore status
        char* newFifoName = malloc(1024);
        sprintf(newFifoName, "%s%d", READ_NAME, pid);//fifo especifico para receber mensagens só neste precesso
        if((res = mkfifo(newFifoName, 0644)) == ERROR){// rw-r--r--
            perror("error creating the fifo Read(client)");
            free(newFifoName);
            return ERROR;
        }

        if ((fd=open(WRITE_NAME, O_WRONLY))==ERROR){
            perror("error opening fifoWrite");
            unlink(newFifoName);
            free(newFifoName);
            return ERROR;
        }
        Pedido req = malloc(sizeof(struct pedido));
        req->elems = argc;
        for (int i = 1; i < argc; i++){
            char* tmp = strdup(argv[i]);
            int j;
            for (j = 0; j < strlen(tmp); j++){
                req->args[i][j] = tmp[j];
                //printf("[%c|%c]", req->args[i][j], tmp[j]);
            }
            free(tmp);
            req->args[i][j] = '\0';
            printf("<%d//%s>", i, req->args[i]);
        }
        sprintf(req->args[0], "%d", pid);//ver se isto da assim
        
        printf("[Debug]PID->%d<-\n", pid);
        //printf(">%p<", req);
        write(fd, req, sizeof(struct pedido));

        if ((fd=open(newFifoName, O_RDONLY))==ERROR){
            perror("error opening fifoRead(unico)");
            unlink(newFifoName);
            free(newFifoName);
            free(req);// dar free recursivo
            //dar free's, avisar avisar servidor??
            return ERROR;
        }
        char* buffer = malloc(1024);
        while((n = read(fd,buffer,1024)) > 0){
            write(STDOUT_FILENO,buffer,n);
        }

        close(fd);
        
        if(n==ERROR){
            perror("error reading from fifo");
            return ERROR;
        }
        unlink(newFifoName);
        free(newFifoName);
        free(req);
        free(buffer);
    }
    else{
        perror("error on the input arguments");
        return ERROR;
    }

    return OK;
}
