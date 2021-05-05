#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include "../lib/lists/lists.h"
#include "../lib/bloomfilter/bloomfilter.h"

// String compare function wrapper
int compareStr(void *a, void *b){
    return strcmp((char *)a, (char *)b);
}

// Parses parameters of executable
void parseExecutableParameters(int argc, char *argv[], int *monitorNum, int *buffSize, int *bloomSize, char **inDir){
    
    // Parameter indices
    int monitor = -1;
    int buff = -1;
    int bloom = -1;
    int dir = -1;

    if(argc > 1){
        for(int i = 1; i < argc; i++){
            if(strcmp(argv[i],"-m") == 0){
                monitor = i;
            }else if(strcmp(argv[i],"-b") == 0){
                buff = i;
            }else if(strcmp(argv[i],"-s") == 0){
                bloom = i;
            }else if(strcmp(argv[i],"-i") == 0){
                dir = i;
            }
        }
    }

    if(monitor == -1 || buff == -1 || bloom == -1 || dir == -1){
        fprintf(stderr, "ERROR: INCORRECT ARGUMENTS\n");
        exit(1);
    }

    if(monitor+1 < argc){
        *monitorNum = atoi(argv[monitor+1]);
    }else{
        fprintf(stderr, "ERROR: INCORRECT ARGUMENTS\n");
        exit(1);
    }

    if(buff+1 < argc){
        *buffSize = atoi(argv[buff+1]);
    }else{
        fprintf(stderr, "ERROR: INCORRECT ARGUMENTS\n");
        exit(1);
    } 

    if(bloom+1 < argc){
        *bloomSize = atoi(argv[bloom+1]);
    }else{
        fprintf(stderr, "ERROR: INCORRECT ARGUMENTS\n");
        exit(1);
    } 

    if(dir+1 < argc){
        *inDir = argv[dir+1];
    }else{
        fprintf(stderr, "ERROR: INCORRECT ARGUMENTS\n");
        exit(1);
    }
}

int main(int argc, char *argv[]){

    int numMonitors;
    int bufferSize;
    int sizeOfBloom;
    char *input_dir;

    struct timespec *tspec = malloc(sizeof(struct timespec));
    tspec->tv_sec = 0;
    tspec->tv_nsec = 2000000;

    parseExecutableParameters(argc, argv, &numMonitors, &bufferSize, &sizeOfBloom, &input_dir);

    //printf("numMonitors: %d\nbufferSize: %d\nsizeOfBloom: %d\ninput_dir: %s\n", numMonitors, bufferSize, sizeOfBloom, input_dir);

    int fd;
  
    // FIFO file path
    char *myfifo = "./tmp/";
  
    for(int i = 0; i < numMonitors; i++){
        // Creating the named file(FIFO)
        // mkfifo(<pathname>, <permission>)
        char temp[20];
        strncpy(temp, myfifo, 20);

        char str[20];
        sprintf(str, "%d", i);
        
        mkfifo(strncat(temp, str, 20), 0666);

        pid_t pid = fork();
        if(pid == -1){
            fprintf(stderr, "Error during fork\n");
            exit(1);
        }else if(pid == 0){
            //printf("I am child %u\n", getpid());
            execl("./monitor", "./monitor", temp, (char *)NULL);
            exit(1);
        }
    }

    nanosleep(tspec, NULL);

    DIR *inDir;
    struct dirent *direntp;

    Listptr subdirs = ListCreate();

    if((inDir = opendir(input_dir)) == NULL){
        fprintf(stderr, "Could not open %s\n", input_dir);
    }else{
        while((direntp = readdir(inDir)) != NULL){
            //printf("inode %d -> entry %s\n", (int)direntp->d_ino, direntp->d_name);
            char *subdir = malloc(sizeof(char)*100);
            strcat(subdir, input_dir);
            strcat(subdir, direntp->d_name);
            ListInsertSorted(subdirs, subdir, &compareStr);
        }
        closedir(inDir);
    }

    //ListPrintList(subdirs);

    char *current = malloc(strlen(input_dir)+1);
    strcpy(current, input_dir);
    strcat(current, ".");

    char *previous = malloc(strlen(input_dir)+1);
    strcpy(previous, input_dir);
    strcat(previous, "..");

    int count = 0;


    for(Listptr l = subdirs->head->next; l != l->tail; l = l->next){
        char *dirname = l->value;

        if(!strcmp(dirname, current) || !strcmp(dirname, previous)){
            continue;
        }

        char temp[20];
        strncpy(temp, myfifo, 20);

        char postfix[20] = "";
        sprintf(postfix, "%d", count%numMonitors);

        strcat(temp, postfix);
        //strcat(dirname, "\n");
        fd = open(temp, O_WRONLY);
        //printf("Writing %s to pipe %d\n", dirname, count%numMonitors);
        write(fd, dirname, strlen(dirname)+1);
        nanosleep(tspec, NULL);
        close(fd);

        count++;
    }

    for(int i = 0; i < numMonitors; i++){
        char temp[20];
        strncpy(temp, myfifo, 20);

        char postfix[20];
        sprintf(postfix, "%d", i);

        char EOT[20] = "Hello!";

        strcat(temp, postfix);
        fd = open(temp, O_WRONLY);
        //printf("Writing %s to pipe %d\n", EOT, i);
        write(fd, EOT, strlen(EOT)+1);
        nanosleep(tspec, NULL);
        close(fd);
    }

    return 0;
}
