//server

//0->stdin
//1->stdout

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>

#define NOP 0
#define BCOMPRESS 1
#define BDECOMPRESS 2
#define GCOMPRESS 3
#define GDECOMPRESS 4
#define ENCRYPT 5
#define DECRYPT 6

//criar um com nomes?? torna codigo mais melhor bom??
int config[7];//nr maximos de cada tipo de filtros que podem executar ao mesmo tempo
int using[7];//nr de filtros de cada tipo que estao a executar no momento presente

char* pendingRequests[100];//pedidos em espera
int nrpendingRequests;

char* transformations_folder = NULL;

#define ERROR -1
#define OK 1

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
        if (line)
            free(line);
        if (string)
            free(string);
    }
    return OK;
}

char* concatStrings(char *s1, char *s2){
    char *result = malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

char** parse(char* string, char* delimiter){//falta testar
    char** array = malloc(sizeof(char*)*1024);//limitacao de 1024 items a dar parse, alteracao conformar se funciona assim
	char *ptr = strtok(string, delimiter);
    for (int i = 0; ptr != NULL; i++){
        char *step = malloc(strlen(ptr)+1);
        strcpy(step, ptr);
        array[i]=step;

        ptr = strtok(NULL, delimiter);
    }
    return array;//warning que pode desaparecer quando sai da funcao
}

int redirecionar(char *inputPath, char *outputPath){
    int input,output;

    input = open(inputPath, O_RDONLY);
    if(input == ERROR){
        perror("error redirecting input");
        return ERROR;
    }
    dup2(input,0);
    close(input);
  
    output = open(outputPath, O_TRUNC | O_WRONLY | O_CREAT, 0666); //0644
    if( output == ERROR){
        perror("error redirecting output");
        return ERROR;
    }
    dup2(output,1);
    close(output);

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

void addPending(char* request){// ver se nao vai ser preciso mais  info
    pendingRequests[nrpendingRequests] = malloc(strlen(request)+1);
    strcpy(pendingRequests[nrpendingRequests], request);
    nrpendingRequests++;
}

void removePending(int index){
    nrpendingRequests--;
    free(pendingRequests[index]);
    for (int i = index; i < nrpendingRequests; i++){
        pendingRequests[i] = pendingRequests[i+1];
    }
}

void showSatus(int fifo){//ver melhor o input
    char* buf = malloc(1024);

    //escrever mais alguma info antes??

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
    if(argc!=3){
        perror("invalid arguments to run the server");
        return ERROR;
    }
    int res = readConfigFile((char*)argv[1]);
    if(res == ERROR){
        return ERROR;
    }
    strcpy(transformations_folder, (char*)argv[2]);
    nrpendingRequests=0;
    for (int i = 0; i < 7; i++){
        using[i]=0;
    }
    for (int i = 0; i < 100; i++){
        pendingRequests[i] = NULL;
    }
    
    





    /* code */
    return 0;
}
