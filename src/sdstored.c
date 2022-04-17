//server

//0->stdin
//1->stdout

#include "sharedFunction.h"

char* transformations_folder = NULL;

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

void addPending(int id){
    pendingRequestsIdx[nrpendingRequests] = id;
    nrpendingRequests++;
}

void addRunning(int id){
    runningRequestsIdx[nrrunningRequests] = id;
    nrpendingRequests++;
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

    for(int i=0;i<nrrunningRequests;i++){
        int idx = runningRequestsIdx[i];
        sprintf(buf,"task #%d: %s\n", idx, tasks[idx]);//imprime as que estao a correr no momento
        write(fifo, buf, strlen(buf));
    }

    // perguntar stor se tambem imprimimos os que estão pending

    for(int i=0;i<TRANS_NR;i++){
        write(fifo, "transf ",8);
        sprintf(buf,"%s: %d/%d (running/max)\n", transformacoesNome[i], using[i], config[i]);//estado de ocupacao de cada uma das transformacoes
        write(fifo, buf, strlen(buf));
    }
    if(buf)
        free(buf);
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
    // -------------- validating arguments and configuring server settings -------------- 
    if(argc!=3){
        perror("invalid arguments to run the server");
        return ERROR;
    }
    int res;
    if((res = readConfigFile((char*)argv[1])) == ERROR){
        return ERROR;
    }
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
    
    // -------------- creating fifo's -------------- 
    if((res = mkfifo("tmp/fifoWrite",0622)) == ERROR){// rw--w--w-
        perror("error creating the fifo files(write)");
        return ERROR;
    } 
    if((res = mkfifo("tmp/fifoRead",0644)) == ERROR){// rw-r--r--
        perror("error creating the fifo status(read)");
        return ERROR;
    }
    printf("Ready to accept client requests\n");
    // -------------- handle a cliente -------------- 
    
    



    // -------------- fazer sempre no fim -------------- 
    //existe o finally??

    if(transformations_folder) free(transformations_folder);
    return 0;
}
