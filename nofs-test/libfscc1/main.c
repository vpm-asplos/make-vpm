#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libfs.h"
#include <dirent.h>
#include <stdbool.h>
#include "lfs_error.h"
#include <linux/kernel.h>
#include <sys/syscall.h>
#include "measure-time.h"

#define PBRKSTART 0x2a0002000000
#define PCREATE 1
#define PSHARE 2
#define PBRKSIZE 0x010000000

/*
void calc_diff(struct timespec *smaller, struct timespec *bigger, struct timespec *diff)
{
    if (smaller->tv_nsec > bigger->tv_nsec)
    {
        diff->tv_nsec = 1000000000 + bigger->tv_nsec - smaller->tv_nsec;
        diff->tv_sec = bigger->tv_sec - 1 - smaller->tv_sec;
    }
    else 
    {
        diff->tv_nsec = bigger->tv_nsec - smaller->tv_nsec;
        diff->tv_sec = bigger->tv_sec - smaller->tv_sec;
    }
}*/

void check_file(const char *name)
{
    char buf[150];
    int ret;
    int fd = lfs_open(name, LFS_O_RDONLY);
    if (fd == -1)
    {
        printf("****check error! Failed to open: %s\n", name);
        exit(1);
    }
    else
    {
        ret = lfs_read(fd, buf, 1);
        if (ret != 1)
        {
            printf("****check error! Failed to read: %s\n", name);
            lfs_close(fd);
            exit(1);
        }
        lfs_close(fd);
    }
}

void loaddir(const char *name, int indent, int remaining)
{
    if (!remaining)
        return;
    bool dot;
    if (strcmp(name, ".") == 0)
        dot = true;
    else
        dot = false;
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(name)))
        return;

    while ((entry = readdir(dir)) != NULL)
    {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
        if (entry->d_type == DT_DIR)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            //printf("%*s[%s]\n", indent, "", entry->d_name);
            if (lfs_mkdir(dot ? &path[1] : path, 0))
                printf("failed to create directory: %s, %d\n", dot ? &path[1] : path, lfs_error);
            //else
            //printf("created directory: %s\n",dot ? &path[1] : path);
            loaddir(path, indent + 2, remaining - 1);
        }
        else
        {
            FILE *f = fopen(path, "r");
            if (!f)
                continue;
            int fd = lfs_creat(dot ? &path[1] : path, (IRUSR | IWUSR | IRGRP | IROTH)); //(IRUSR|IWUSR|IRGRP)
            if (fd == -1)
            {
                printf("failed to create: %s, %d\n", dot ? &path[1] : path, lfs_error);
                continue;
            }
            //printf("created: %s\n",dot ? &path[1] : path);
            char buff[1024];
            unsigned got;
            while (got = fread(&buff[0], 1, 1024, f))
            {
                lfs_write(fd, &buff[0], got);
            }
            fclose(f);
            lfs_close(fd);
            //printf("close: %s\n", path);
            //printf("%*s- %s\n", indent, "", entry->d_name);
        }
    }
    closedir(dir);
}
int main(int arc, char **args)
{
    long ret_t2_1 = syscall(334, "newm", 4, PCREATE);
    if (ret_t2_1 < 0)
    {
        printf("Cannot create region because of ID conflict. Now exiting.\n");
        ret_t2_1 = syscall(334, "newm", 4, PSHARE);
        if (ret_t2_1 < 0)
            return 0;
    }
    printf("creating pheap\n");
    long ret_t2_2 = syscall(333, PBRKSTART + PBRKSIZE);

    void *lfs_buf = PBRKSTART;
    lfs_init(lfs_buf, LFS_FORMAT, 512 * 1024 * 1024);
    if (lfs_mkdir("/usr", 0))
        printf("failed to create directory: /usr\n");
    if (lfs_mkdir("/usr/include", 0))
        printf("failed to create directory: /usr/include\n");
    // if (lfs_mkdir("/usr/include/x86_64-linux-gnu", 0))
    //     printf("failed to create directory: /usr/include/x86_64-linux-gnu\n");
    if (lfs_mkdir("/usr/local", 0))
        printf("failed to create directory: /usr/local\n");
    if (lfs_mkdir("/usr/local/include", 0))
        printf("failed to create directory: /usr/local/include\n");
    loaddir("/usr/include", 0, 1);
    // if (lfs_mkdir("/usr/include/asm", 0))
    //     printf("failed to create directory: /usr/include/asm\n");
    // loaddir("/usr/include/asm", 0, 100);
    loaddir("/usr/include/bits", 0, 100);
    loaddir("/usr/include/gnu", 0, 100);
    loaddir("/usr/include/sys", 0, 100);
    // loaddir("/usr/include/linux", 0, 100);
    //loaddir("/usr/include/x86_64-linux-gnu", 0, 100);
    loaddir("/usr/local/include", 0, 100);
    loaddir(".", 0, 100);

    arc--;
    char * newarg[arc+1];
    int i;
    for (i=0;i<arc;i++){
        newarg[i] = strdup(args[i+1]);
    }
    newarg[arc] = NULL;
    lfs_handoff_bias_mutex();
    syscall(335);
    struct timespec t1, t2, diff;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    int pid = fork();
    if(pid){
        int ret;
        wait(&ret);
        clock_gettime(CLOCK_MONOTONIC, &t2);
        calc_diff(&t1, &t2, &diff);
        if (ret==0){
            printf("success!\n");
            printf("operation tool: %ld.%09ld\n", diff.tv_sec, diff.tv_nsec);
        }
    }
    else{
        printf("executing %s!\n", newarg[0]);
        execv(newarg[0],newarg);
        printf("exec returned!\n");
    }
    printf("end of loader!\n");
    return 0;
}
