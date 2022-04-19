#include "sharedFunction.h"


char** parse(char* string, char* delimiter){//falta testar
    char** array = malloc(sizeof(char*)*1024);//limitacao de 1024 items a dar parse, alteracao conformar se funciona assim
	char *ptr = strtok(string, delimiter);
    int i;
    for (i = 0; ptr != NULL; i++){
        char *step = malloc(strlen(ptr)+1);
        strcpy(step, ptr);
        array[i]=step;

        ptr = strtok(NULL, delimiter);
    }
    array[i]=NULL;
    return array;//warning que pode desaparecer quando sai da funcao
}

char* concatStrings(char *s1, char *s2){
    char *result = malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}


int redirecionar(char *inputPath, char *outputPath){
    int input,output;

    input = open(inputPath, O_RDONLY);
    if(input == ERROR){
        perror("error redirecting input");
        return ERROR;
    }
    dup2(input,STDIN_FILENO);
    close(input);
  
    output = open(outputPath, O_TRUNC | O_WRONLY | O_CREAT, 0666); //0644
    if( output == ERROR){
        perror("error redirecting output");
        return ERROR;
    }
    dup2(output,STDOUT_FILENO);
    close(output);

    return OK;
}
