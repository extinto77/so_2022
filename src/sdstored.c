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

int pendingRequestsIdx[100][2];//pedidos em espera             idTask | fifoU
int nrpendingRequests;//numero de pedidos em espera

int runningRequestsIdx[100];//pedidos a correr
int nrrunningRequests;//numero de pedidos a correr

int waiting[100][2];// pid | idxTask
int nrwaitingProcess;

Pedido tasks[777];//todos os pedidos enviados para o servidor
int tasksNr;

pid_t pidServidor;
int backbone;
int alrm;

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
        if(bytes_read>=1024)
            perror("config file too big, some filters could not loaded");

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
                if (!strcmp(temp, transformacoesNome[i])){
                    config[i]=nr;
                    //printf("--%s->%d\n", transformacoesNome[i], nr);//apagar depois
                    break;
                }
            }
            if(temp)
                free(temp);
        }
        close(fd);
        if (string)
            free(string);
    }
    return OK;
}

int aplicarTransformacoes(Pedido req){//assumindo que o redirecionamento já está feito antes...
    int i, p[2];
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
                _exit(ERROR);
            }
            if( close(p[0])==ERROR){
                perror("closing fd (aplicar transformacoes)");
                _exit(ERROR);
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
            free(tmp);//para nao dar erro no exec so dar free se ja nao estiver em uso
        }
    }
    char* tmp = strdup(req->args[i]);
    if(execl(concatStrings(concatStrings(transformations_folder, "/"),tmp),tmp,NULL) == ERROR){
        perror("Erro a aplicar filtro");
        free(tmp);
        return(ERROR);
    }
    free(tmp);
    return OK;
}

int addPending(int idxTask, int fifoU){//adiciona ao fim da fila o idxTask e o fifoU da tarefa que fica pendente
    pendingRequestsIdx[nrpendingRequests][0] = idxTask;
    pendingRequestsIdx[nrpendingRequests][1] = fifoU;
    nrpendingRequests++;
    return nrpendingRequests-1;
}

int addRunning(int id){//adiciona ao fim da fila o idxTask que passa vai ficar a correr e atualiza array using
    for (int i = 4; i < tasks[id]->elems; i++){
        char* filtro = tasks[id]->args[i];
        for (int i = 0; i < TRANS_NR; i++){
            if (!strcmp(filtro, transformacoesNome[i])){
                using[i]+=1;
                break;
            }
        }
    }
    runningRequestsIdx[nrrunningRequests] = id;
    nrpendingRequests++;

    return nrrunningRequests-1;
}

