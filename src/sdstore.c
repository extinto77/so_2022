//client

#include "sharedFunction.h"

int verificarSintax(char** transformacoes, int nTransf){//antes de enviar para server passar por isto
    int count=0;
    for (int i = 0; i < nTransf; i++){
        for (int j = 0; j < TRANS_NR; j++){
            if (!strcmp(transformacoes[i], transformacoesNome[j])){
                count++;
                break;
            }
        }
    }
    return count;
}

int main(int argc, char const *argv[]){
    if(argc==1){// ./sdstore
        char* info = "To get server status use the command './sdstore status'\nTo submit a request to server the command should be in the the following synthax './sdstore proc-file priority input-filename output-filename transformation-id-1 transformation-id-2 ... transformation-id-n'\n";
        write(1, info, strlen(info));
    }else if(argc>=5 && !strcmp(argv[1], "proc-file")){// ./sdstore proc-file file-in file-out transf1...
        if(verificarSintax(&(argv[1]), argc-1)==argc-1){
            //enviar para o server pedido de aplicaco de transformacoes
            redirecionar(argv[2], argv[3]);//????

        }
        else{
            perror("syntax error in the transformations");
            return ERROR;
        }
    }else if(argc==2 && !strcmp(argv[1],"status")){// ./sdstore status
        //enviar para o server pedido de status
        return OK;
    }else{
        perror("error on the input arguments");
        return ERROR;
    }

    return 1;
}
