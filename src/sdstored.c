//server

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

int config[TRANS_NR];//nr maximos de cada tipo de filtros que podem executar ao mesmo tempo
int using[TRANS_NR];//nr de filtros de cada tipo que estao a executar no momento presente

int pendingRequestsIdx[100];//pedidos em espera
int nrpendingRequests;//numero de pedidos em espera

int runningRequestsIdx[100];//pedidos a correr
int nrrunningRequests;//numero de pedidos a correr

int waiting[100][2];// pid | idxTask
int nrwaitingProcess;

Pedido tasks[777];//todos os pedidos enviados para o servidor
int tasksNr;

pid_t pidServidor;

void deleteFolderContent(char* dir){//apagar o conteudo de uma pasta, para nao estar sempre a fzaer isto na shell
    DIR *theFolder = opendir(dir);
    struct dirent *next_file;
    char* filepath = malloc(1024);

    while ( (next_file = readdir(theFolder)) != NULL ){
        sprintf(filepath, "%s/%s", dir, next_file->d_name);
        unlink(filepath);
        //remove(filepath);
    }
    free(filepath);
    closedir(theFolder);
}

int readConfigFile(char *path){// só lê 1024... se for maior ardeu FALTA TESTAR
    int fd;
    if((fd = open(path,O_RDONLY)) == ERROR){
        perror("error opening config file");
        return ERROR;
    }
    else{
        char* line = malloc(1024);
        int bytes_read;

        if((bytes_read=read(fd,line,1024))==ERROR){
            perror("reading config file");
            if (line) free(line);
            return ERROR; 
        }
        char* string = malloc(1024);
        int nr, i, lineNr=0;
        for (i = 0; i < bytes_read; i++){
            if(line[i]=='\n')
                lineNr++;
        }
        for(i=0;i<lineNr;i++){ 
            strcpy(string, strsep(&line, "\n"));
            char*temp=malloc(1024);
            int n = sscanf(string, "%s %d" , temp, &nr);
            if(n != 2){
                perror("erro de sintaxe no ficheiro config");
                if (line) free(line);
                if (string) free(string);
                //
                continue;
            }
            
            for (int i = 0; i < TRANS_NR; i++){
                if (!strcmp(string, transformacoesNome[i])){
                    config[i]=nr;
                    printf("--%s->%d\n", transformacoesNome[i], nr);
                    break;
                }
            }
            if(temp)
                free(temp);
        }
        close(fd);
        if (string) free(string);
    }
    return OK;
}

int aplicarTransformacoes(Pedido req){//assumindo que o redirecionamento já está feito antes...
    int i, p[2], stat;
    //printf("transNr: %d\n", nTransformacoes);

    for (i=4; i < req->elems-1; i++){
        int res = pipe(p);
        if(res==ERROR){
            perror("error creating pipe");
            return ERROR;
        }
        pid_t pid;

        char* tmp = strdup(req->args[i]);
        if((pid = fork())==0){
            dup2(p[1],1);
            if(close(p[1])==ERROR){
                perror("closing fd (aplicar transformacoes)");
            }
            if( close(p[0])==ERROR){
                perror("closing fd (aplicar transformacoes)");
            }
            //printf("mais que um filtro");
            if(execl(concatStrings(concatStrings(transformations_folder, "/"),tmp),tmp,NULL) == ERROR){
                perror("Erro a aplicar filtro");
                _exit(ERROR);
            }
        }
        else{
            dup2(p[0],0);
            if(close(p[0])==ERROR){
                perror("closing fd (aplicar transformacoes)");
            }
            if(close(p[1])){
                perror("closing fd (aplicar transformacoes)");
            }
            int status;
            if(waitpid(pid, &status, WNOHANG) != 0) 
                free(tmp);//para nao dar erro no exec so dar free se ja nao estiver em uso
        }
    }
    char* tmp = strdup(req->args[i]);
    if(!fork()){
        if(execl(concatStrings(concatStrings(transformations_folder, "/"),tmp),tmp,NULL) == ERROR){
            perror("Erro a aplicar filtro");
            return(ERROR);
        }
    }
    wait(&stat);
    free(tmp);
    return OK;//nunca chega aqui??
}

int addPending(int id){
    pendingRequestsIdx[nrpendingRequests] = id;
    nrpendingRequests++;
    return nrpendingRequests-1;
}

