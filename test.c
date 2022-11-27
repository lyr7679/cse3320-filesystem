#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>

unsigned char data_blocks[4226][8192];
struct directory_entry *directory_ptr;
FILE *fp;

struct directory_entry{
  char name[125];
  int valid;
  char inode_idx[4];
};

void initDirectory()
{
  for(int i = 0; i < 2; i++)
  {
    strcpy(directory_ptr[i].name, "jimmy");
    directory_ptr[i].valid = 1;
    strcpy(directory_ptr[i].inode_idx, "125");
  }
}

int main() {
    FILE *fp = fopen("test.txt", "w+");
    memset(&data_blocks[0], 0 , 4226 * 8192);
    directory_ptr = (struct directory_entry *)&data_blocks[0];
    initDirectory();
    fwrite(&data_blocks[0], 8192, 4226, fp);
    fclose(fp);

    strcpy(directory_ptr[0].name, "bob");
    directory_ptr[0].valid = 3;
    printf("%s\n", directory_ptr[0].name);
    printf("%d\n", directory_ptr[0].valid);

    FILE *fd = fopen("test.txt", "r+");
    FILE *fy = fopen("test1.txt", "w+");
    fread(&data_blocks[0], 8192, 4226, fd);
    fwrite(&data_blocks[0], 8192, 4226, fy);
    fclose(fy);
    fclose(fd);
    if(fd)
        printf("aaaaa");
    printf("%s\n", directory_ptr[0].name);
    printf("%d\n", directory_ptr[0].valid);
    return 0;
}
