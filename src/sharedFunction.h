#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#ifndef __SHARED_F__
#define __SHARED_F__


#define STATUS 1
#define PROC_FILE 2

typedef struct pedido{
    int elems;
    char args[77][1024];// meter "status\n" se for do tipo STATUS
}*Pedido;



#define ERROR -1
#define OK 1

#define NOP 0
#define BCOMPRESS 1
#define BDECOMPRESS 2
#define GCOMPRESS 3
#define GDECOMPRESS 4
#define ENCRYPT 5
#define DECRYPT 6

#define TRANS_NR 7

#define SERVICE_PENDENT "Request status: PENDENT\n"
#define SERVICE_RUNNING "Request status: RUNNING\n"
#define SERVICE_FINISHED "Request status: FINISHED\n"
#define SERVICE_ABORTED "Request status: ABORTED\n"

#define PIPE_FILE_CREATED "Podes criar o fifo unico"

#define TEST ""

#define READ_NAME "tmp/fifoRead"
#define WRITE_NAME "tmp/fifoWrite"

void parse(char* string, char delimiter, char**result);

char* concatStrings(char *s1, char *s2);

int redirecionar(char *inputPath, char *outputPath);




#endif