int addTask(Pedido temp){//adiciona a struct ao fim da fila 
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

int removePending(int index){
    nrpendingRequests--;
    int result=pendingRequestsIdx[index][1];
    for (int i = index; i < nrpendingRequests; i++){
        pendingRequestsIdx[i][0] = pendingRequestsIdx[i+1][0];
        pendingRequestsIdx[i][1] = pendingRequestsIdx[i+1][1];
    }
    pendingRequestsIdx[nrpendingRequests][0]=-1;
    pendingRequestsIdx[nrpendingRequests][1]=-1;
    return result;//devolve fifo onde deve comunicar 
}

void removeRunning(int index){
    //consultar tasks para ver filtros e
    // atualizar array using
    for (int i = 4; i < tasks[index]->elems; i++){
        char* filtro = tasks[index]->args[i];
        for (int i = 0; i < TRANS_NR; i++){
            if (!strcmp(filtro, transformacoesNome[i])){
                using[i]-=1;
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
    printf("------R>%d<-------", nrrunningRequests);
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
    printf("------P>%d<-------", nrpendingRequests);

    for(int i=0;i<nrpendingRequests;i++){
        int idx = pendingRequestsIdx[i][0];
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


//-------------------- HANDLERS --------------------
void handlerGracioso(int num){
    printf("[GRACIOSA]\n");
    kill(alrm, SIGKILL);
    close(backbone);

    for (int i = 0; i < nrpendingRequests; i++){
        int fifoU = pendingRequestsIdx[i][1];
        write(fifoU, SERVICE_ABORTED, strlen(SERVICE_ABORTED));
        close(fifoU);
    }
    
    while (nrrunningRequests>0 || nrwaitingProcess>0){
        int status;
        for (int i = 0; i < nrwaitingProcess; i++){
            int pid = waiting[i][0];
            if(waitpid(pid, &status, WNOHANG)!=0){
                int idxTask = waiting[i][1];
                removeRunning(idxTask);
                removeWaiting(idxTask);
                i--;
            }
        }
        //running mas ainda nao tinha criado o fork?? resolver??
    }
}

void handlerDependences(int num){
    printf("[DEPENDENCIAS]\n");
    //lidar Waitting
    int status;
    for (int i = 0; i < nrwaitingProcess; i++){
        int pid = waiting[i][0];
        if(waitpid(pid, &status, WNOHANG)!=0){
            int idxTask = waiting[i][1];
            removeRunning(idxTask);
            removeWaiting(idxTask);
            i--;
        }
    }
    
    //lidar Pendentes
    printf("\t\t\t\t\t<%d>", nrpendingRequests);
    for (int i = 0; i < nrpendingRequests; i++){
        int idxTask = pendingRequestsIdx[i][0];
        if(goPendingOrNot(tasks[idxTask])==OK){
            int fifoU = removePending(i);
            // ATENCAO VER SE NAO MUDA NADA, RECOPIAR O CODIGO DA MAIN DE NOVO MAIS PARA A FRENTE daqui para baixo só !!! e resolver possiveis problemas com variaveis usadas

            int pid1, pid2;
            int idx = addRunning(idxTask);
                
            if((pid1=fork())==0){// para não deixar o processo que corre o servidor à espera fazer double fork 
                int status2;
                if ((pid2=fork())==0){
                    showRunning(fifoU);

                    redirecionar(tasks[idxTask]->args[2], tasks[idxTask]->args[3]);
                    aplicarTransformacoes(tasks[idxTask]);
                    _exit(OK);
                }
                else{
                    waitpid(pid2, &status2, 0);//mal nunca recebe
                    printf("pai: FEEEEIIIIITTTOOOOOO!!!\n");
                    showConclued(fifoU);
                    //kill(pidServidor, SIGUSR1);//=???
                    //enviar sinal para acordar pendentes?? conformar se esta bem
                }
                _exit(OK);
            }
            //daqui para a frente está no processo principal
            else{
                int stat;
                if(waitpid(pid1, &stat, WNOHANG)==0){
                    addWaiting(pid1, idxTask);
                }
                else{
                    removeRunning(idx);
                }
            }
            close(fifoU);//como o filho do filho tem copia do descritor pode usa-la
        }
    }
}

void handlerAlarm(int num){
    printf("[ALARM]\n");
    alarm(ALARM_TIME);
}

int main(int argc, char const *argv[]){
    setbuf(stdout, NULL);//tirar depois

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

    if(signal(SIGALRM, handlerAlarm) == SIG_ERR){
        perror("sigAlarm error");
        return ERROR;
    }
    if(signal(SIGTERM, handlerGracioso) == SIG_ERR){
        perror("sigTerm error");
        return ERROR;
    }
    if(signal(SIGUSR1, handlerDependences) == SIG_ERR){
        perror("sigTerm error");
        return ERROR;
    }
    

    // -------------- inicializations -------------- 
    nrpendingRequests=0;
    nrrunningRequests=0;
    tasksNr=0;
    nrwaitingProcess=0;
    for (int i = 0; i < 7; i++){
        using[i]=0;
    }
    for (int i = 0; i < 100; i++){
        pendingRequestsIdx[i][0] = -1;
        pendingRequestsIdx[i][1] = -1;
        runningRequestsIdx[i] = -1;
        waiting[i][0] = -1;
        waiting[i][1] = -1;
    }
    
    
    // -------------- alarm subroutine -------------- 
    pidServidor = getpid();
    if((alrm=fork())==0){//criar subrotina de alarme para dizer ao servidor para constantemente verificar por coisas pendentes/waitings
        alarm(ALARM_TIME);
        while (1){
            pause();//espera que o alarme acabe
            kill(pidServidor, SIGUSR1);//manda o servidor ver pendentes e waitings
        }
        _exit(OK);
    }
    
    // -------------- handle a cliente -------------- 
    int fifoW, fifoU, n;
    if((res = mkfifo(WRITE_NAME, 0622)) == ERROR){// rw-w--w--
        perror("error creating the fifo Write(client)");
        return ERROR;
    }
    //as permissoes podem mudar, para prevenir vamos acertar as permissoes
    chmod(WRITE_NAME, 0622);


    printf("[Debug]Ready to accept client requests. Mypid<%d>\n", pidServidor);        
    if ((fifoW = open(WRITE_NAME, O_RDONLY, 0622)) == ERROR){
        perror("Erro a abrir FIFO(write)");
        return ERROR;
    }
    Pedido temporary = malloc(sizeof(struct pedido));
    
    // na graciosa fechar este e acaba o while
    if ((backbone = open(WRITE_NAME, O_WRONLY)) == ERROR){//backbone, nunca vai receber EOF na leitura
        perror("Erro a abrir FIFO(write)");
        return ERROR;
    }

    while((n = read(fifoW,temporary,sizeof(struct pedido))) > 0){//ler do cliente
        Pedido infoStruct = malloc(sizeof(struct pedido));

        infoStruct = temporary;

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

        if(infoStruct->elems==2){//recebe pedido de status
            showSatus(fifoU);
            close(fifoU);
            free(newFifoName);

            kill(pidServidor, SIGUSR1);//manda o servidor ver pendentes e waitings
        }else{//pid proc-file file-in file-out transf1...
            int position = addTask(infoStruct);//rever !!!!!
            printf("--testing pendente--\n");
            if( goPendingOrNot(infoStruct) == ERROR ){//passar o pid à frente, fica pendente
                printf("pendente\n");
                addPending(position, fifoU);//esperar ate que o sinal SIGSUR1 faca a sua parte 
                showPendent(fifoU);
            }else{
                printf("nao pendente\n");
                int pid1, pid2, status1;
                int idx = addRunning(position);
                
                if((pid1=fork())==0){// para não deixar o processo que corre o servidor à espera fazer double fork 
                    int status2;
                    if ((pid2=fork())==0){
                        showRunning(fifoU);

                        redirecionar(infoStruct->args[2], infoStruct->args[3]);
                        aplicarTransformacoes(infoStruct);
                        _exit(OK);
                    }
                    else{
                        waitpid(pid2, &status2, 0);//mal nunca recebe
                        printf("pai: FEEEEIIIIITTTOOOOOO!!!\n");
                        showConclued(fifoU);
                        //kill(pidServidor, SIGUSR1);//=???
                        //enviar sinal para acordar pendentes?? conformar se esta bem
                    }
                    _exit(OK);
                }
                //daqui para a frente está no processo principal
                else{
                    if(waitpid(pid1, &status1, WNOHANG)==0){
                        addWaiting(pid1, position);
                    }
                    else{
                        removeRunning(idx);
                    }
                }
                close(fifoU);//como o filho do filho tem copia do descritor pode usa-la
            }
            kill(pidServidor, SIGALRM);
            // de cima ou isto: alarm(0);
            free(newFifoName);
            //dar free nos mallocs todos(palavras e afins)
        }
        printf("acabei 1 pedido\n\n\n");
    }
    if(n==ERROR){
        perror("error reading from fifoW");
    }
    close(backbone);
    close(fifoW);
    unlink(WRITE_NAME);//unlink nao apaga, logo. só quando todos fecham o fd.
    
    if(temporary)
        free(temporary);

    // na graciosa ir a todas as tasks e dar free em todas as structs, recursivamente tb

    if(transformations_folder)
        free(transformations_folder);

    //int trash;
    //waitpid(alrm, &trash, 0);//capturar o que sai do ciclo do alarm
    return 0;
}