int addRunning(int id){
    printf("<inicio>");
    //consultar tasks para ver filtros e
    // atualizar array using
    for (int i = 1; i < tasks[id]->elems; i++){
        printf("!for%d!", i);
        char* filtro = tasks[id]->args[i];
        for (int i = 0; i < TRANS_NR; i++){
            if (!strcmp(filtro, transformacoesNome[i])){
                config[i]+=1;
                break;
            }
        }
    }
    //rever daqui para cima
    runningRequestsIdx[nrrunningRequests] = id;
    nrpendingRequests++;

    return nrrunningRequests-1;
    printf("<fim>");
}

int addTask(Pedido temp){
    tasks[tasksNr] = malloc(sizeof(struct pedido));
    tasks[tasksNr] = temp;
    tasksNr++;
    return tasksNr-1;
}

void addWaiting(int pid, int idx){
    waiting[nrwaitingProcess][0] = pid;
    waiting[nrwaitingProcess][1] = idx;
    nrwaitingProcess++;
}

void removeWaiting(int pid){
    int i;
    for (i = 0; i < nrwaitingProcess; i++){
        if(waiting[i][0]==pid)
            break;
    }
    if(i==nrwaitingProcess){
        perror("invalid pid\n");
        return;
    }
    nrwaitingProcess--;
    for (int j = i; j < nrwaitingProcess; i++){
        waiting[j][0] = waiting[j+1][0];
        waiting[j][1] = waiting[j+1][1];
    }
    waiting[nrwaitingProcess][0] = -1;
    waiting[nrwaitingProcess][1] = -1;
}

void removePending(int index){
    nrpendingRequests--;
    for (int i = index; i < nrpendingRequests; i++){
        pendingRequestsIdx[i] = pendingRequestsIdx[i+1];
    }
    pendingRequestsIdx[nrpendingRequests]=-1;
}

void removeRunning(int index){
    //consultar tasks para ver filtros e
    // atualizar array using
    for (int i = 1; i < tasks[index]->elems; i++){
        char* filtro = tasks[index]->args[i];
        for (int i = 0; i < TRANS_NR; i++){
            if (!strcmp(filtro, transformacoesNome[i])){
                config[i]-=1;
                break;
            }
        }
    }
    //rever daqui para cima
    
    nrrunningRequests--;
    for (int i = index; i < nrrunningRequests; i++){
        runningRequestsIdx[i] = runningRequestsIdx[i+1];
    }
    runningRequestsIdx[nrrunningRequests]=-1;
}

void showSatus(int fifo){
    char* buf = malloc(1024);
    strcpy(buf, "---RUNING REQUESTS---\n");
    write(fifo, buf, strlen(buf));
    for(int i=0;i<nrrunningRequests;i++){
        int idx = runningRequestsIdx[i];
        char *full= malloc(1024);
        strcpy(full, "");
        for (int i = 0; i < tasks[idx]->elems; i++){
            full = concatStrings(full, concatStrings(tasks[idx]->args[i], " "));
        }
        sprintf(buf,"task #%d: (pid)%s\n", idx, full);//imprime as que estao a correr no momento
        if(full)
            free(full);
        //rever acima a ultima string
        write(fifo, buf, strlen(buf));
    }

    strcpy(buf, "\n---PENDING REQUESTS---\n");
    write(fifo, buf, strlen(buf));
    for(int i=0;i<nrpendingRequests;i++){
        int idx = pendingRequestsIdx[i];
        char *full= malloc(1024);
        strcpy(full, "");
        for (int i = 0; i < tasks[idx]->elems; i++){
            full = concatStrings(full, concatStrings(tasks[idx]->args[i], " "));
        }

        sprintf(buf,"pending #%d: (pid)%s\n", idx, full);//imprime as que estao a correr no momento
        if(full)
            free(full);
        //rever acima a ultima string
        write(fifo, buf, strlen(buf));
    }

    strcpy(buf, "\n---SERVER CONFIGURATIONS---\n");
    write(fifo, buf, strlen(buf));
    for(int i=0;i<TRANS_NR;i++){
        char* trans = "transf ";
        write(fifo, trans, strlen(trans));
        sprintf(buf,"%s: %d/%d (running/max)\n", transformacoesNome[i], using[i], config[i]);//estado de ocupacao de cada uma das transformacoes
        write(fifo, buf, strlen(buf));
    }
    if(buf)
        free(buf);
}

void showPendent(int fifo){
    write(fifo, SERVICE_PENDENT, strlen(SERVICE_PENDENT));
}

void showRunning(int fifo){
    write(fifo, SERVICE_RUNNING, strlen(SERVICE_RUNNING));
}

void showConclued(int fifo){
    write(fifo, SERVICE_FINISHED, strlen(SERVICE_FINISHED));//+1??
}

