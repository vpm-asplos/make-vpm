//
// Created by Ding Yuan on 2018-01-03.
//

#include <stdint.h>
#include <time.h>

#ifndef LIBFS_LIBFS_H
#define LIBFS_LIBFS_H

/* Flag values for lfs_init() */
#define LFS_FORMAT 0
#define LFS_INIT   1

/* File modes; 16 bits, here is the layout (copied from Unix) */
/*
 * 16      |    15   14        |  13        |   12         |    11      |    10        |  9 8 7 | 6 5 4  | 3 2 1  |
 * IALLOC  | type of file      | ILARG      | ISUID        | ISGID      | ISVTX        | RWX by | RWX by | RWX by |
 * file is | 10: dir           | large      | set user id  | set group  | save swapped | owner  | group  | others |
 * used    | 01: char special  | addressing | on execution | id on exe. | text even    |        |        |        |
 *         | 11: block special | algorithm  |              |            | after use    |        |        |        |
*/
#define IALLOC  0100000 /* file is used */
#define IFMT    060000  /* type of file */
#define IFDIR   040000  /* directory */
#define IFCHR   020000  /* character special */
#define IFBLK   060000  /* block special, 0 is regular */
#define ILARG   010000  /* large addressing algorithm */
#define ISUID   04000   /* set user id on execution */
#define ISGID   02000   /* set group id on execution */
#define ISVTX   01000   /* save swapped text even after use */
#define IRUSR   0400    /* read, write, execute permissions */
#define IWUSR   0200
#define IXUSR   0100
#define IRGRP   040
#define IWGRP   020
#define IXGRP   010
#define IROTH   04
#define IWOTH   02
#define IXOTH   01

// Stat:
struct lfs_stat {
    // dev_t     st_dev;     /* ID of device containing file */
    uint16_t  st_ino;     /* inode number */
    uint16_t  st_mode;    /* protection */
    uint16_t  st_nlink;   /* number of hard links */
    uid_t     st_uid;     /* user ID of owner */
    gid_t     st_gid;     /* group ID of owner */
    uint32_t  st_size;    /* total size, in bytes */
    //blkcnt_t  st_blocks;  /* number of 512B blocks allocated */
    //time_t    st_atime;   /* time of last access */
    time_t    st_modtime;   /* time of last modification */
    //time_t    st_ctime;   /* time of last status change */
};

#define LFS_NAMELEN 30 /* bytes per path component; with another 2 bytes as inode number, we have 32 bytes per dir entry. */

/* Directory entry. Total bytes per entry: 64. */
struct libfs_dirent {
    uint16_t i_number;
    char name[LFS_NAMELEN];
};

// Flags for open
#define LFS_O_RDONLY 0x0000
#define LFS_O_WRONLY 0x0001
#define LFS_O_RDWR   0x0002
#define LFS_O_CREAT  0x0100	/* second byte, away from DOS bits */
// #define O_EXCL		0x0200
// #define O_NOCTTY	0x0400
#define LFS_O_TRUNC	 0x0800
#define LFS_O_APPEND 0x1000
// #define O_NONBLOCK	0x2000

#define LFS_SEEK_SET 0
#define LFS_SEEK_CUR 1
#define LFS_SEEK_END 2

// This is the header file included by the user program
int  lfs_init (void *addr, int flag, int user_alloc_size); // soft update finished
int  lfs_mkdir (const char *pathname, uint16_t mode);  // soft update finished
void lfs_test(void *root);
int  lfs_stat (const char *pathname, struct lfs_stat *buf);
int  lfs_fstat (int fd, struct lfs_stat *buf);
int  lfs_lstat (const char *pathname, struct lfs_stat *buf);
int  lfs_open (const char *pathname, int flags, ...); // soft udpate finished
int  lfs_creat (const char *pathname, uint16_t mode); // soft update finished
int  lfs_close (int fd);
int  lfs_write(int fd, const void *buf, int count); // soft update finished
int  lfs_read(int fd, void *buf, int count);
int  lfs_link(const char *oldpath, const char *newpath); // soft update finished
int  lfs_unlink (const char *pathname); // soft update finished
int  lfs_rmdir (const char *pathname); // soft update finished
int  lfs_rename (const char *oldpath, const char *newpath); // soft update finished
int  lfs_opendir(const char *name);
int  lfs_readdir(int fd, struct libfs_dirent *dp);
int  lfs_closedir(int fd);
void lfs_handoff_bias_mutex();
void lfs_reacquire_bias_mutex();
int  lfs_chdir(const char *path);
char *lfs_getcwd(char *buf, int size);
int  lfs_lseek(int fd, int offset, int whence);

// Testing purpose
void lfs_printsuper();
void lfs_dump(uint16_t inumber, char *namebuf, int endidx);
uint32_t  lfs_used_blocks();
#endif //LIBFS_LIBFS_H
