//
// Created by Ding Yuan on 2018-01-02.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "lfs.h"
#include "lfs_error.h"
#include "inode.h"
#include "file.h"
#include "sync.h"

int lfs_init (void *addr, int flag, int user_alloc_size) {
    lfs_error = 0;
    assert (sizeof (struct lfs_super) == LFS_BLOCKSIZE); // DEBUG purpose.
    if (root_addr != NULL) { // already initialized
        lfs_error = LFS_EALREADY;
        return -1;
    }
    // Check that addr is in a valid range
    /* if ((uintptr_t) addr < (uintptr_t) INIT_BRK) {
        lfs_error = LFS_EFAULT;
        return -1;
    } */

    if ((uintptr_t) addr & (uintptr_t) LFS_PAGEMASK) { // addr should be page-aligned
        lfs_error = LFS_EALIGN;
        return -1;
    }

    if (flag == LFS_FORMAT) {
        // Total size of the initial FS. Consists of:
        //  - A super block
        //  - struct file x NFILE
        //  - Free block map
        //  - inodes
        //  - Data blocks
        bmutex_pid = 0;
#ifdef USER_ALLOCATE_SPACE
        int total_size = user_alloc_size;
        assert ((total_size & LFS_PAGEMASK) == 0); // total size should be a multiple of page size
#else
        assert (addr == MYSBRK(0)); // Currently assume that the root addr needs to be the same as current break;
        int total_size = sizeof(struct lfs_super) + sizeof(struct file) * NFILE + BMAP_BYTES + sizeof(inode_t) * NINODES;
        total_size += next_alloc_size();
        total_size = PAGEALIGN_ROUNDUP(total_size);

        assert ((total_size & LFS_PAGEMASK) == 0); // total size should be a multiple of page size
        p = MYSBRK(total_size);
        if (p == (void*) -1) {
            lfs_error = LFS_EENV;
            return -1;
        }
#endif
        root_addr = addr;
        init_superblock(total_size);
        nonbiased_lock (&((struct lfs_super *) root_addr)->super_futex);
        init_sfile();
        init_freemap();
        init_inodes();
        mkrootdir(); // create the first directory -- root!
        init_user();
        nonbiased_unlock(&((struct lfs_super *) root_addr)->super_futex);
        return 0;
    } else if (flag == LFS_INIT) {
        struct lfs_super *p = (struct lfs_super *) addr;
        nonbiased_lock(&p->super_futex);
        assert(root_addr==NULL);
        assert (p->s_magic==LFS_MAGIC);
        root_addr = addr;
        if (p->nproc == 63) {
            lfs_error = LFS_ETOOMANY;
            nonbiased_unlock(&p->super_futex);
            return -1;
        }
        bmutex_pid = p->nproc;
        p->nproc++;
        nonbiased_unlock(&p->super_futex);
        return 0;
    } else {
        lfs_error = LFS_EINVAL;
        return -1;
    }
}

void lfs_printsuper() {
    struct lfs_super *p = (struct lfs_super *) root_addr;
    nonbiased_lock(&p->super_futex);
    printf("Super block: size = %lu\n", sizeof (struct lfs_super));
    printf("super->s_magic: 0x%08x\n", p->s_magic);
    printf("super->s_nblocks: %d\n", p->s_nblocks);
    printf("super->s_endaddr (relative): %p\n", (void*) p->s_endaddr);
    printf("super->nproc: %d\n", p->nproc);
    printf("bmutex_pid: %d\n", bmutex_pid);
    nonbiased_unlock(&p->super_futex);
    return;
}

/* Assume caller holds the superblock->futex. */
void init_freemap () {
    uint8_t *freemap = (uint8_t *)(FREEMAP_START(root_addr));
    for (int i = 0; i < BMAP_BYTES; i++) { freemap[i] = (uint8_t) 0; }
    return;
}

