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

    char *pipename = argv[1];
    char *pipename2 = argv[2];
    char *buff = malloc(sizeof(char)*buffsize);

    Listptr countryPaths = ListCreate();

    fd = open(pipename, O_RDONLY);
    fd2 = open(pipename2, O_WRONLY);

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

    // Read buffersize, bloomsize and subdirectory paths from parent

    int msgNum = 0;
    while(num_open_fds > 0){
        int ready = poll(pfds, nfds, -1);

        if(ready == -1){
            perror("poll error\n");
            exit(1);
        }

        if(pfds[0].revents != 0){
            if(pfds[0].revents & POLLIN){

                int bytes_read = read(fd, buff, buffsize);

                if(bytes_read == -1){
                    perror("Error in read");
                    exit(1);
                }else if(bytes_read == 0){
                    continue;
                }

                if(msgNum == 0){
                    // First message

                    buffsize = atoi(buff);
                }else if(msgNum == 1){
                    // Second message

                    bloomsize = atoi(buff);
                }else{
                    // Subdirectory path

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

    DIR *subdir;
    struct dirent *direntp;

    // Traverse subdirectories and save file paths in list

    Listptr filepaths = ListCreate();

    for(Listptr l = countryPaths->head->next; l != l->tail; l = l->next){
        char *subdirName = l->value;

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

    HTHash viruses = HTCreate();
    HTHash persons = HTCreate();
    HTHash countries = HTCreate();
    
    // Parse files

    for(Listptr l = filepaths->head->next; l != l->tail; l = l->next){
        char *filepath = l->value;

        parseInputFile(filepath, bloomsize, persons, countries, viruses);

        free(filepath);
    }

    ListDestroy(filepaths);

    // Send bloomfilters to parent
        // -First message is virus name
        // -Following messages are bloomfilter parts

    int count = 1;
    for(int i = 0; i < viruses->curSize; i++){
		for(Listptr l = viruses->ht[i]->next; l != l->tail; l = l->next){
            HTEntry ht = l->value;
            Virus v = ht->item;

			char *buff = malloc(sizeof(char)*buffsize);
            memset(buff, 0, sizeof(char)*buffsize);
            strcpy(buff, v->name);

            write(fd2, buff, buffsize);
            for(int i = 0; i <= bloomsize/buffsize; i++){
                if(i == bloomsize/buffsize){
                    // Remaining part of bloomfilter

                    memcpy(buff, v->vaccinated_bloom->bloom+i*buffsize, bloomsize%buffsize);
                }else{
                    memcpy(buff, v->vaccinated_bloom->bloom+i*buffsize, buffsize);
                }
                write(fd2, buff, buffsize);
            }
            free(buff);
            count++;
		}
	}

    if(close(fd2)){
        perror("close error\n");
        exit(1);
    }

    fd = open(pipename, O_RDONLY);
    //fd2 = open(pipename2, O_WRONLY);

    while(1){
        char buff[buffsize];
        int bytes_read = read(fd, buff, buffsize);

        if(bytes_read > 0){
            //printf("Received: %s\n", buff);

            if(!strcmp(buff, "travelRequest")){
                bytes_read = read(fd, buff, buffsize);

                char *id = malloc(strlen(buff)+1);
                strcpy(id, buff);

                bytes_read = read(fd, buff, buffsize);

                char *virName = malloc(strlen(buff)+1);
                strcpy(virName, buff);

                //printf("Received %s and %s\n", id, virName);

                Virus v = HTGetItem(viruses, virName);

                VaccRecord rec = skipGet(v->vaccinated_persons, id);

                if(rec){
                    //printf("VACCINATED ON %02d-%02d-%04d\n\n", rec->date->day, rec->date->month, rec->date->year);
                    close(fd);
                    fd2 = open(pipename2, O_WRONLY);
                    strcpy(buff, "YES");
                    write(fd2, buff, buffsize);
                    strcpy(buff, "");
                    sprintf(buff, "%d", rec->date->day);
                    write(fd2, buff, buffsize);
                    strcpy(buff, "");
                    sprintf(buff, "%d", rec->date->month);
                    write(fd2, buff, buffsize);
                    strcpy(buff, "");
                    sprintf(buff, "%d", rec->date->year);
                    write(fd2, buff, buffsize);
                    close(fd2);
                    fd = open(pipename, O_RDONLY);
                }else{
                    //printf("NOT VACCINATED\n\n");
                    close(fd);
                    fd2 = open(pipename2, O_WRONLY);
                    strcpy(buff, "NO");
                    write(fd2, buff, buffsize);
                    close(fd2);
                    fd = open(pipename, O_RDONLY);
                }

                free(id);
                free(virName);
            }
            if(!strcmp(buff, "exit")){
                break;
            }
        }
    }

    if(close(fd)){
        // perror("close error");
        // exit(1);
    }

    if(close(fd2)){
        // perror("close error");
        // exit(1);
    }

    // Memory freeing

    freeMemory(countries, viruses, persons);

    free(buff);
    free(pfds);

    exit(0);
}
