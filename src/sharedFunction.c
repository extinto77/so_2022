#include "sharedFunction.h"


void parse(char* string, char delimiter, char**result){//falta testar
    int idx=0;
    for (int i = 0; i < strlen(string);idx++){
        int j;
        //result[idx] = malloc(1024);
        for (j = 0; i+j<strlen(string) && string[i+j]!=delimiter; j++){
            result[idx][j] = string[i+j];
        }
        printf("-->%d<--", j);
        result[idx][j++] = '\0';
        i+=j;
    }
    result[idx]=NULL;
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
        printf("--%s--\n", inputPath);
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

void copyStruct(Pedido src, Pedido dst){
    dst->elems = src->elems;
    dst->priority = src->priority;
    for (int i = 0; i < src->elems; i++){
        sprintf(dst->args[i], "%s", src->args[i]);
    }
}