/* Assume caller holds the superblock->futex. */
void init_sfile() {
    struct file *files = (struct file *)SFILE_START(root_addr);
    for (int i = 0; i < NFILE; i++) {
        biased_lock_init(&files[i].f_bmutex);
        files[i].f_flag = 0;
        files[i].f_count = 0;
        files[i].f_inode = 0;
        files[i].f_offset = 0;
    }
    return;
}

/* Assume caller holds the superblock->futex. */
void init_superblock (int fs_size) {
    struct lfs_super *p = (struct lfs_super *) root_addr;
    p->s_magic = LFS_MAGIC;
    p->super_futex = 0;
    biased_lock_init(&p->filelist_bmutex);
    biased_lock_init(&p->bitmap_bmutex);
    biased_lock_init(&p->inodelist_bmutex);
    p->s_endaddr =  (rptr_t) fs_size;
#ifndef USER_ALLOCATE_SPACE 
    assert (REL2ABS(p->s_endaddr) == MYSBRK(0));
#endif
    p->s_nblocks = (p->s_endaddr / LFS_BLOCKSIZE);
    // p->next_inode = 1; // We start at inode #1; inode 0 is not a valid inode number.
    p->next_block = 0;
    p->nproc = 1;
    return;
}

/* Assume caller holds the superblock->futex. */
void init_inodes() {
    inode_t *inodes= (inode_t *)(INODES_START(root_addr));
    inode_t *p = NULL;
    if (LFS_DEBUG) {
        printf("Root addr: %p, starting addr of inode: %p\n", root_addr, inodes);
    }
    for (p = inodes; p < &inodes[NINODES]; p++) {
        //if (LFS_DEBUG)
        //    printf("Init inode: %p\n", p);
        biased_lock_init(&p->i_bmutex);
        //p->i_flag  = (char) 0;
        p->i_count = (char) 0;
        p->i_number = 0;
        p->i_mode = 0;
        p->i_nlink = 0;
        p->i_uid = 0;
        p->i_gid = 0;
        p->i_size1 = 0;
        p->i_direntries = 0;
        p->i_mtime = 0;
        for (int i = 0; i < 15; i++) {
            p->i_addr[i] = 0;
        }
    }
    return;
}

/* Assume caller holds the superblock->futex. */
void init_user () {
    inode_t *inodes = (inode_t *) INODES_START(root_addr);
    biased_lock_init(&u.u_bmutex);
    u.u_uid = 0; // TODO
    u.u_gid = 0; // TODO
    for (int i = 0; i < NOFILE; i++)
        u.u_ofile[i] = NULL;
    u.u_cdir = (void *) &inodes[1];
    u.u_cdirStr[0] = '/';
    for (int i = 1; i < 256; i++)
        u.u_cdirStr[i] = '\0';
}


int next_alloc_size() {
#ifdef USER_ALLOCATE_SPACE
    panic("next_alloc_size should not be called on USER_ALLOCATE_SPACE");
#endif
    // static int nblocks = 4;
    static int nblocks = MAX_BLOCKS*2; // Change it back to 4 when moved to PERSISTENT
    if (nblocks < 2048) {
        nblocks = nblocks * 2;
    }
    return nblocks * LFS_BLOCKSIZE;
}

void panic (const char *format, ...) {
    va_list arg;
    va_start (arg, format);
    vprintf(format, arg);
    va_end(arg);

    exit(1);
}

void lfs_handoff_bias_mutex() {
    struct lfs_super *p = (struct lfs_super *) root_addr;
    nonbiased_lock(&p->super_futex);
    assert(p->nproc > 0);
    p->nproc--;
    nonbiased_unlock(&p->super_futex);
    return;
}

void lfs_reacquire_bias_mutex() {
    struct lfs_super *p = (struct lfs_super *) root_addr;
    nonbiased_lock(&p->super_futex);
    assert(p->nproc < 63);
    p->nproc++;
    nonbiased_unlock(&p->super_futex);
    return;
}
