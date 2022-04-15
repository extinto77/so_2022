//client

#include "sharedFunction.h"

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
    if(argc==1){// ./sdstore
        char* info = "To get server status use the command './sdstore status'\nTo submit a request to server the command should be in the the following synthax './sdstore proc-file priority input-filename output-filename transformation-id-1 transformation-id-2 ... transformation-id-n' ";
        write(1, info, strlen(info));
    }else if(argc>=5 && !strcmp(argv[1], "proc-file")){// ./sdstore proc-file file-in file-out transf1...
        //enviar para o server pedido de aplicaco de transformacoes
        
        return OK;
    }else if(argc==2 && !strcmp(argv[1],"status")){// ./sdstore status
        //enviar para o server pedido de status
        return OK;
    }else{
        perror("error on the input arguments");
        return ERROR;
    }

    return 1;
}
