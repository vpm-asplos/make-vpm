#include "stdio.h"
#include <stdarg.h>
#include "libfs/libfs.h"

int _fclose(FILE *__stream)
{
    if ((int)__stream < 100)
    {
        printf("fclose: %d\n", (int)__stream);
        lfs_close((int)__stream);
        return 0;
    }
    return fclose(__stream);
}

int _fflush(FILE *__stream)
{
    //printf("PENDING: fflush\n");
    if ((int)__stream < 100)
    {
        return 0;
    }
    return fflush(__stream);
}

FILE *_fopen(const char *__restrict __filename,
             const char *__restrict __modes)
{

    char first = *__filename;
    if (first != '/')
    {
        //printf("fopen: %s\n", __filename);
        char new_name[256];
        sprintf(new_name,"/%s",__filename);
        int fd = lfs_open(new_name, LFS_O_RDONLY);
        printf("fopen: %s, fd: %d\n", new_name, fd);
        return (FILE *)(fd + 1);
    }
    // //return NULL;
    // int flags, fd;
    // if(strcmp(__modes,"r")!=0){
    //     flags = LFS_O_RDONLY;
    // }
    // else if (strcmp(__modes,"w")!=0){
    //     flags = LFS_O_WRONLY | LFS_O_CREAT;
    // }
    // else if (strcmp(__modes,"a")!=0){
    //     flags = LFS_O_WRONLY | LFS_O_APPEND | LFS_O_CREAT;
    // }
    // else if (strcmp(__modes,"r+")!=0){
    //     flags = LFS_O_RDWR;
    // }
    // else if (strcmp(__modes,"w+")!=0){
    //     flags = LFS_O_RDWR | LFS_O_CREAT;
    // }
    // else {
    //     flags = LFS_O_RDWR | LFS_O_CREAT | LFS_O_CREAT;
    // }
    // fd = lfs_open(__filename, flags);
    // printf("return: fopen\n");
    // return (FILE*)(uint32_t)fd;
    return fopen(__filename, __modes);
}

int _fprintf(FILE *__restrict __stream,
             const char *__restrict __format, ...)
{
    va_list args;
    va_start(args, __format);
    return vfprintf(__stream, __format, args);
}

int _fscanf(FILE *__restrict __stream,
            const char *__restrict __format, ...)
{
    va_list args;
    va_start(args, __format);
    return vfscanf(__stream, __format, args);
}

int _fputc(int __c, FILE *__stream)
{
    if ((int)__stream < 100)
    {
        printf("fput: %d\n", (int)__stream);
        lfs_write((int)__stream, &__c, 1);
        return 1;
    }
    return fputc(__c, __stream);
}

char *_fgets(char *__restrict __s, int __n, FILE *__restrict __stream)
{
    if ((int)__stream < 100)
    {
        printf("fget: %d, n: %d\n", (int)__stream, __n);
        char * ptr = __s;
        int res;
        while(1){
            res = lfs_read((int)__stream - 1, ptr, 1);
            //printf("%c",*ptr);
            if (*ptr == '\n'){
                break;
            }
            else if (*ptr == EOF || res == 0){
                return NULL;
            }
            ptr++;
        }
        ptr++;
        *ptr = NULL;
        return __s;
    }
    return fgets(__s, __n, __stream);
}

int _fputs(const char *__restrict __s, FILE *__restrict __stream)
{
    return fputs(__s, __stream);
}

size_t _fread(void *__restrict __ptr, size_t __size,
              size_t __n, FILE *__restrict __stream)
{
    if ((int)__stream < 100)
    {
        printf("fread: %d\n", (int)__stream);
        return lfs_read((int)__stream - 1, &__ptr, __size);
    }

    return fread(__ptr, __size, __n, __stream);
};

size_t _fwrite(const void *__restrict __ptr, size_t __size,
               size_t __n, FILE *__restrict __s)
{
    if ((int)__s < 100)
    {
        printf("fwrite: %d\n", (int)__s);
        return lfs_write((int)__s - 1, &__ptr, __size);
    }

    return fwrite(__ptr, __size, __n, __s);
}
