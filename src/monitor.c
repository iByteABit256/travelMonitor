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

// Frees up remaining memory after exiting
void freeMemory(HTHash countries, HTHash viruses, HTHash citizenRecords){
    for(int i = 0; i < countries->curSize; i++){
		for(Listptr l = countries->ht[i]->next; l != l->tail; l = l->next){
            Country country = ((HTEntry)(l->value))->item;
            free(country->name);
            free(country);
        }
    }

    for(int i = 0; i < citizenRecords->curSize; i++){
		for(Listptr l = citizenRecords->ht[i]->next; l != l->tail; l = l->next){
            Person per = ((HTEntry)(l->value))->item;
            free(per->citizenID);
            free(per->firstName);
            free(per->lastName);
            free(per);
        }
    }

    for(int i = 0; i < viruses->curSize; i++){
		for(Listptr l = viruses->ht[i]->next; l != l->tail; l = l->next){
            Virus vir = ((HTEntry)(l->value))->item;
            free(vir->name);
            bloomDestroy(vir->vaccinated_bloom);
            
            Skiplist skip = vir->vaccinated_persons;

            for(skipNode snode = skip->dummy->forward[0];
                snode != NULL; snode = snode->forward[0]){

                VaccRecord rec = snode->item;
                free(rec->date);
                free(rec);
            }

            skip = vir->not_vaccinated_persons;

            for(skipNode snode = skip->dummy->forward[0];
                snode != NULL; snode = snode->forward[0]){

                VaccRecord rec = snode->item;
                free(rec->date);
                free(rec);
            }

            skipDestroy(vir->vaccinated_persons);
            skipDestroy(vir->not_vaccinated_persons);

            free(vir->vaccinated_bloom);
            free(vir->vaccinated_persons);
            free(vir->not_vaccinated_persons);

            free(vir);
        }
    }

    HTDestroy(countries);
    HTDestroy(citizenRecords);
    HTDestroy(viruses);
}
  
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
    fd2 = open(pipename2, O_WRONLY);

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
                    char *temp = malloc(buffsize*10);
                    strcpy(temp, subdirName);
                    strcat(temp, "/");
                    strcat(temp, direntp->d_name);
                    ListInsertLast(filepaths, temp);
                }
            }
            closedir(subdir);
        }
        free(subdirName);
    }

    ListDestroy(countryPaths);

    //ListPrintList(filepaths);
    HTHash viruses = HTCreate();
    HTHash persons = HTCreate();
    HTHash countries = HTCreate();
    
    for(Listptr l = filepaths->head->next; l != l->tail; l = l->next){
        char *filepath = l->value;

        parseInputFile(filepath, bloomsize, persons, countries, viruses);

        free(filepath);
    }

    ListDestroy(filepaths);

    //popStatusByAge(HTGetItem(viruses, "COVID-19"), NULL, NULL, countries, NULL);
    // printf("%d viruses from pid %d\n", HTSize(viruses), getpid());

    int count = 1;
    for(int i = 0; i < viruses->curSize; i++){
		for(Listptr l = viruses->ht[i]->next; l != l->tail; l = l->next){
            HTEntry ht = l->value;
            Virus v = ht->item;

            // printf("%d/%d viruses\n", count, HTSize(viruses));
			char *buff = malloc(sizeof(char)*buffsize);
            memset(buff, 0, sizeof(char)*buffsize);
            strcpy(buff, v->name);

            //strcat(buff, "\n");
            // printf("Writing %s to parent\n", buff);
            write(fd2, buff, buffsize);
            for(int i = 0; i <= bloomsize/buffsize; i++){
                //memset(buff, '\0', buffsize);
                if(i == bloomsize/buffsize){
                    memcpy(buff, v->vaccinated_bloom->bloom+i*buffsize, bloomsize%buffsize);
                }else{
                    memcpy(buff, v->vaccinated_bloom->bloom+i*buffsize, buffsize);
                }
                // printf("Copied %s to buff\n", v->vaccinated_bloom->bloom+i*buffsize);
                //strcat(buff, "\n");
                // printf("Writing %s to parent\n", buff);
                write(fd2, buff, buffsize);
            }
            free(buff);
            count++;
		}
	}

    freeMemory(countries, viruses, persons);

    //close(fd2);
    free(buff);
    free(pfds);

    exit(0);
}
