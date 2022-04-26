#include "sharedFunction.h"


void parse(char* string, char delimiter, char**result){//falta testar
    /*char* strcopy=malloc(1024);
    strcpy(strcopy, string);
    char *ptr = strtok(strcopy, delimiter);
    int i;
    for (i = 0; ptr != NULL; i++){
        sprintf(result[i], "%s", ptr);
        printf("--%d->%s--\n\n", i, result[i]);

        ptr = strtok(NULL, delimiter);
    }
    result[i]=NULL;*/

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
