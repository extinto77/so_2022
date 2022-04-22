#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef __SHARED_F__
#define __SHARED_F__

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


char** parse(char* string, char* delimiter);

char* concatStrings(char *s1, char *s2);

int redirecionar(char *inputPath, char *outputPath);




#endif