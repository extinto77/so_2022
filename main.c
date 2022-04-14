#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>

#define ERROR -1
#define OK 1

//0->stdin
//1->stdout


int config[7];//0->nop|1->bcompress|2->bdecompress|3->gcompress|4->gdecompress|5->encrypt|6->decrypt

void readConfigFile(char *path){
    FILE * fp = fopen(path, "r");
    if (fp == NULL)
        perror("error reading config file");

    char * line = NULL;
    size_t size = 0;
    ssize_t read;
    char* string;
    int nr;
    while ((read = getline(&line, &size, fp)) != -1) {
        read = sscanf(line, "%s %d" , string, &nr);
        if(read != 2)
            perror("erro de sintaxe no ficheiro config");

        if (strcmp(string, "nop") == 0)
            config[0]=nr;
        else if (strcmp(string, "bcompress") == 0)
            config[1]=nr;
        else if (strcmp(string, "bdecompress") == 0)
            config[2]=nr;
        else if (strcmp(string, "gcompress") == 0)
            config[3]=nr;
        else if (strcmp(string, "gdecompress") == 0)
            config[4]=nr;
        else if (strcmp(string, "encrypt") == 0)
            config[5]=nr;
        else if (strcmp(string, "decrypt") == 0)
            config[6]=nr;
    }
    fclose(fp);
    if (line)
        free(line);
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

char* concatStrings(char *s1, char *s2){
    char *result = malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

int aplicarTransformacoes(char** transformacoes, int nTransformscoes){//assumindo que o redirecionamento já está feito antes...
    int i, p[2];

    for (i=0; i < nTransformscoes-1; i++){
        int res = pipe(p);
        pid_t pid;

        if((pid = fork())==0){
            dup2(p[1],1);
            close(p[1]);
            close(p[0]);    

            if(execl(concatStrings("SDStore-transf/",transformacoes[i]),transformacoes[i],NULL) == ERROR){
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
    if(execl(concatStrings("SDStore-transf/",transformacoes[i]),transformacoes[i],NULL) == ERROR){
        perror("Erro a aplicar filtro");
        _exit(ERROR);
    }
    return OK;
    //ver se tá certo daqui para cima e esquecer o resto 
    





    /*
    int p[2];
    int res = pipe(p);
    //fds[0]->escritor de escrita
    //fds[1]->escritor de leitura  
    if(res==-1){
        perror("Erro ao criar pipe.\n");
        return NULL;
    }

    pid_t pid;
    if((pid=fork())==0){//consumidor
        //fechar o file descriptor de escrita
        close(fds[1]);

        int nRead;
        //ler do pai
        while ((nRead = read(fds[0], &buffer, MAX_SIZE))>0){
            printf("#_: recebi a mensagem: %s\n", buffer);
        }    
        
        //fechar pipe de leitura
        close(fds[0]);

        _exit(0);
    }
    else if(pid==-1){
        perror("Fork not made.\n");
        return -1;
    }
    else{//produtor
        //fechar o file descriptor de leitura
        close(fds[0]);

        //sleep(5);

        for (int i = 0; chache[i]!=NULL; i++){
            //escrever para o filho
            write(fds[1], chache[i], strlen(chache[i]));
            sleep(1);//para não dar erro
        }

        //fechar pipe de escrita
        close(fds[1]);

        wait(&status);
    }
    */
}


int main(int argc, char const *argv[]){
    /*readConfigFile("config.txt");
    for(int i=0;i<7;i++)
        printf("%d-> %d\n", i, config[i]);
    */


    char* cache[]={"encrypt", "decrypt"};
    redirecionar("pdfTest.pdf", "novoPdfTest.pdf");
    aplicarTransformacoes(cache, 2);
    return 0;
}
