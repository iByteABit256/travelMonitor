#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include "../lib/lists/lists.h"
#include "../lib/bloomfilter/bloomfilter.h"
  
int main(int argc, char *argv[])
{
    int fd;
  
    // FIFO file path
    //char * myfifo = "/tmp/myfifo";
  
    // Creating the named file(FIFO)
    // mkfifo(<pathname>,<permission>)
    //mkfifo(myfifo, 0666);

    char *pipename = argv[1];
    char buff[80] = "";

    //printf("Opening %s\n", pipename);
    
    char EOT[20] = "Hello!";

    Listptr countries = ListCreate();

    fd = open(pipename, O_RDONLY);

    struct timespec *tspec = malloc(sizeof(struct timespec));
    tspec->tv_sec = 0;
    tspec->tv_nsec = 2000000;

    while(strcmp(buff, EOT)){
        nanosleep(tspec, NULL);
        if(!read(fd, buff, 80) || !strcmp(buff, EOT)){
            continue;
        }
        //printf("Process %u: Subdirectory %s\n", getpid(), buff);
        char *country = malloc(strlen(buff)+1);
        strcpy(country, buff);
        ListInsertLast(countries, country);
    }
    close(fd);

    //printf("Process %u: Subdirectories:\n", getpid());
    //ListPrintList(countries);

    DIR *subdir;
    struct dirent *direntp;

    Listptr filepaths = ListCreate();

    for(Listptr l = countries->head->next; l != l->tail; l = l->next){
        char *subdirName = l->value;

        //printf("subdirName = %s\n", subdirName);
        if((subdir = opendir(subdirName)) == NULL){
            fprintf(stderr, "Could not open %s\n", subdirName);
        }else{     
            while((direntp = readdir(subdir)) != NULL){
                if(strcmp(direntp->d_name, ".") && strcmp(direntp->d_name, "..")){
                    char *temp = malloc(80);
                    strcpy(temp, subdirName);
                    strcat(temp, "/");
                    strcat(temp, direntp->d_name);
                    ListInsertLast(filepaths, temp);
                }
            }
            closedir(subdir);
        }
    }

    ListPrintList(filepaths);
    printf("\n\n");

    exit(0);
  
    // char str1[80], str2[80];
    // while (1)
    // {
    //     // First open in read only and read
    //     fd1 = open(myfifo,O_RDONLY);
    //     read(fd1, str1, 80);
  
    //     // Print the read string and close
    //     printf("User1: %s\n", str1);
    //     close(fd1);
  
    //     // Now open in write mode and write
    //     // string taken from user.
    //     fd1 = open(myfifo,O_WRONLY);
    //     fgets(str2, 80, stdin);
    //     write(fd1, str2, strlen(str2)+1);
    //     close(fd1);
    // }
    // return 0;
}
