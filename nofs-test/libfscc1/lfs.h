//
// Created by Ding Yuan on 2017-12-31.
//
// This is the internal header file used by library fs implementation.
// Users: do not include this file. Instead include libfs.h
#ifndef LIBFS_LFS_H
#define LIBFS_LFS_H

/*
 * Layout:
 * | super block | struct file x NFILE | free block bitmap | inodes x 100 | root dir |
 */
#include <stdint.h>
#include <sys/types.h>

#include "libfs.h"
#include "param.h"
#include "sync.h"

typedef uintptr_t rptr_t;

#ifndef PERSISTENT_TMALLOC
#include <unistd.h>
#define INIT_BRK 0x602000 /* The starting BRK on Linux, wihtout ASLR */
#define MYSBRK sbrk
#define MYBRK brk
#endif

#define LFS_MAGIC     0xabadf001 /* magic number identifying us */

/* File types */
#define LFS_TYPE_INVAL 0 /* Should not ever appear */
#define LFS_TYPE_FILE  1 /* Regular file */
#define LFS_TYPE_DIR   2 /* Directory */


/* Utilities to return a page-aligned number, rounded up or down to the page size. */
#define PAGEALIGN_ROUNDUP(len) (((LFS_PAGEMASK)&len) ? ((len+LFS_PAGESIZE) & ~(LFS_PAGEMASK)):len)
#define PAGEALIGN_ROUNDDOWN(len) (len & ~ (LFS_PAGEMASK))

#define LFS_DEBUG 1 /* debug flag. */

/* Utilities to return the starting address of free block bitmap and inodes. */
#define SFILE_START(root) ((uintptr_t) root + (uintptr_t) LFS_BLOCKSIZE)
#define FREEMAP_START(root) ((uintptr_t) root + (uintptr_t) LFS_BLOCKSIZE + (uintptr_t)(NFILE*sizeof(struct file)))
#define INODES_START(root)  ((uintptr_t) root + (uintptr_t) LFS_BLOCKSIZE + (uintptr_t)(NFILE*sizeof(struct file)) + (uintptr_t) BMAP_BYTES)
#define BLOCKS_START(root)  ((uintptr_t) root + (uintptr_t) LFS_BLOCKSIZE + (uintptr_t)(NFILE*sizeof(struct file)) + (uintptr_t) BMAP_BYTES + (uintptr_t) (NINODES * sizeof(inode_t)))

void    *root_addr; // Address of the start of FS
uint8_t bmutex_pid; // The pid of this process used for biased lock

/* Utilities to convert between relative pointer and actual pointer */
#define REL2ABS(rptr) ((uintptr_t)root_addr + (uintptr_t)rptr)
#define ABS2REL(ptr)  ((uintptr_t)ptr - (uintptr_t)root_addr)

/* Superblock */
struct lfs_super {
    uint32_t     s_magic;     /* super block magic number, should be LFS_MAGIC */
    volatile int super_futex;       // Global lock (unbiased)!
    bmutex_t     filelist_bmutex;
    bmutex_t     bitmap_bmutex; // locks bitmap, the list of blocks, and next_block
    bmutex_t     inodelist_bmutex;
    uint32_t     s_nblocks;   /* Number of blocks in this fs. Note that it excludes the super block, bitmap, and inodes. */
    rptr_t       s_endaddr;  /* Ending address of the entire fs region. */
//  uint32_t  next_inode;  /* An index to the next available inode in the inode array. */
    uint32_t     next_block;  /* An index to the next available block. */
    uint8_t      nproc;      /* Number of processes using this fs. */
    char         s_pad[453]; /* Padding, making up for a 512-byte block. (Necessary?) */
};

/* This global data structure stores some info related to the user & process. */
struct lfs_user {
    bmutex_t u_bmutex;
    uid_t    u_uid;
    gid_t    u_gid;
    void    *u_ofile[NOFILE]; /* pointers to file structures of open file. */
    void    *u_cdir; /* Pointer to the inode of the current dir */
    char     u_cdirStr[256]; // pathname of current dir
} u;


void init_superblock (int fs_size);
void init_inodes();
void init_freemap();
void init_sfile();
void init_user();

// Internal helper functions
void panic (const char *format, ...);
int next_alloc_size();
#endif // LIBFS_LFS_H_
