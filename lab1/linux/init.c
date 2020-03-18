#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sysmacros.h>

int main()
{
    if (mknod("/dev/ttyS0", S_IFCHR | S_IRUSR | S_IWUSR, makedev(4, 64)) == -1)
        perror("mknod() failed");
    else
        printf("create /dev/ttyS0 successefully\n");
    if (mknod("/dev/fb0", S_IFCHR | S_IRUSR | S_IWUSR, makedev(29, 0)) == -1)
        perror("mknod() failed");
    else
        printf("create /dev/fb0 successefully\n");
    if (mknod("./ttyS0", S_IFCHR | S_IRUSR | S_IWUSR, makedev(4, 64)) == -1)
        perror("mknod() failed");
    else
        printf("create ./ttyS0 successefully\n");
    if (mknod("./fb0", S_IFCHR | S_IRUSR | S_IWUSR, makedev(29, 0)) == -1)
        perror("mknod() failed");
    else
        printf("create ./fb0 successefully\n");
    fflush(stdout);
    char *const argv_execv[] = {NULL};
    if (fork() == 0)
    {
        if (execv("./1", argv_execv) < 0)
        {
            perror("Err on execv");
            return -1;
        }
        return 0;
    }
    fflush(stdout);
    sleep(7);
    if (fork() == 0)
    {
        if (execv("./2", argv_execv) < 0)
        {
            perror("Err on execv");
            return -1;
        }
        return 0;
    }
    sleep(1);
    if (fork() == 0)
    {
        if (execv("./3", argv_execv) < 0)
        {
            perror("Err on execv");
            return -1;
        }
        return 0;
    }
    while (1);
}