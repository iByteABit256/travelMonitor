#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include "../lib/lists/lists.h"

// String compare function wrapper
int compareStr(void *a, void *b){
    return strcmp((char *)a, (char *)b);
}

// Parses parameters of executable
void parseExecutableParameters(int argc, char *argv[], int *monitorNum, int *buffSize, int *bloomSize, char **inDir){

    // Path of input file
    char *filePath;

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

    parseExecutableParameters(argc, argv, &numMonitors, &bufferSize, &sizeOfBloom, &input_dir);

    printf("numMonitors: %d\nbufferSize: %d\nsizeOfBloom: %d\ninput_dir: %s\n", numMonitors, bufferSize, sizeOfBloom, input_dir);

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
            printf("I am child %u\n", getpid());
            execl("./monitor", "./monitor", temp, (char *)NULL);
            exit(1);
        }
    }

    sleep(1);

    DIR *inDir;
    struct dirent *direntp;

    Listptr subdirs = ListCreate();

    if((inDir = opendir(input_dir)) == NULL){
        fprintf(stderr, "Could not open %s\n", input_dir);
    }else{
        while((direntp = readdir(inDir)) != NULL){
            printf("inode %d -> entry %s\n", (int)direntp->d_ino, direntp->d_name);
            ListInsertSorted(subdirs, direntp->d_name, &compareStr);
        }
        closedir(inDir);
    }

    ListPrintList(subdirs);
  
    // char arr1[80], arr2[80];
    // while (1)
    // {
    //     // Open FIFO for write only
    //     fd = open(strcat(myfifo, "0"), O_WRONLY);
  
    //     // Take an input arr2ing from user.
    //     // 80 is maximum length
    //     fgets(arr2, 80, stdin);
  
    //     // Write the input arr2ing on FIFO
    //     // and close it
    //     write(fd, arr2, strlen(arr2)+1);
    //     close(fd);
  
    //     // Open FIFO for Read only
    //     fd = open(myfifo, O_RDONLY);
  
    //     // Read from FIFO
    //     read(fd, arr1, sizeof(arr1));
  
    //     // Print the read message
    //     printf("User2: %s\n", arr1);
    //     close(fd);
    // }
    return 0;
}
