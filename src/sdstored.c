//server

//0->stdin
//1->stdout

#include "sharedFunction.h"

char* transformations_folder = NULL;

//criar um com nomes das transformacoes?? torna codigo mais melhor bom??
int config[7];//nr maximos de cada tipo de filtros que podem executar ao mesmo tempo
int using[7];//nr de filtros de cada tipo que estao a executar no momento presente

int pendingRequestsIdx[100];//pedidos em espera
int nrpendingRequests;

int runningRequestsIdx[100];//pedidos a correr
int nrrunningRequests;

char* tasks[777];//todos os pedidos enviados para o servidor


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

            if (!strcmp(string, "nop"))
                config[NOP]=nr;
            else if (!strcmp(string, "bcompress"))
                config[BCOMPRESS]=nr;
            else if (!strcmp(string, "bdecompress"))
                config[BDECOMPRESS]=nr;
            else if (!strcmp(string, "gcompress"))
                config[GCOMPRESS]=nr;
            else if (!strcmp(string, "gdecompress"))
                config[GDECOMPRESS]=nr;
            else if (!strcmp(string, "encrypt"))
                config[ENCRYPT]=nr;
            else if (!strcmp(string, "decrypt"))
                config[DECRYPT]=nr;
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

// simplificar as 4 seguintes para 2?? ou nao vale a pena??
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

    write(fifo, "transf nop: ", 13);
    sprintf(buf,"%d/%d (running/max)\n",using[NOP],config[NOP]);
    write(fifo, buf, strlen(buf));

    write(fifo, "transf bcompress: ", 19);
    sprintf(buf,"%d/%d (running/max)\n",using[BCOMPRESS],config[BCOMPRESS]);
    write(fifo, buf, strlen(buf));

    write(fifo, "transf bdecompress: ", 21);
    sprintf(buf,"%d/%d (running/max)\n",using[BDECOMPRESS],config[BDECOMPRESS]);
    write(fifo, buf, strlen(buf));

    write(fifo, "transf gcompress: ", 19);
    sprintf(buf,"%d/%d (running/max)\n",using[GCOMPRESS],config[GCOMPRESS]);
    write(fifo, buf, strlen(buf));

    write(fifo, "transf gdecompress: ", 21);
    sprintf(buf,"%d/%d (running/max)\n",using[GDECOMPRESS],config[GDECOMPRESS]);
    write(fifo, buf, strlen(buf));

    write(fifo, "transf encrypt: ", 17);
    sprintf(buf,"%d/%d (running/max)\n",using[ENCRYPT],config[ENCRYPT]);
    write(fifo, buf, strlen(buf));

    write(fifo, "transf decrypt: ", 17);
    sprintf(buf,"%d/%d (running/max)\n",using[DECRYPT],config[DECRYPT]);
    write(fifo, buf, strlen(buf));

    if(buf)
        free(buf);
}

int goPendingOrNot(char** transformacoes, int nTransformacoes){
    int after[7];
    for (int i = 0; i < 7; i++){
        using[i] = after[i];
    }
    for (int i = 0; i < nTransformacoes; i++){
        if (!strcmp(transformacoes[i], "nop"))
            after[NOP] += 1;
        else if (!strcmp(transformacoes[i], "bcompress"))
            after[BCOMPRESS] += 1;
        else if (!strcmp(transformacoes[i], "bdecompress"))
            after[BDECOMPRESS] += 1;
        else if (!strcmp(transformacoes[i], "gcompress"))
            after[GCOMPRESS] += 1;
        else if (!strcmp(transformacoes[i], "gdecompress"))
            after[GDECOMPRESS] += 1;
        else if (!strcmp(transformacoes[i], "encrypt"))
            after[ENCRYPT] += 1;
        else if (!strcmp(transformacoes[i], "decrypt"))
            after[DECRYPT] += 1;
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
    // validating arguments and configuring server settings
    if(argc!=3){
        perror("invalid arguments to run the server");
        return ERROR;
    }
    int res = readConfigFile((char*)argv[1]);
    if(res == ERROR){
        return ERROR;
    }
    strcpy(transformations_folder, (char*)argv[2]);

    // inicializations
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
    
    
    



    //fazer sempre no fim
    if(transformations_folder) free(transformations_folder);
    return 0;
}
