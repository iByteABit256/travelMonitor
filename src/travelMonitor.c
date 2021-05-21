#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <poll.h>
#include "../src/vaccineMonitor.h"
#include "../lib/lists/lists.h"
#include "../lib/bloomfilter/bloomfilter.h"
#include "../lib/hashtable/htInterface.h"

#define INITIAL_BUFFSIZE 100

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
    char *input_dir = NULL;

    struct timespec *tspec = malloc(sizeof(struct timespec));
    tspec->tv_sec = 0;
    tspec->tv_nsec = 2000000;

    parseExecutableParameters(argc, argv, &numMonitors, &bufferSize, &sizeOfBloom, &input_dir);

    //printf("numMonitors: %d\nbufferSize: %d\nsizeOfBloom: %d\ninput_dir: %s\n", numMonitors, bufferSize, sizeOfBloom, input_dir);
  
    // FIFO file path
    char *myfifo = "./tmp/";
  
    for(int i = 0; i < numMonitors; i++){
        // Creating the named file(FIFO)
        // mkfifo(<pathname>, <permission>)
        char temp[20];
        strncpy(temp, myfifo, 20);

        char str[20];
        sprintf(str, "%dv", i);
        
        mkfifo(strncat(temp, str, 20), 0666);

        char temp2[20];
        strcpy(temp2, temp);
        temp2[strlen(temp)-1] = '^';

        mkfifo(temp2, 0666);

        pid_t pid = fork();
        if(pid == -1){
            fprintf(stderr, "Error during fork\n");
            exit(1);
        }else if(pid == 0){
            //printf("I am child %u\n", getpid());
            execl("./monitor", "./monitor", temp, temp2, (char *)NULL);
            exit(1);
        }
    }

    //nanosleep(tspec, NULL);

    DIR *inDir;
    struct dirent *direntp = NULL;

    Listptr subdirs = ListCreate();

    if((inDir = opendir(input_dir)) == NULL){
        fprintf(stderr, "Could not open %s\n", input_dir);
    }else{
        while((direntp = readdir(inDir)) != NULL){
            //printf("inode %d -> entry %s\n", (int)direntp->d_ino, direntp->d_name);
            char *subdir = malloc(sizeof(char)*bufferSize);
            memset(subdir, 0, sizeof(char)*bufferSize);
            strcat(subdir, input_dir);
            strcat(subdir, direntp->d_name);
            ListInsertSorted(subdirs, subdir, &compareStr);
        }
        closedir(inDir);
    }

    //ListPrintList(subdirs);

    char *current = malloc(strlen(input_dir)+2);
    strcpy(current, input_dir);
    strcat(current, ".");

    char *previous = malloc(strlen(input_dir)+3);
    strcpy(previous, input_dir);
    strcat(previous, "..");

    // for(int i = 0; i < numMonitors; i++){
    //     char temp[20];
    //     strncpy(temp, myfifo, 20);

    //     char str[20];
    //     sprintf(str, "%d", i);

    //     fd = open(temp, O_WRONLY);
    //     write(fd, &sizeOfBloom, sizeof(int));
    //     write(fd, &bufferSize, sizeof(int));
    //     close(fd);
    // }
    int fd_arr[numMonitors][2];

    for(int i = 0; i < numMonitors; i++){
        char temp[20];
        strncpy(temp, myfifo, 20);

        char postfix[20] = "";
        sprintf(postfix, "%dv", i);

        strcat(temp, postfix);

        fd_arr[i][0] = open(temp, O_WRONLY);

        temp[strlen(temp)-1] = '^';

        fd_arr[i][1] = open(temp, O_RDONLY);
    }

    int count = 0;

    for(int i = 0 ; i < numMonitors; i++){
        char buff[INITIAL_BUFFSIZE];
        memset(buff, 0, INITIAL_BUFFSIZE);
        sprintf(buff, "%d", bufferSize);

        write(fd_arr[i][0], buff, INITIAL_BUFFSIZE);

        char buff2[bufferSize];
        memset(buff2, 0, bufferSize);
        sprintf(buff2, "%d", sizeOfBloom);
        write(fd_arr[i][0], buff2, bufferSize);
    }

    for(Listptr l = subdirs->head->next; l != l->tail; l = l->next){
        char *dirname = l->value;

        if(!strcmp(dirname, current) || !strcmp(dirname, previous)){
            continue;
        }

        char buff[bufferSize];
        memset(buff, 0, bufferSize);
        strcpy(buff, dirname);

        write(fd_arr[count%numMonitors][0], buff, bufferSize);
        // nanosleep(tspec, NULL);

        count++;
    }

    for(int i = 0; i < numMonitors; i++){
        close(fd_arr[i][0]);
        //close(fd_arr[i][1]);
    }

    struct pollfd *pfds;
    int num_open_fds, nfds;

    num_open_fds = nfds = numMonitors;

    pfds = calloc(nfds, sizeof(struct pollfd));    
    if(pfds == NULL){
        perror("calloc error\n");
        exit(1);
    }

    for(int i = 0; i < numMonitors; i++){
        pfds[i].fd = fd_arr[i][1];
        pfds[i].events = POLLIN;
    }

    HTHash viruses = HTCreate();

    int msgNum[numMonitors];
    char *curVirus[numMonitors];
    BloomFilter tempBloom[numMonitors];
    int counter[numMonitors];
    int missed[numMonitors];

    for(int i = 0; i < numMonitors; i++){
        msgNum[i] = 0;
        tempBloom[i] = bloomInitialize(sizeOfBloom);
        counter[i] = 0;
        missed[i] = 0;
    }

    while(num_open_fds > 0){
        char buff[bufferSize];
        //memset(buff, '\0', bufferSize);

        int ready = poll(pfds, nfds, -1);

        if(ready == -1){
            perror("poll error\n");
            exit(1);
        }

        for(int i = 0; i < nfds; i++){
            if(pfds[i].revents != 0){
                //  printf("  fd=%d; events: %s%s%s%s\n", pfds[i].fd,
                //     (pfds[i].revents & POLLIN)  ? "POLLIN "  : "",
                //     (pfds[i].revents & POLLHUP) ? "POLLHUP " : "",
                //     (pfds[i].revents & POLLERR) ? "POLLERR " : "",
                //     (pfds[i].revents & POLLNVAL) ? "POLLNVAL " : "");

                if(pfds[i].revents & POLLIN){
                    //memset(buff, '\0', bufferSize);

                    int bytes_read = read(pfds[i].fd, buff, bufferSize);

                    if(bytes_read == -1){
                        perror("Error in read");
                        exit(1);
                    }else if(bytes_read == 0){
                        missed[i]++;
                        continue;
                    }

                    // printf("Read %d bytes\n", bytes_read);

                    if(msgNum[i] == 0){
                        counter[i]++;
                        // printf("%d viruses read from fd %d\n", counter[i], pfds[i].fd);
                        // printf("%d messages missed from fd %d\n", missed[i], pfds[i].fd);
                        char *virName = malloc((strlen(buff)+1)*sizeof(char));
                        strcpy(virName, buff);

                        curVirus[i] = virName;

                        if(!HTExists(viruses, virName)){
                            Virus vir = newVirus(virName, sizeOfBloom, 9, 0.5);
                            HTInsert(viruses, virName, vir);
                        }
                    }else if(msgNum[i]-1 < sizeOfBloom/bufferSize){         
                        memcpy(tempBloom[i]->bloom+(msgNum[i]-1)*bufferSize, buff, bufferSize);
                    }else{
                        Virus vir = HTGetItem(viruses, curVirus[i]);
                        BloomFilter bF = vir->vaccinated_bloom;
                        bloomOR(bF, tempBloom[i]);

                        msgNum[i] = 0;
                        curVirus[i] = NULL;
                        bloomDestroy(tempBloom[i]);
                        tempBloom[i] = bloomInitialize(sizeOfBloom);

                        continue;
                    }

                    msgNum[i]++;
                    // printf("Message received: %s\n", buff);      

                }else{
                    if(close(pfds[i].fd)){
                        perror("close error\n");
                        //exit(1);
                    }
                    pfds[i].fd = -1; //Ignore events on next call
                    num_open_fds--;
                }
            }
        }
    }

    char id[5] = "5510";
    vaccineStatusBloom(id, HTGetItem(viruses, "COWPOX"));

    char id2[5] = "2057";
    vaccineStatusBloom(id2, HTGetItem(viruses, "CHOLERA"));

    char id3[5] = "7437";
    vaccineStatusBloom(id3, HTGetItem(viruses, "SARS-1"));

    vaccineStatusBloom(id, HTGetItem(viruses, "COVID-19"));

    return 0;
}
