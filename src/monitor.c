#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <poll.h>
#include "../lib/lists/lists.h"
#include "../lib/bloomfilter/bloomfilter.h"
#include "../lib/hashtable/htInterface.h"
#include "../src/parser.h"
#include "../src/vaccineMonitor.h"

#define INITIAL_BUFFSIZE 100
  
int main(int argc, char *argv[])
{
    int fd, fd2;
    int bloomsize = 69;
    int buffsize = INITIAL_BUFFSIZE;
  
    // FIFO file path
    //char * myfifo = "/tmp/myfifo";
  
    // Creating the named file(FIFO)
    // mkfifo(<pathname>,<permission>)
    //mkfifo(myfifo, 0666);

    char *pipename = argv[1];
    char *pipename2 = argv[2];
    char *buff = malloc(sizeof(char)*buffsize);

    //printf("Opening %s\n", pipename);
    
    // char EOT[20] = "Hello!";

    Listptr countryPaths = ListCreate();

    fd = open(pipename, O_RDONLY);
    fd2 = open(pipename2, O_RDONLY);

    // struct timespec *tspec = malloc(sizeof(struct timespec));
    // tspec->tv_sec = 0;
    // tspec->tv_nsec = 2000000;

    struct pollfd *pfds;
    int num_open_fds, nfds;

    num_open_fds = nfds = 1;

    pfds = calloc(nfds, sizeof(struct pollfd));    
    if(pfds == NULL){
        perror("calloc error\n");
        exit(1);
    }

    pfds[0].fd = fd;
    pfds[0].events = POLLIN;

    //nanosleep(tspec, NULL);

    // read(fd, &bloomsize, sizeof(int));
    // read(fd, &buffsize, sizeof(int));

    // printf("bloomsize = %d\nbuffsize = %d\n", bloomsize, buffsize);

    int msgNum = 0;

    while(num_open_fds > 0){
        int ready = poll(pfds, nfds, -1);

        if(ready == -1){
            perror("poll error\n");
            exit(1);
        }

        if(pfds[0].revents != 0){
            if(pfds[0].revents & POLLIN){

                //printf("buf1 = %s\n", buff);
                int bytes_read = read(fd, buff, buffsize);
                //printf("buf2 = %s\n", buff);

                if(bytes_read == -1){
                    perror("Error in read");
                    exit(1);
                }else if(bytes_read == 0){
                    continue;
                }

                //printf("msgnum = %d, buff = %s\n", msgNum, buff);

                if(msgNum == 0){
                    buffsize = atoi(buff);
                    //printf("buffsize = %d\n", buffsize);
                }else if(msgNum == 1){
                    bloomsize = atoi(buff);
                    //printf("bloomsize = %d\n", bloomsize);
                }else{
                    //printf("Process %u: Subdirectory %s\n", getpid(), buff);
                    char *country = malloc(strlen(buff)+1);
                    strcpy(country, buff);
                    ListInsertLast(countryPaths, country);
                }

                msgNum++;
            }else{
                if(close(pfds[0].fd)){
                    perror("close error\n");
                    exit(1);
                }
                pfds[0].fd = -1; //Ignore events on next call
                num_open_fds--;
            }
        }
    }

    //printf("Process %u: Subdirectories:\n", getpid());
    //ListPrintList(countryPaths);

    DIR *subdir;
    struct dirent *direntp;

    Listptr filepaths = ListCreate();

    for(Listptr l = countryPaths->head->next; l != l->tail; l = l->next){
        char *subdirName = l->value;

        //printf("subdirName = %s\n", subdirName);
        if((subdir = opendir(subdirName)) == NULL){
            fprintf(stderr, "Could not open %s\n", subdirName);
        }else{     
            while((direntp = readdir(subdir)) != NULL){
                if(strcmp(direntp->d_name, ".") && strcmp(direntp->d_name, "..")){
                    char *temp = malloc(buffsize);
                    strcpy(temp, subdirName);
                    strcat(temp, "/");
                    strcat(temp, direntp->d_name);
                    ListInsertLast(filepaths, temp);
                }
            }
            closedir(subdir);
        }
    }

    //ListPrintList(filepaths);
    HTHash viruses = HTCreate();
    HTHash persons = HTCreate();
    HTHash countries = HTCreate();
    
    for(Listptr l = filepaths->head->next; l != l->tail; l = l->next){
        char *filepath = l->value;

        parseInputFile(filepath, bloomsize, persons, countries, viruses);
    }

    popStatusByAge(HTGetItem(viruses, "COVID-19"), NULL, NULL, countries, NULL);

    exit(0);
}
