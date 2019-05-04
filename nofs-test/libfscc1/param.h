//
// Created by Ding Yuan on 2018-02-02.
//

#ifndef LIBFS_PARAM_H
#define LIBFS_PARAM_H
// Parameter settings
// Various macros limiting the FS usage
#define NINODES     1024 /* Like Unix, fix the number of maximum i-nodes! 2^18 */
/* TODO: remove this limit; use imap. */
// #define BMAP_BYTES  4096 /* Number of entries in freeblock. This number multiply by 8 is the total number of blocks */
#define BMAP_BYTES  786432 /* Number of entries in freeblock. This number multiply by 8 is the total number of blocks */
#define NFILE       1024 /* Number of max open files by all processes sharing this FS. */
#define NOFILE      100  /* Number of max open files by a single process. */

#define LFS_BLOCKSIZE 512 /* Size of block. Don't change it!! Many places assume this block size. */
#define LFS_PAGESIZE  4096 /* Size of page */
#define LFS_PAGEMASK  4095 /* 0x0...0 1111 1111 1111 */




#endif //LIBFS_PARAM_H
