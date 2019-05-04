//
// Created by Ding Yuan on 2018-01-12.
//

#ifndef LIBFS_INODE_H
#define LIBFS_INODE_H

#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#include "lfs.h"
#include "param.h"
#include "sync.h"

// Data structures and utilities for inode handling

/* The i-node data structure. */
struct inode {
    bmutex_t   i_bmutex;    // Lock
    uint8_t    i_count;  /* Reference count. This is only used for inodes whose files are opened. Not sure whether this
                          * is useful -- might be useful when handling concurrency. TODO: consider delete? */
    uint16_t   i_number; // At this moment, this is not necessary as the inode number is just index into the inode array.
    uint16_t   i_mode;   /* mode (type + permission), see libfs.h */
    uint16_t   i_nlink;  /* number of directory entries */
    uid_t      i_uid;    /* owner */
    gid_t      i_gid;    /* owner's group */
    uint32_t   i_size1;  /* size of the file. */
    uint16_t   i_direntries; /* size of the dir file, measured in number of direntries.
                              * This may be different from i_size1/sizeof(struct libfs_dirent) when an entry in the dir has
                              * been deleted. In this case, i_direntries is decremented, but i_size1 remains the same. */
    // void       *i_addr[15]; /* 12 direct pointers, 1 indirect, 1 double-indirect, 1 triple indirect. */
    time_t     i_mtime;   /* Time of last modification. */
    rptr_t     i_addr[15]; /* 12 direct pointers, 1 indirect, 1 double-indirect, 1 triple indirect. */
};

typedef struct inode inode_t;


/* Data that is needed by namei. */
struct namei_data {
    int           error; // error code
    char          *cp;   // The char pointer to the pathname at the end of namei search
    inode_t       *parent_ip; // The inode pointer to the parent dir
    struct libfs_dirent *direntp; // The pointer to the dir entry in the parent dir of the pathname
};

#define NSEARCH 0
#define NCREATE 1
#define NDELETE 2

#define PTRS_PER_BLOCK (LFS_BLOCKSIZE/sizeof(rptr_t))
#define DIRECT_LIMIT   12
#define SINGLY_LIMIT   (DIRECT_LIMIT+PTRS_PER_BLOCK)
#define DOUBLY_LIMIT   (SINGLY_LIMIT+PTRS_PER_BLOCK*PTRS_PER_BLOCK)
#define TRIPLY_LIMIT   (DOUBLY_LIMIT+PTRS_PER_BLOCK*PTRS_PER_BLOCK*PTRS_PER_BLOCK)
#define MAX_BLOCKS     TRIPLY_LIMIT

int _mkdir(const char *pathname, uint16_t mode);
inode_t *ialloc();
inode_t *namei (const char *pathname, int flag, struct namei_data *ndata);
void *bread (inode_t *dp, uint32_t next_blk);
void *allocate_block(); // returns absolute address
int mkrootdir();
void zero_block (void *bp);
int wdir (inode_t *ip, const char *fname, uint32_t i_number);
int isPrefix (inode_t *shortip, inode_t *longip);
void *get_block_abs_addr(inode_t *ip, uint32_t bn);
rptr_t *get_block_ptr_addr(inode_t *ip, uint32_t bn);
rptr_t *get_indirect_addr (void *indirect_bp, uint32_t nth); // takes absolute address in the first param
time_t current_time();
void ilock (inode_t *ip);
void iunlock (inode_t *ip);
int  islocked(inode_t *ip);

// inode_t *maknode (uint16_t mode);
#endif //LIBFS_INODE_H
