//server

//0->stdin
//1->stdout

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

char* transformations_folder;

//criar um com nomes das transformacoes?? torna codigo mais melhor bom??
int config[TRANS_NR];//nr maximos de cada tipo de filtros que podem executar ao mesmo tempo
int using[TRANS_NR];//nr de filtros de cada tipo que estao a executar no momento presente

int pendingRequestsIdx[100];//pedidos em espera
int nrpendingRequests;

int runningRequestsIdx[100];//pedidos a correr
int nrrunningRequests;

char* tasks[777];//todos os pedidos enviados para o servidor
//int tasksNr = nrpendingRequests + nrrunningRequests;

int readConfigFile(char *path){
    //printf("%s\n",path);
    FILE * fp = fopen(path, "r");
    if (fp == NULL){
        perror("error reading config file");
        return ERROR;
    }
    else{
        char * line = malloc(1024);
        size_t size = 0;
        ssize_t read;
        char* string = malloc(1024);
        int nr;
        while ((read = getline(&line, &size, fp)) > 0) {
            read = sscanf(line, "%s %d" , string, &nr);
            if(read != 2){
                perror("erro de sintaxe no ficheiro config");
                return ERROR;
            }

            for (int i = 0; i < TRANS_NR; i++){
                if (!strcmp(string, transformacoesNome[i])){
                    config[i]=nr;
                    break;
                }
            }
        }
        fclose(fp);
        if (line) free(line);
        if (string) free(string);
    }
    return OK;
}

int aplicarTransformacoes(char** transformacoes, int nTransformacoes){//assumindo que o redirecionamento já está feito antes...
    int i, p[2];

    for (i=0; i < nTransformacoes-1; i++){
        int res = pipe(p);
        if(res==ERROR){
            perror("error creating pipe");
            return ERROR;
        }
        pid_t pid;

        if((pid = fork())==0){
            dup2(p[1],1);
            close(p[1]);
            close(p[0]);    

            if(execl(concatStrings(transformations_folder,transformacoes[i]),transformacoes[i],NULL) == ERROR){
                perror("Erro a aplicar filtro");
                _exit(ERROR);
            }
        }
        else{
            dup2(p[0],0);
            close(p[0]);
            close(p[1]);
        }
    }
    if(execl(concatStrings(transformations_folder,transformacoes[i]),transformacoes[i],NULL) == ERROR){
        perror("Erro a aplicar filtro");
        return(ERROR);
    }
    return OK;//nunca chega aqui??
}

int addPending(int id){
    pendingRequestsIdx[nrpendingRequests] = id;
    nrpendingRequests++;
    return nrpendingRequests-1;
}

int addRunning(int id){
    runningRequestsIdx[nrrunningRequests] = id;
    nrpendingRequests++;
    return nrrunningRequests-1;
}

int addTask(char* str){
    int position = nrpendingRequests+nrrunningRequests;
    tasks[position] = str;
    return position;
}

void removePending(int index){
    nrpendingRequests--;
    for (int i = index; i < nrpendingRequests; i++){
        pendingRequestsIdx[i] = pendingRequestsIdx[i+1];
    }
    pendingRequestsIdx[nrpendingRequests]=-1;
}

void removeRunning(int index){
    nrrunningRequests--;
    for (int i = index; i < nrrunningRequests; i++){
        runningRequestsIdx[i] = runningRequestsIdx[i+1];
    }
    runningRequestsIdx[nrrunningRequests]=-1;
}

void showSatus(int fifo){//ver melhor o input
    char* buf = malloc(1024);
    buf="---RUNING REQUESTS---";
    write(fifo, buf, strlen(buf));
    for(int i=0;i<nrrunningRequests;i++){
        int idx = runningRequestsIdx[i];
        sprintf(buf,"task #%d: (pid)%s\n", idx, tasks[idx]);//imprime as que estao a correr no momento
        write(fifo, buf, strlen(buf));
    }

    buf="\n---PENDING REQUESTS---";
    write(fifo, buf, strlen(buf));
    for(int i=0;i<nrpendingRequests;i++){
        int idx = pendingRequestsIdx[i];
        sprintf(buf,"pending #%d: (pid)%s\n", idx, tasks[idx]);//imprime as que estao a correr no momento
        write(fifo, buf, strlen(buf));
    }

    buf="\n---SERVER CONFIGURATIONS---";
    write(fifo, buf, strlen(buf));
    for(int i=0;i<TRANS_NR;i++){
        write(fifo, "transf ",8);
        sprintf(buf,"%s: %d/%d (running/max)\n", transformacoesNome[i], using[i], config[i]);//estado de ocupacao de cada uma das transformacoes
        write(fifo, buf, strlen(buf));
    }
    if(buf)
        free(buf);
}

void showPendent(int fifo){//ver melhor o input
    char* str = "pendent";
    write(fifo, str, strlen(str));
}

void showRunning(int fifo){//ver melhor o input
    char* str = "running";
    write(fifo, str, strlen(str));
}

