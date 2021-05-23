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
#define MAX_LINE 100

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

int getMonitorNum(int numMonitors, HTHash *countries, char *country){
    for(int i = 0; i < numMonitors; i++){
        if(HTExists(countries[i], country)){
            return i;
        }
    }

    return -1;
}

int main(int argc, char *argv[]){

    int numMonitors;
    int bufferSize;
    int sizeOfBloom;
    char *input_dir = NULL;

    parseExecutableParameters(argc, argv, &numMonitors, &bufferSize, &sizeOfBloom, &input_dir);

    if(bufferSize < 64){
        fprintf(stderr, "Buffer size must be at least 64 bytes\n");
        return 1;
    }
  
    // FIFO file path
    char *myfifo = "./tmp/";
  
    for(int i = 0; i < numMonitors; i++){

        // Creating the named pipes
        char temp[20];
        strncpy(temp, myfifo, 20);

        // Parent to child
        char str[20];
        sprintf(str, "%dv", i);
        
        mkfifo(strncat(temp, str, 20), 0666);

        // Child to parent
        char temp2[20];
        strcpy(temp2, temp);
        temp2[strlen(temp)-1] = '^';

        mkfifo(temp2, 0666);

        pid_t pid = fork();

        if(pid == -1){
            // Error
            fprintf(stderr, "Error during fork\n");
            exit(1);
        }else if(pid == 0){
            // Forked process
            execl("./monitor", "./monitor", temp, temp2, (char *)NULL);
            exit(1);
        }
    }

    DIR *inDir;
    struct dirent *direntp = NULL;

    Listptr subdirs = ListCreate();

    // Traverse input directory and add subdirectory names to list
    if((inDir = opendir(input_dir)) == NULL){
        fprintf(stderr, "Could not open %s\n", input_dir);
    }else{
        while((direntp = readdir(inDir)) != NULL){
            char *subdir = malloc(sizeof(char)*bufferSize);
            memset(subdir, 0, sizeof(char)*bufferSize);
            strcat(subdir, input_dir);
            strcat(subdir, direntp->d_name);
            ListInsertSorted(subdirs, subdir, &compareStr);
        }
        closedir(inDir);
    }

    char *current = malloc(strlen(input_dir)+2);
    strcpy(current, input_dir);
    strcat(current, ".");

    char *previous = malloc(strlen(input_dir)+3);
    strcpy(previous, input_dir);
    strcat(previous, "..");

    // File descriptor array for both directions
    int fd_arr[numMonitors][2];
    char *pipesNames[numMonitors][2];

    for(int i = 0; i < numMonitors; i++){
        char temp[20];
        strncpy(temp, myfifo, strlen(myfifo)+1);

        char postfix[20] = "";
        sprintf(postfix, "%dv", i);

        strcat(temp, postfix);

        fd_arr[i][0] = open(temp, O_WRONLY);
        char *pipename = malloc(strlen(temp)+1);
        strcpy(pipename, temp);
        pipesNames[i][0] = pipename;

        temp[strlen(temp)-1] = '^';

        fd_arr[i][1] = open(temp, O_RDONLY);
        char *pipename2 = malloc(strlen(temp)+1);
        strcpy(pipename2, temp);
        pipesNames[i][1] = pipename2;
    }

    // Pass buffsize and bloomsize to children
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

    HTHash countries[numMonitors];
    for(int i = 0 ; i < numMonitors; i++){
        countries[i] = HTCreate();
    }

    // Pass subdirectories to children with round robin order

    int count = 0;
    for(Listptr l = subdirs->head->next; l != l->tail; l = l->next){
        char *dirname = malloc(strlen(l->value)+1);
        strcpy(dirname, l->value);

        if(!strcmp(dirname, current) || !strcmp(dirname, previous)){
            free(dirname);
            continue;
        }

        char buff[bufferSize];
        memset(buff, 0, bufferSize);
        strcpy(buff, dirname);

        char *temp = dirname;
        char *prev = dirname;
        dirname = strtok(dirname, "/");
        while(dirname != NULL){
            prev = dirname;
            dirname = strtok(NULL, "/");
        }

        char *country = malloc(strlen(prev)+1);
        strcpy(country, prev);
        free(temp);

        HTInsert(countries[count%numMonitors], country, country);
        write(fd_arr[count%numMonitors][0], buff, bufferSize);

        count++;
    }

    for(Listptr l = subdirs->head->next; l != l->tail; l = l->next){
        free(l->value);
    }
    ListDestroy(subdirs);
    free(current);
    free(previous);

    for(int i = 0; i < numMonitors; i++){
        close(fd_arr[i][0]);
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

    for(int i = 0; i < numMonitors; i++){
        msgNum[i] = 0;
        tempBloom[i] = bloomInitialize(sizeOfBloom);
    }

    while(num_open_fds > 0){
        char buff[bufferSize];

        int ready = poll(pfds, nfds, -1);

        if(ready == -1){
            perror("poll error\n");
            exit(1);
        }

        // Read pipe input from every child still running
        for(int i = 0; i < nfds; i++){
            if(pfds[i].revents != 0){

                if(pfds[i].revents & POLLIN){

                    int bytes_read = read(pfds[i].fd, buff, bufferSize);

                    if(bytes_read == -1){
                        perror("Error in read");
                        exit(1);
                    }else if(bytes_read == 0){
                        continue;
                    }

                    // First message is virus name
                    if(msgNum[i] == 0){

                        char *virName = malloc((strlen(buff)+1)*sizeof(char));
                        strcpy(virName, buff);

                        curVirus[i] = malloc(strlen(virName)+1);
                        strcpy(curVirus[i], virName);

                        if(!HTExists(viruses, virName)){
                            Virus vir = newVirus(virName, sizeOfBloom, 9, 0.5);
                            HTInsert(viruses, vir->name, vir);
                        }

                        free(virName);
                    
                    }else if(msgNum[i]-1 < sizeOfBloom/bufferSize){   
                        // Bloomfilter parts copied to temporary bloomfilter

                        memcpy(tempBloom[i]->bloom+(msgNum[i]-1)*bufferSize, buff, bufferSize);
                    }else{
                        // Temporary bloomfilter acts as a mask on the actual bloomfilter

                        Virus vir = HTGetItem(viruses, curVirus[i]);
                        BloomFilter bF = vir->vaccinated_bloom;
                        bloomOR(bF, tempBloom[i]);

                        msgNum[i] = 0;
                        free(curVirus[i]);
                        curVirus[i] = NULL;
                        bloomDestroy(tempBloom[i]);
                        free(tempBloom[i]);
                        tempBloom[i] = bloomInitialize(sizeOfBloom);

                        continue;
                    }

                    msgNum[i]++;   

                }else{
                    if(close(pfds[i].fd)){
                        perror("close error\n");
                        exit(1);
                    }
                    pfds[i].fd = -1; //Ignore events on next call
                    num_open_fds--;
                }
            }
        }
    }
    
    char id[5] = "5510";
    vaccineStatusBloom(id, HTGetItem(viruses, "COWPOX"));

    char idate[5] = "2057";
    vaccineStatusBloom(idate, HTGetItem(viruses, "CHOLERA"));

    char id3[5] = "7437";
    vaccineStatusBloom(id3, HTGetItem(viruses, "SARS-1"));

    vaccineStatusBloom(id, HTGetItem(viruses, "COVID-19"));

    char *inbuf = malloc(MAX_LINE*sizeof(char));
    memset(inbuf, 0, MAX_LINE);

    // Input loop

    while(1){
        // Read input
        if(fgets(inbuf, MAX_LINE, stdin) != NULL){

            char *token = strtok(inbuf, " \n"); 

            if(token == NULL){
                fprintf(stderr, "ERROR1: INCORRECT SYNTAX\n\n");
                continue;
            }

            if(!strcmp(token, "/travelRequest")){
                char *id;
                char *date_str;
                Date date;
                char *countryFrom;
                char *countryTo;
                char *virName;

                id = strtok(NULL, " \n");

                if(id == NULL){
                    fprintf(stderr, "ERROR2: INCORRECT SYNTAX\n\n");
                    continue;
                }

                date_str = strtok(NULL, " \n");

                if(date_str == NULL){
                    fprintf(stderr, "ERROR3: INCORRECT SYNTAX\n\n");
                    continue;
                }        

                countryFrom = strtok(NULL, " \n");

                if(countryFrom == NULL){
                    fprintf(stderr, "ERROR4: INCORRECT SYNTAX\n\n");
                    continue;
                }

                countryTo = strtok(NULL, " \n");

                if(countryTo == NULL){
                    fprintf(stderr, "ERROR5: INCORRECT SYNTAX\n\n");
                    continue;
                }

                virName = strtok(NULL, " \n");

                if(virName == NULL){
                    fprintf(stderr, "ERROR6: INCORRECT SYNTAX\n\n");
                    continue;
                }

                date = malloc(sizeof(struct datestr));

                char *datetok1 = strtok(date_str, "-\n");
                char *datetok2 = strtok(NULL, "-\n");
                char *datetok3 = strtok(NULL, "-\n");

                // check if date is valid
                if(datetok1 != NULL && datetok2 != NULL && datetok3 != NULL){
                    date->day = atoi(datetok1); 
                    date->month = atoi(datetok2); 
                    date->year = atoi(datetok3);
                }else{
                    fprintf(stderr, "ERROR7: INCORRECT SYNTAX\n\n");
                    free(date);
                    continue;
                }

                Virus v = HTGetItem(viruses, virName);
                if(v == NULL){
                    printf("No data about given virus\n\n");
                    free(date);
                    continue;
                }

                if(bloomExists(v->vaccinated_bloom, id)){
                    int mon = getMonitorNum(numMonitors, countries, countryFrom);

                    fd_arr[mon][0] = open(pipesNames[mon][0], O_WRONLY);

                    char buff[bufferSize];
                    memset(buff, 0, bufferSize);
                    strcpy(buff, "travelRequest");

                    write(fd_arr[mon][0], buff, bufferSize);

                    strcpy(buff, id);

                    write(fd_arr[mon][0], buff, bufferSize);

                    strcpy(buff, virName);

                    write(fd_arr[mon][0], buff, bufferSize);

                    if(close(fd_arr[mon][0])){
                        perror("close error");
                        exit(1);
                    }

                    fd_arr[mon][1] = open(pipesNames[mon][1], O_RDONLY);

                    read(fd_arr[mon][1], buff, bufferSize);

                    if(!strcmp(buff, "YES")){
                        Date vacc_date = malloc(sizeof(struct datestr));
                        read(fd_arr[mon][1], buff, bufferSize);
                        vacc_date->day = atoi(buff);
                        read(fd_arr[mon][1], buff, bufferSize);
                        vacc_date->month = atoi(buff);
                        read(fd_arr[mon][1], buff, bufferSize);
                        vacc_date->year = atoi(buff);

                        if(getDiffDate(date, vacc_date)/30 >= 6){
                            printf("REQUEST REJECTED – YOU WILL NEED ANOTHER VACCINATION BEFORE TRAVEL DATE\n\n");
                        }else{
                            printf("REQUEST ACCEPTED – HAPPY TRAVELS\n\n");
                        }

                        free(vacc_date);

                    }else if(!strcmp(buff, "NO")){
                        printf("REQUEST REJECTED – YOU ARE NOT VACCINATED\n\n");
                    }

                    close(fd_arr[mon][1]);

                    fd_arr[mon][0] = open(pipesNames[mon][0], O_WRONLY);

                }else{
                    printf("REQUEST REJECTED – YOU ARE NOT VACCINATED\n\n");
                }
                free(date);
            }

            if(!strcmp(token, "/exit")){
                for(int i = 0; i < numMonitors; i++){
                    fd_arr[i][0] = open(pipesNames[i][0], O_WRONLY);

                    write(fd_arr[i][0], "exit", 5);

                    if(close(fd_arr[i][0])){
                        perror("close error");
                        exit(1);
                    }
                }

                break;
            }
        }
    }

    for(int i = 0 ; i < numMonitors; i++){
        if(close(fd_arr[i][0])){
            //perror("close error");
            //exit(1);
        }
        if(close(fd_arr[i][1])){
            //perror("close error");
            //exit(1);
        }
    }

    // Memory freeing

    free(inbuf);

    for(int i = 0; i < viruses->curSize; i++){
		for(Listptr l = viruses->ht[i]->next; l != l->tail; l = l->next){
            HTEntry ht = l->value;
            //free(ht->key);
            Virus v = ht->item;
            destroyVirus(v);
            free(v);
        }
    }

    HTDestroy(viruses);
    free(pfds);
    
    for(int i = 0; i < numMonitors; i++){
        HTHash ht = countries[i];
        for(int i = 0; i < ht->curSize; i++){
            for(Listptr l = ht->ht[i]->next; l != l->tail; l = l->next){
                HTEntry ht = l->value;
                free(ht->item);   
            }
        }
        HTDestroy(ht);
        free(pipesNames[i][0]);
        free(pipesNames[i][1]);
        bloomDestroy(tempBloom[i]);
        free(tempBloom[i]);
    }

    return 0;
}