int goPendingOrNot(Pedido req){
    int after[TRANS_NR];
    for (int i = 0; i < TRANS_NR; i++){
        after[i] = using[i];
    }
    for (int i = 4; i < req->elems; i++){
        char* tmp =strdup(req->args[i]);
        printf("testfilter:<%s>\n", tmp);
        for(int j=0;j<TRANS_NR;j++){
            if(!strcmp(tmp, transformacoesNome[j])){
                after[j]++;
                break;
            }
        }
        free(tmp);
    }
    for (int i = 0; i < TRANS_NR; i++){
        if(after[i]>config[i]){
            return ERROR;
        }
    }
    return OK;
}

/*
void handler(int s){
    printf("entrou no HANDLER\n");
    if(s == SIGUSR1){
        for (int i = 0; i < nrpendingRequests; i++){
            int idxTask = pendingRequestsIdx[i];
            char* palavras[77];
            
            parse(tasks[idxTask], ' ', palavras);
            int nrPals;
            for (nrPals = 0; palavras[nrPals]!=NULL; nrPals++)
                ;

            if( goPendingOrNot(&(palavras[1]), nrPals-1) == ERROR){
                removePending(i);
                int fifo;
                char* newFifoName = malloc(1024);
                sprintf(newFifoName, "tmp/fifoRead%s",palavras[0]);//fifo especifico para enviar mensagens para processo especifico
                if ((fifo = open("tmp/fifoRead",O_WRONLY,0622)) == -1){//em vez de usar sinais tornar o fifo único para o cliente
                    perror("Erro a abrir FIFO");
                    addPending(idxTask);
                    //avisar clienete que ficheiro sofreu erro???
                    continue;
                }
                pid_t child_pid;
                int status;
                if(!fork()){// para não deixar o processo que corre o servidor à espera fazer double fork 
                    if ((child_pid=!fork())==0){
                        showRunning(fifo);
                        int idx = addRunning(idxTask);

                        redirecionar(palavras[2], palavras[3]);
                        aplicarTransformacoes(&(palavras[4]), nrPals-4);
                        removeRunning(idx);
                    }
                    else{
                        wait(&status);
                        showConclued(fifo);
                        close(fifo);//por causa de ter 2 abertos influencia?? o outro também 
                        //kill(pidServidor, SIGUSR1);//=???
                        //enviar sinal para acordar pendentes??
                    }
                }
            }
        }
        return;
    }
    else if(s==SIGTERM){
        //acabar de forma graciosa
        
        //1->esperar que todos os running acabem
        //2->notificar os pending que foi abortado
        while(nrpendingRequests>0){
            int pid = atoi(strsep(&tasks[pendingRequestsIdx[0]], " "));
            removePending(0);
            kill(pid, SIGTERM);
        }
        
        //3-> nao deixar que mais processos sejam aceites(fechar fifo chega??, ja que foi aberto 2 vezes)
    }
}
*/



