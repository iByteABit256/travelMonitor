#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
  
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

    printf("Opening %s\n", pipename);
    
    // char EOT[2];
    // EOT[0] = 0x04;
    // EOT[1] = '\n';
    char EOT[20] = "Hello!";

    fd = open(pipename, O_RDONLY);

    struct timespec *tspec = malloc(sizeof(struct timespec));
    tspec->tv_sec = 0;
    tspec->tv_nsec = 2000000;

    while(strcmp(buff, EOT)){
        nanosleep(tspec, NULL);
        if(!read(fd, buff, 80)){
            continue;
        }
        printf("Process %u: Subdirectory %s\n", getpid(), buff);
    }
    close(fd);

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
