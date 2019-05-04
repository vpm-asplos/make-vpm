//
// Created by Ding Yuan on 2018-01-28.
//

#ifndef LIBFS_FILE_H
#define LIBFS_FILE_H
/**
 * Data structures and procedures to implement file operations.
 */
#include <stdint.h>
#include "inode.h"
#include "param.h"
#include "sync.h"

/*
 * One file structure is allocated
 * for each open/creat/pipe call.
 * Main use is to hold the read/write
 * pointer associated with each open
 * file.
 */
struct file
{
    bmutex_t     f_bmutex;
    uint8_t      f_flag;
    uint8_t      f_count;    /* reference count: number of open file descriptors */
    rptr_t       f_inode;    /* Relative pointer to inode structure */
    uint32_t     f_offset;   /* R/W offset */
};

/* flags */
#define FREAD   01
#define FWRITE  02

// #define FPIPE 04
int  open1 (inode_t *ip, int mode);
int  falloc(struct file *file);
int  ufalloc();
int  _access(inode_t *ip, uint16_t mode);
void itrunc(inode_t *ip);
int  validate_fd(int fd);
void flock (struct file *fp);
void funlock (struct file *fp);

#endif //LIBFS_FILE_H