int main(int argc, char const *argv[]){
    setbuf(stdout, NULL);

    /*if(signal(SIGUSR1, handler) == SIG_ERR){
        perror("SIGUSR1 failed");
    }*/

    // -------------- validating arguments and configuring server settings -------------- 
    if(argc!=3){
        perror("invalid arguments to run the server");
        return ERROR;
    }
    int res;
    if((res = readConfigFile((char*)argv[1])) == ERROR){
        return ERROR;
    }
    transformations_folder = strdup((char*)argv[2]);

    deleteFolderContent("tmp");//apagar depois

    // -------------- inicializations -------------- 
    nrpendingRequests=0;
    nrrunningRequests=0;
    tasksNr=0;
    nrwaitingProcess=0;
    for (int i = 0; i < 7; i++){
        using[i]=0;
    }
    for (int i = 0; i < 100; i++){
        pendingRequestsIdx[i] = -1;
        runningRequestsIdx[i] = -1;
        waiting[i][0] = -1;
        waiting[i][1] = -1;
    }
    
    // -------------- handle a cliente -------------- 
    
    int backbone, fifoW, fifoU, n;

    pidServidor = getpid();
    
    if((res = mkfifo(WRITE_NAME, 0622)) == ERROR){// rw-w--w--
        perror("error creating the fifo Write(client)");
        return ERROR;
    }
    //as permissoes podem mudar, para prevenir vamos acertar as permissoes
    chmod(WRITE_NAME, 0622);


    printf("[Debug]Ready to accept client requests\n");        
    if ((fifoW = open(WRITE_NAME, O_RDONLY, 0622)) == ERROR){
        perror("Erro a abrir FIFO(write)");
        return ERROR;
    }
    Pedido temporary = malloc(sizeof(struct pedido));
    if ((backbone = open(WRITE_NAME, O_WRONLY)) == ERROR){//backbone, nunca vai receber EOF na leitura
        perror("Erro a abrir FIFO(write)");
        return ERROR;
    }

    while((n = read(fifoW,temporary,sizeof(struct pedido))) > 0){//ler do cliente
        //printf("--struct-->%p---", temporary);
        Pedido infoStruct = malloc(sizeof(struct pedido));
        //infoStruct->elems = temporary->elems;
        //infoStruct->args = malloc(sizeof(char*) * infoStruct->elems);
        infoStruct = temporary;
        printf("<FOR-%d->", infoStruct->elems);
        printf("<FOR-%d->", temporary->elems);
        for (int i = 0; i < temporary->elems; i++){
            printf(">|%s|<", temporary->args[i]);//erro a aceder aqui
            //infoStruct->args[i] = malloc(strlen(temporary->args[i]));
            //printf("malloc\n");
            //strcpy(infoStruct->args[i], temporary->args[i]);
            printf("%s>\n", infoStruct->args[i]);
        }
        printf("\n");

        if(infoStruct->elems==2){//recebe pedido de status (com pid antes)
            if(!fork()){
                //------Abertura de um fifo unico para o Cliente--------
                char* newFifoName = malloc(1024);
                sprintf(newFifoName, "%s%s",READ_NAME, infoStruct->args[0]);//fifo especifico para enviar mensagens para processo especifico
                printf("--%s--\n", newFifoName);

                if ((fifoU = open(newFifoName,O_WRONLY, 0644)) == ERROR){//em vez de usar sinais tornar o fifo único para o cliente
                    perror("Erro a abrir FIFO(read)");
                    free(newFifoName);
                    continue;
                }
                //----------------------------

                printf("2palavras");
                showSatus(fifoU);
                close(fifoU);
            }
        }
        else{//pid proc-file file-in file-out transf1...
            //------Abertura de um fifo unico para o Cliente--------
            char* newFifoName = malloc(1024);
            sprintf(newFifoName, "%s%s",READ_NAME, infoStruct->args[0]);//fifo especifico para enviar mensagens para processo especifico
            printf("--%s--\n", newFifoName);
            if ((fifoU = open(newFifoName,O_WRONLY, 0644)) == ERROR){
                perror("Erro a abrir FIFO(read)");
                free(newFifoName);
                continue;
            }
            //----------------------------

            

            int position = addTask(infoStruct);//rever !!!!!
            printf("--testing pendente--\n");
            if( goPendingOrNot(infoStruct) == OK ){//passar o pid à frente, fica pendente
                printf("pendente\n");
                addPending(position);//esperar ate que o sinal SIGSUR1 faca a sua parte 
                showPendent(fifoU);
            }
            else{
                printf("nao pendente\n");
                int pid1, pid2, status1;
                int idx = addRunning(position);
                printf("vou fazer fork\n");
                if((pid1=!fork())==0){// para não deixar o processo que corre o servidor à espera fazer double fork 
                    int status2;
                    if ((pid2=!fork())==0){
                        showRunning(fifoU);

                        redirecionar(infoStruct->args[2], infoStruct->args[3]);
                        aplicarTransformacoes(infoStruct);
                        _exit(OK);
                    }
                    else{
                        waitpid(pid2, &status2, 0);
                        printf("pai: FEEEEIIIIITTTOOOOOO!!!\n");
                        showConclued(fifoU);
                        //kill(pidServidor, SIGUSR1);//=???
                        //enviar sinal para acordar pendentes?? conformar se esta bem
                    }
                    _exit(OK);
                }else{
                    if(waitpid(pid1, &status1, WNOHANG)==0)
                        addWaiting(pid1, position);
                    else
                        removeRunning(idx);
                }
            }
            close(fifoU);//como o filho do filho tem copia do descritor pode usa-la
            if(newFifoName) free(newFifoName);
            //dar free nos mallocs todos(palavras e afins)
        }
        printf("acabei 1 pedido\n");
    }
    if(n==ERROR){
        perror("error reading from fifoW");
    }
    close(fifoW);
    close(backbone);
    unlink(WRITE_NAME);//unlink nao apaga, logo. só quando todos fecham o fd.
    
    if(temporary) free(temporary);

    // na graciosa ir a todas as tasks e dar free em todas as structs, recursivamente tb

    if(transformations_folder) free(transformations_folder);
    return 0;
}
