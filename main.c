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
int config[7];

#define ERROR -1
#define OK 1

//0->stdin
//1->stdout

char* concatStrings(char *s1, char *s2){
    char *result = malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

char** parse(char* string, char* delimiter){//falta testar
    char* array[1024];//limitacao de 1024 items a dar parse
	char *ptr = strtok(string, delimiter);
    for (int i = 0; ptr != NULL; i++){
        char *step = malloc(strlen(ptr)+1);
        strcpy(step, ptr);
        array[i]=step;

        ptr = strtok(NULL, delimiter);
    }
    return array;//warning que pode desaparecer quando sai da funcao
}

void readConfigFile(char *path){
    FILE * fp = fopen(path, "r");
    if (fp == NULL)
        perror("error reading config file");
    else{
        char * line = NULL;
        size_t size = 0;
        ssize_t read;
        char* string;
        int nr;
        while ((read = getline(&line, &size, fp)) != -1) {
            read = sscanf(line, "%s %d" , string, &nr);
            if(read != 2)
                perror("erro de sintaxe no ficheiro config");

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
    }
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
        return(ERROR);
    }
    return OK;//nunca chega aqui??
}

int verificarSintax(char** transformacoes, int nTransf){
    int count=0;
    for (int i = 0; i < nTransf; i++){
        if (!strcmp(transformacoes[i], "nop"))
            count++;
        else if (!strcmp(transformacoes[i], "bcompress"))
            count++;
        else if (!strcmp(transformacoes[i], "bdecompress"))
            count++;
        else if (!strcmp(transformacoes[i], "gcompress"))
            count++;
        else if (!strcmp(transformacoes[i], "gdecompress"))
            count++;
        else if (!strcmp(transformacoes[i], "encrypt"))
            count++;
        else if (!strcmp(transformacoes[i], "decrypt"))
            count++;
    }
    return count;
}

int main(int argc, char const *argv[]){
    /*readConfigFile("etc/config.txt");
    for(int i=0;i<7;i++)
        printf("%d-> %d\n", i, config[i]);
    */




    char* cache[]={"encrypt", "decrypt"};
    redirecionar("test_files/pdfTest.pdf", "test_files/novoPdfTest.pdf");
    aplicarTransformacoes(cache, 2);
    return 0;
}
