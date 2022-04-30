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

char* tasks[777];//todos os pedidos enviados para o servidor
//int tasksNr = nrpendingRequests + nrrunningRequests;

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
                if (line) free(line);
                if (string) free(string);
                return ERROR;
            }

            for (int i = 0; i < TRANS_NR; i++){
                if (!strcmp(string, transformacoesNome[i])){
                    config[i]=nr;
                    //printf("--%s->%d\n", transformacoesNome[i], nr);
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
    //printf("transNr: %d\n", nTransformacoes);

    for (i=0; i < nTransformacoes-1; i++){
        int res = pipe(p);
        if(res==ERROR){
            perror("error creating pipe");
            return ERROR;
        }
        pid_t pid;

        if((pid = fork())==0){
            dup2(p[1],1);
            if(close(p[1])==ERROR){
                perror("closing fd (aplicar transformacoes)");
            }
            if( close(p[0])==ERROR){
                perror("closing fd (aplicar transformacoes)");
            }
            //printf("mais que um filtro");

            if(execl(concatStrings(concatStrings(transformations_folder, "/"),transformacoes[i]),transformacoes[i],NULL) == ERROR){
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
        }
    }
    //printf("--FILTRO:%s--\n", concatStrings(concatStrings(transformations_folder, "/"),transformacoes[i]));
    if(execl(concatStrings(concatStrings(transformations_folder, "/"),transformacoes[i]),transformacoes[i],NULL) == ERROR){
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

void showSatus(int fifo){
    char* buf = malloc(1024);
    strcpy(buf, "---RUNING REQUESTS---\n");
    write(fifo, buf, strlen(buf));
    for(int i=0;i<nrrunningRequests;i++){
        int idx = runningRequestsIdx[i];
        sprintf(buf,"task #%d: (pid)%s\n", idx, tasks[idx]);//imprime as que estao a correr no momento
        write(fifo, buf, strlen(buf));
    }

    strcpy(buf, "\n---PENDING REQUESTS---\n");
    write(fifo, buf, strlen(buf));
    for(int i=0;i<nrpendingRequests;i++){
        int idx = pendingRequestsIdx[i];
        sprintf(buf,"pending #%d: (pid)%s\n", idx, tasks[idx]);//imprime as que estao a correr no momento
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
    char* str = SERVICE_PENDENT;
    write(fifo, str, strlen(str));
}

void showRunning(int fifo){
    char* str = SERVICE_RUNNING;
    write(fifo, str, strlen(str));
}

void showConclued(int fifo){
    char* str = SERVICE_FINISHED;
    write(fifo, str, strlen(str)+1);
}

int goPendingOrNot(char** transformacoes, int nTransformacoes){
    int after[TRANS_NR];
    for (int i = 0; i < TRANS_NR; i++){
        using[i] = after[i];
    }
    for (int i = 0; i < nTransformacoes; i++){
        for(int j=0;j<TRANS_NR;j++){
            if(!strcmp(transformacoes[i], transformacoesNome[j])){
                after[j]++;
                break;
            }
        }
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

    
    // -------------- handle a cliente -------------- 
    
    int fifoW, fifoU, n;

    pidServidor = getpid();
    while (1){//manter o fifo sempre a espera de mais pedidos
        deleteFolderContent("tmp");//se existir ficheiros de fifos na pasta tmp remover todos os ficheiros, apagar depois !!!!!!!
        
        if((res = mkfifo(WRITE_NAME, 0622)) == ERROR){// rw-w--w--
            perror("error creating the fifo Write(client)");
            return ERROR;
        }
        //as permissoes podem mudar, para prevenir vamos acertar as permissoes
        chmod(WRITE_NAME, 0622);

        printf("Ready to accept client requests\n");        
        if ((fifoW = open(WRITE_NAME, O_RDONLY, 0622)) == ERROR){
            perror("Erro a abrir FIFO(write)");
            continue;
        }
        Pedido temporary = malloc(sizeof(struct pedido));

        printf("leitura em espera");
        while((n = read(fifoW,temporary,sizeof(struct pedido))) > 0){//ler do cliente     !!!nas vezes depois nao fica bloqueado aqui
            printf("--struct-->%d-", temporary->elems);
            int nrPals = temporary->elems;
            char* palavras[nrPals];
            for (int i = 0; i < nrPals; i++){
                palavras[i]=strdup(temporary->args[i]);
            }

            if(nrPals==2){//recebe pedido de status (com pid antes)
                if(!fork()){
                    char* newFifoName = malloc(1024);
                    sprintf(newFifoName, "%s%s",READ_NAME, palavras[0]);//fifo especifico para enviar mensagens para processo especifico
                    printf("--%s--\n", newFifoName);

                    if ((fifoU = open(newFifoName,O_WRONLY, 0644)) == ERROR){//em vez de usar sinais tornar o fifo único para o cliente
                        perror("Erro a abrir FIFO(read)");
                        free(newFifoName);
                        continue;
                    }

                    printf("2palavras");
                    showSatus(fifoU);
                    close(fifoU);
                }
            }
            else{//pid proc-file file-in file-out transf1...
                char* newFifoName = malloc(1024);
                sprintf(newFifoName, "%s%s",READ_NAME, palavras[0]);//fifo especifico para enviar mensagens para processo especifico
                printf("--%s--\n", newFifoName);

                if ((fifoU = open(newFifoName,O_WRONLY, 0644)) == ERROR){//em vez de usar sinais tornar o fifo único para o cliente
                    perror("Erro a abrir FIFO(read)");
                    free(newFifoName);
                    continue;
                }
                printf("abri unico\n");
                //int position = addTask(single_Request);
                if( goPendingOrNot(&(palavras[1]), nrPals-1) == OK ){//passar o pid à frente, fica pendente
                    printf("pendente\n");
                    //addPending(position);//esperar ate que o sinal SIGSUR1 faca a sua parte 
                    showPendent(fifoU);
                }
                else{
                    printf("nao pendente\n");
                    //int idx = addRunning(position);
                    if(!fork()){// para não deixar o processo que corre o servidor à espera fazer double fork 
                        int status;
                        if (!fork()){
                            showRunning(fifoU);

                            redirecionar(palavras[2], palavras[3]);
                            aplicarTransformacoes(&(palavras[4]), nrPals-4);
                            _exit(OK);
                        }
                        else{
                            wait(&status);
                            //removeRunning(idx);// neste processo nao fica nos registos do processo principal!! ver e corrigir
                            printf("pai: FEEEEIIIIITTTOOOOOO!!!\n");
                            showConclued(fifoU);
                            //close(fifoU);
                            //close(fifoW);// é preciso?? ou no exit fecha as dependencias??
                            
                            //kill(pidServidor, SIGUSR1);//=???
                            //enviar sinal para acordar pendentes?? conformar se esta bem
                        }
                        _exit(OK);
                    }
                }
                close(fifoU);//como o filho do filho tem copia do descritor pode usa-la
                free(newFifoName);
                //dar free nos mallocs todos(palavras e afins)
            }
            printf("acabei 1 pedido\n");
                
            //}
            //printf("buffer vazio desde que li\n");
            //sleep(5);
        }
        if(n==ERROR){
            perror("error reading from fifoW");
            //continue;
        }
        close(fifoW);
        unlink(WRITE_NAME);

        //sleep(20);
    }
    



    // -------------- fazer sempre no fim -------------- 
    //existe o finally??

    free(transformations_folder);
    return 0;
}
