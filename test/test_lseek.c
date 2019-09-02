#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

int main(){
    char* contents = "abcdefgh";
    char* new_contents = "12345";
    char* last_contents = "12345fgh";
    char read_contents[100];
    int fd = open("/mnt/tinyfs/a.txt", O_RDWR|O_CREAT|O_TRUNC, 0700);
    if(fd < 0){
        printf("open file error\n");
    }
    if(write(fd, contents, strlen(contents)) != strlen(contents))
        printf("write1 error\n");

    lseek(fd, 0, SEEK_SET);

    if(write(fd, new_contents, strlen(new_contents)) != strlen(new_contents))
        printf("write2 error\n");

    lseek(fd, 0, SEEK_SET);

    if(read(fd, read_contents, strlen(last_contents)) != strlen(last_contents))
        printf("read error or lseek error\n");
    if(!strcmp(last_contents, read_contents))
        printf("lseek test suceed!\n");
    else
        printf("lseek test faild!\n");

    close(fd);
    return 0;

}