void showConclued(int fifo){//ver melhor o input
    char* str = "pendent";
    write(fifo, str, strlen(str));
}

int goPendingOrNot(char** transformacoes, int nTransformacoes){
    int after[TRANS_NR];
    for (int i = 0; i < TRANS_NR; i++){
        using[i] = after[i];
    }
    for (int i = 0; i < nTransformacoes; i++){
        for(int j=0;j<TRANS_NR;j++){
            if(!strcmp(transformacoes[i], transformacoesNome[j])){
                after[j] += 1;
                break;
            }
        }
    }
    int exceeded = 0;
    for (int i = 0; i < 7; i++){
        if(after[i]>config[i]){
            exceeded = 1;
            break;
        }
    }
    return exceeded;
}


int main(int argc, char const *argv[]){
    setbuf(stdout, NULL);

    // -------------- validating arguments and configuring server settings -------------- 
    if(argc!=3){
        perror("invalid arguments to run the server");
        return ERROR;
    }
    int res;
    if((res = readConfigFile((char*)argv[1])) == ERROR){
        return ERROR;
    }
    transformations_folder=malloc(1024);
    strcpy(transformations_folder, (char*)argv[2]);

    // -------------- inicializations -------------- 
    nrpendingRequests=0;
    nrrunningRequests=0;
    for (int i = 0; i < 7; i++){
        using[i]=0;
    }
    for (int i = 0; i < 100; i++){
        pendingRequestsIdx[i] = -1;
        runningRequestsIdx[i] = -1;
    }
    for (int i = 0; i < 777; i++){
        tasks[i]=NULL;
    }

    remove("tmp/*");
    
    // -------------- creating fifo's -------------- 
    if((res = mkfifo("tmp/fifoWrite",0622)) == ERROR){// rw--w--w-
        perror("error creating the fifo Write(client))");
        return ERROR;
    } 
    if((res = mkfifo("tmp/fifoRead",0644)) == ERROR){// rw-r--r--
        perror("error creating the fifo Read(client))");
        return ERROR;
    }
    char* ready = "Ready to accept client requests\n";
    write(STDOUT_FILENO, ready, strlen(ready));
    // -------------- handle a cliente -------------- 
    
    int fifo, n;
    char* buf = malloc(1024);
    char* single_Request = malloc(1024);

    pid_t pidServidor = getpid();
    while (1){//manter o fifo sempre a espera de mais pedidos
        if ((fifo = open("tmp/fifoWrite",O_RDONLY,0622)) == -1){
            perror("Erro a abrir FIFO");
            return -1;
        }
        printf("vou começar while a ler do fifo %d\n", fifo);
        while((n = read(fifo,buf,1024)) > 0){//ler do cliente
            printf("li alguma coisa");
            while(buf && strcmp(buf,"")){//tratar do varios pedidos que ja estão no buffer
                // ver aquilo que o stor disse sobre o buff_ (aual azula) não escrever logo no file descritor
                printf("recebi pedido\n");
                
                char* copy = malloc(1024);
                single_Request = strsep(&buf,"\n");
                strcpy(copy, single_Request); 
                char** palavras = parse(single_Request, " ");
                int nrPals;
                int pid = atoi(palavras[0]);//para sinais
                for (nrPals = 0; palavras[nrPals]!=NULL; nrPals++)
                    ;

                char* newFifoName = malloc(1024);
                sprintf(newFifoName, "tmp/fifoRead%s",palavras[0]);//fifo especifico para enviar mensagens para processo especifico
                if ((fifo = open("tmp/fifoRead",O_WRONLY,0622)) == -1){//em vez de usar sinais tornar o fifo único para o cliente
                    perror("Erro a abrir FIFO");
                    return -1;
                }

                if(nrPals==2){//recebe pedido de status (com pid antes)
                    // ja que temos o pid podemos usar sinais??
                    showSatus(fifo);
                    close(fifo);// rever, acho que está mal
                    free(newFifoName);
                }
                else{//pid proc-file file-in file-out transf1...
                    int position = addTask(single_Request);
                    if( goPendingOrNot(&(palavras[1]), nrPals-1) ){//passar o pid à frente, fica pendente
                        int idx = addPending(position);
                        //esperar pelo sinal
                    }
                    else{
                        pid_t child_pid;
                        int status;
                        if(!fork()){// para não deixar o processo que corre o servidor à espera fazer double fork 
                            if ((child_pid=!fork())==0){
                                int idx = addRunning(position);

                                redirecionar(palavras[2], palavras[3]);
                                aplicarTransformacoes(&(palavras[4]), nrPals-4);
                                removeRunning(idx);
                            }
                            else{
                                waitpid(pid, &status, 0);

                                kill(pidServidor, SIGUSR1);//=???
                                //enviar sinal para acordar pendentes??
                            }
                        }
                    }
                }
                free(newFifoName);
            }
            close(fifo);// rever, acho que está mal
        }
    }
    



    // -------------- fazer sempre no fim -------------- 
    //existe o finally??

    if(transformations_folder) free(transformations_folder);
    return 0;
}
