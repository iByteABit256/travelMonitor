#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
  
int main(int argc, char *argv[])
{
    int fd1;
  
    // FIFO file path
    //char * myfifo = "/tmp/myfifo";
  
    // Creating the named file(FIFO)
    // mkfifo(<pathname>,<permission>)
    //mkfifo(myfifo, 0666);

    printf("Opening %s\n", argv[1]);
    
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