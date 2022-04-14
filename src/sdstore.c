//client

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>

#define ERROR -1
#define OK 1

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

int verificarSintax(char** transformacoes, int nTransf){//antes de enviar para server passar por isto
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
    if(argc==2 && !strcmp(argv[1],"status")){// ./sdstore status
        //enviar para o server pedido de status
        return OK;
    }
    else if(argc==1){// ./sdstore
        return OK;
    }else if(argc>=5 && !strcmp(argv[1], "proc-file")){// ./sdstore proc-file file-in file-out transf1...
        return OK;
    }else{
        perror("error on the input arguments");
        return ERROR;
    }

    return 1;
}
