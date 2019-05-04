//
// Created by Ding Yuan on 2018-01-10.
//

// inode.c: implement functions handling inodes

#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lfs.h"
#include "inode.h"
#include "sync.h"
#include "lfs_error.h"
#include "bitmap.h"
#include "file.h"
void stat_copy (inode_t *ip, struct lfs_stat *buf) ;
/*
 * Make a new inode.
 * Dead code.
 */
/*inode_t *maknode (uint16_t mode) {
    inode_t *ip = ialloc(); // find an unused inode.
    if (ip == NULL) {
        return ip;
    }
    // No need to lock, right?
    ip->i_mode = mode;
    // ip->i_nlink = 1;  // (will be) linked into a dir; otherwise why do you want to create it
    ip->i_uid = u.u_uid; // Global
    ip->i_gid = u.u_gid; // Global
    return (ip);
}*/

/*
 * Creates a new link (also known as a hard link) to an existing file.
 *
 * If newpath exists, it will not be overwritten.
 *
 * RETURN VALUE:On success, zero is returned.  On error, -1 is returned, and errno is set appropriately.
 * ERRORS:
 * LFS_EACCES Write access to the directory containing newpath is denied, or search permission is denied for one
 *            of the directories in the path prefix of oldpath or newpath.
 * LFS_EEXIST newpath already exists.
 * LFS_EMLINK The file referred to by oldpath already has the maximum number of links to it.
 * LFS_ENAMETOOLONG oldpath or newpath was too long.
 * LFS_ENOENT A directory component in oldpath or newpath does not exist
 * LFS_ENOSPC The device containing the file has no room for the new directory entry.
 * LFS_ENOTDIR A component used as a directory in oldpath or newpath is not, in fact, a directory.
 *
 */
int lfs_link(const char *oldpath, const char *newpath) {
    inode_t *ip = NULL;
    inode_t *parent_ip = NULL;
    struct namei_data ndata;
    ip = namei(oldpath, NSEARCH, &ndata);
    if (ip == NULL) {
        lfs_error = ndata.error;
        return -1;
    }

    if (ndata.parent_ip != NULL)
        iunlock(ndata.parent_ip); // We do not need to touch the parent dir of oldpath

    if (ip->i_nlink >= 127) {
        lfs_error = LFS_EMLINK;
        goto out0;
    }

    if ((ip->i_mode&IFMT) == IFDIR) {
        lfs_error = LFS_EPERM;
        goto out0;
    }


    iunlock(ip);// in case oldpath is a prefix of new path, avoid double locking
    parent_ip = namei(newpath, NCREATE, &ndata);
    if (parent_ip == NULL) { // Error, might be already exists
        lfs_error = ndata.error;
        return -1;
    }

    // At this point, the dir containing the newpath (parent_ip), and its parent, are locked.
    // We attempt to lock oldpath (ip). Double-locking (i.e., ip==parent_ip or its parent) is
    // impossible, b/c old path is not a dira
    assert (ip!=parent_ip);
    assert (ip!=ndata.parent_ip);

    ilock(ip);
    if (ndata.parent_ip != NULL)
        iunlock(ndata.parent_ip); // We don't need to touch the grandparent dir of new path
    // At this point, we have parent_ip pointing to the parent directory of the new path
    int newpathlen = 0;
    // First, sanity check, whether the new pathname is valid
    assert (ndata.cp != NULL);
    ndata.cp--; // skip the trailing '\0'
    while (*(ndata.cp) == '/')
        ndata.cp--; // remove trailing /

    while (*ndata.cp != '/' && newpathlen < LFS_NAMELEN) {
        ndata.cp--;
        newpathlen++;
    }

    if (newpathlen >= LFS_NAMELEN) {
        lfs_error = LFS_ENAMETOOLONG;
        goto out1;
    }
    ndata.cp++; // skip the '/'; now cp points to the start of the new file name

    ip->i_nlink++; // Do it before adding the entry. So that if it crashed before the
                   // entry is added to parent dir, we can decrement the i_nlink
    /******************* mfence **************************/
    asm volatile ("mfence" ::: "memory");
    /*****************************************************/

    if (wdir(parent_ip, ndata.cp, ip->i_number) != 0) {
        goto out1; // lfs_error is properly set by wdir
    }
    parent_ip->i_mtime = current_time();

    iunlock(parent_ip);
    iunlock(ip);
    return 0;

out1:
    iunlock(parent_ip);
out0:
    iunlock(ip);
    return -1;
}

int lfs_rename (const char *oldpath, const char *newpath) {
    inode_t *oldip = NULL;
    inode_t *np = NULL; // inode pointer to new path or parent
    struct namei_data olddata, newdata;

    oldip = namei(oldpath, NDELETE, &olddata);
    if (oldip == NULL) {
        lfs_error = olddata.error;
        return -1;
    }
    // At this point, the inodes to both oldpath and its parent dir are locked.
    if (oldip->i_nlink >= 127) {
        lfs_error = LFS_EMLINK;
        goto out0;
    }

    // Releasing the locks here b/c they might be needed in the search of new path.
    // Will there be a race?
    if (olddata.parent_ip != NULL)
        iunlock(olddata.parent_ip);
    iunlock(oldip);

    np = namei(newpath, NCREATE, &newdata);
    if (np != NULL) { // the new path does NOT exist, add it
        // At this point:
        //  - np points to the directory that contains newpath;
        //  - newdata.parent_ip points to newpath's parent directory
        //  - oldip points to the oldpath
        //  - olddata.parent_ip points to the parent dir of the oldpath
        //
        // np, newdata.parent_ip are locked by namei. Clearly we do not
        // need to lock newdata.parent_ip.
        if (newdata.parent_ip != NULL)
            iunlock(newdata.parent_ip); // we are not touching the grand parent dir of newpath

        // First, check that oldpath is not on the path leading to newpath
        // E.g., this should not happen: oldpath: /1/2/, newpath: /1/2/3/
        // Note that we do not need to check newpath is a prefix of oldpath b/c newpath simply doesn't exist
        if ((oldip->i_mode & IFMT) == IFDIR && isPrefix (oldip, np)) {
            lfs_error = LFS_EINVAL;
            iunlock(np);
            return -1;
        }

        // Now we need to lock the oldpath and the directory containing oldpath.
        // Note that it is impossible to double-lock: i.e., we already locked np,
        // and it is not possible that, at this point (after isPrefix test), either
        // oldip or olddata.parent_ip is the same as np.
        assert (np != oldip);
        // assert (np != olddata.parent_ip);
        assert(olddata.parent_ip != NULL);
        ilock(oldip);
        if (np != olddata.parent_ip) // they could be the same: rename /dir1/a.txt to /dir1/a_rename.txt
            ilock(olddata.parent_ip);

        assert (olddata.direntp->i_number == oldip->i_number);
        assert (*newdata.cp == '\0');
        newdata.cp--;
        while (*newdata.cp == '/')
            newdata.cp--;
        while (*newdata.cp != '/')
            newdata.cp--;

        newdata.cp++; // skip the '/'
        if (wdir(np, newdata.cp, oldip->i_number) != 0)
            goto out1;

        np->i_mtime = current_time();
        // We do not update the mtime of oldpath (pointed by oldip)

        // This fence make sure that the new entry is added before we remove the old
        /******************* mfence **************************/
        asm volatile ("mfence" ::: "memory");
        /*****************************************************/

        // Remove the old entry
        olddata.direntp->i_number = 0;
        olddata.parent_ip->i_direntries--;
        /*
         * This fence makes sure that the old entry is properly removed before we
         * update the timestamp. If it crashed somewhere here and we end up with more dir
         * pointing to an inode than its i_nlinks, we can use the timestamp to determine
         * which one we should keep and which to remove during recovery.
         */
        /******************* mfence **************************/
        asm volatile ("mfence" ::: "memory");
        /*****************************************************/

        olddata.parent_ip->i_mtime = current_time();

        if (np != olddata.parent_ip)
            iunlock(olddata.parent_ip);
        iunlock(oldip); // Can we release this lock earlier?

        iunlock(np);
        return 0;
    }

    // the previous namei returned NULL
    ////// Locking: at this point, nothing is locked //////
    if (newdata.error != LFS_EEXIST) {
        lfs_error = newdata.error;
        return -1; // Only oldip and olddata.parent_ip are locked
    }

    // the new path already exists, replace it.
    np = namei(newpath, NSEARCH, &newdata);
    if (np == NULL) {
        // Shouldn't happen
        panic("in rename, newpath existed in the first time search, but disappeared in the second time search! newpath: %s\n", newpath);
    }

    // At this point:
    //  - np points to the newpath (already exists)
    //  - newdata.parent_ip points to the parent dir of new path
    //  - oldip points to the oldpath
    //  - olddata.parent_ip points to the parent dir of the oldpath
    if (np->i_number == oldip->i_number) {
        iunlock(np);
        if (newdata.parent_ip != NULL)
            iunlock(newdata.parent_ip);
        return 0; // same file, do nothing
    }

    ilock(oldip);
    assert(olddata.parent_ip != NULL);
    ilock(olddata.parent_ip);

    if ((np->i_mode & IFMT) == IFDIR) {
        // newpath is a dir; First, it must be empty
        if (np->i_direntries > 2) {
            lfs_error = LFS_ENOTEMPTY;
            goto out1;
        }

        // Second, oldpath needs also be a dir
        if ((oldip->i_mode & IFMT) != IFDIR) {
            lfs_error = LFS_EISDIR; // newpath is an existing dir, but oldpath is not a dir
            goto out1;
        }
        // At this moment, newpath is an empty dir, and oldpath is a dir.
        // Finally, check that oldpath is not a prefix of newpath
        if (isPrefix (oldip, np)) {
            lfs_error = LFS_EINVAL;
            goto out1;
        }
    } else {
        // both has to be regular file
        if ((oldip->i_mode & IFMT) == IFDIR) {
            lfs_error = LFS_ENOTDIR; //  old path is a dir, but newpath exists but is not a dir
            goto out1;
        }

        // At this point, newpath and oldpath are regular files. Note we don't need to check prefix here b/c neither
        // is a directory.
    }

    // Replace the original path w/ the newpath. Name of the entry: olddata.direntp->name
    assert (olddata.direntp->i_number == oldip->i_number);
    newdata.direntp->i_number = oldip->i_number;
    newdata.parent_ip->i_mtime = current_time();

    /*
     * This fence makes sure that the entry now points to the new inode, before we can free
     * (or decrease the nlink) of the old inode, and it will be potentially removed.
     */
    /******************* mfence **************************/
    asm volatile ("mfence" ::: "memory");
    /*****************************************************/

    // remove oldpath from its parent dir
    olddata.direntp->i_number = 0;
    olddata.parent_ip->i_direntries--;
    olddata.parent_ip->i_mtime = current_time();

    np->i_nlink--;
    if (np->i_nlink == 0)
        itrunc (np); // np is locked!

    if (newdata.parent_ip != NULL)
        iunlock(newdata.parent_ip);
    iunlock(np);

    if (olddata.parent_ip != NULL)
        iunlock(olddata.parent_ip);
    iunlock(oldip);

    // No need to decrement the oldpath's i_nlink
    return 0;

out1:
    if (newdata.parent_ip != NULL)
        iunlock(newdata.parent_ip);
    iunlock(np);

out0:
    if (olddata.parent_ip != NULL)
        iunlock(olddata.parent_ip);
    iunlock(oldip);
    return -1;
}

/*
 *  Delete a name and possibly the file it refers to
 *  Return value: 0 on success, and -1 on error.
 *  Note that it will only free resources of the file/dir of the pathname. It won't recursively go into the
 *  dir to subsequently remove the files.
 *
 */
int lfs_unlink (const char *pathname) {
    inode_t *ip;
    struct namei_data ndata;
    ip = namei(pathname, NDELETE, &ndata);

    if (ip == NULL) {
        lfs_error = ndata.error;
        return -1;
    }

    if ((ip->i_mode&IFMT)==IFDIR) {
        lfs_error = LFS_EISDIR;
        goto out;
    }

    if (_access(ip, IWUSR)) {
        lfs_error = LFS_EACCES;
        goto out;
    }
    assert(ip->i_number==ndata.direntp->i_number);
    ndata.direntp->i_number = 0; // And that's it!! it is removed from the parent dir!! We don't try to reclaim the dirent entry.
    ndata.parent_ip->i_direntries--; // decrease the number of entries
    ndata.parent_ip->i_mtime = current_time();

    /******************* mfence **************************/
    asm volatile ("mfence" ::: "memory");
    /*****************************************************/

    // Note: we're not changing the i_size1 of the parent!
    ip->i_nlink--;
    if (ip->i_nlink == 0)
       itrunc(ip); // We can use itrunc to free the blocks of a regular file

    iunlock(ndata.parent_ip);
    iunlock(ip);
    return 0;

out:
    if (ndata.parent_ip != NULL)
        iunlock(ndata.parent_ip);
    iunlock(ip);
    return -1;
}

int lfs_rmdir (const char *pathname) {
    inode_t *ip;
    struct namei_data ndata;
    ip = namei(pathname, NDELETE, &ndata);

    if (ip == NULL) {
        lfs_error = ndata.error;
        return -1;
    }

    if ((ip->i_mode&IFMT)!=IFDIR) {
        lfs_error = LFS_ENOTDIR;
        goto out;
    }

    if (ip->i_direntries > 2) {
        lfs_error = LFS_ENOTEMPTY;
        goto out;
    }

    if (_access(ip, IWUSR)) {
        lfs_error = LFS_EACCES;
        goto out;
    }

    assert(ip->i_number==ndata.direntp->i_number);
    ndata.direntp->i_number = 0; // And that's it!! it is removed from the parent dir!! We don't try to reclaim the dirent entry.
    ndata.parent_ip->i_direntries--; // decrease the actual size of dir
    ndata.parent_ip->i_mtime = current_time();

    /******************* mfence **************************/
    asm volatile ("mfence" ::: "memory");
    /*****************************************************/

    ip->i_nlink--;
    if (ip->i_nlink == 0)
        itrunc(ip); // We can use itrunc to free the blocks

    iunlock(ndata.parent_ip);
    iunlock(ip);
    return 0;

out:
    if (ndata.parent_ip != NULL)
        iunlock(ndata.parent_ip);
    iunlock(ip);
    return -1;
}


/*
 * Search for the next unused inode.
 */
inode_t *ialloc() {
    struct lfs_super *p = (struct lfs_super *) root_addr;
    inode_t *inodes = (inode_t *) INODES_START(p);
    biased_lock(&p->inodelist_bmutex);
    /*if (p->next_inode < NINODES) {
        inode_t *retp = &(inodes[p->next_inode]);
        retp->i_number = p->next_inode;
        p->next_inode = p->next_inode + 1;
        assert (retp->i_nlink == 0); // it is not allocated!
        writelock_release(&p->inodelist_futex);
        return retp;
    }*/
    // At this point, we run out of unused inodes; need to start from the beginning of the inodes array and linearly
    // search for an inode that has been freed.
    // Note that inode 0 is not a valid inumber!
    for (int i = 1; i < NINODES; i++) {
        if (inodes[i].i_nlink == 0) {
            biased_lock_init(&inodes[i].i_bmutex);
            inodes[i].i_nlink = 1;
            inodes[i].i_number = i;
            inodes[i].i_count = 0;
            inodes[i].i_uid = 0;
            inodes[i].i_gid = 0;
            inodes[i].i_size1 = 0;
            inodes[i].i_direntries = 0;
            inodes[i].i_mtime = 0;
            for (int j = 0; j < 15; j++)
                assert(inodes[i].i_addr[j] == 0);
            biased_unlock(&p->inodelist_bmutex);
            return &inodes[i];
        }
    }
    biased_unlock(&p->inodelist_bmutex);
    return NULL; // Failed, run out of inode!!
}

int mkrootdir () {
    inode_t *inodes = (inode_t *) INODES_START(root_addr);

    // inode[0] should always be the inode of /
    inode_t *ip = ialloc();
    assert (ip == &(inodes[1]));
    ilock(ip);
    ip->i_mode = (IFDIR | IRUSR | IWUSR | IXUSR | IRGRP | IXGRP | IROTH | IXOTH);
    // ip->i_nlink = 1;
    ip->i_uid = u.u_uid; // Global
    ip->i_gid = u.u_gid; // Global

    wdir (ip, ".", 1); // write the first entry, ".", whose inumber is 1
    wdir (ip, "..", 1); // write the first entry, ".", whose inumber is 1
    ip->i_mtime = current_time();
    iunlock(ip);
    return 0;
}

int lfs_stat(const char *pathname, struct lfs_stat *buf) {
    struct namei_data ndata;
    inode_t *ip = namei(pathname, NSEARCH, &ndata);
    if (ip == NULL) {
        lfs_error = ndata.error;
        return -1;
    }
    if (ndata.parent_ip != NULL)
        iunlock(ndata.parent_ip);
    stat_copy(ip, buf);
    iunlock(ip);
    return 0;
}

int lfs_fstat(int fd, struct lfs_stat *buf) {
    if (validate_fd(fd) != 0)
        return -1;

    struct file *fp = (struct file *)u.u_ofile[fd];

    inode_t *ip = (inode_t*)REL2ABS(fp->f_inode);
    ilock(ip);
    stat_copy(ip, buf);
    iunlock(ip);
    return 0;
}

int lfs_lstat(const char *pathname, struct lfs_stat *buf) {
    return lfs_stat(pathname, buf);
}

/* Common code for stat and fstat.
 * ASSUME CALLER locks ip*/
void stat_copy (inode_t *ip, struct lfs_stat *buf) {
    buf->st_ino = ip->i_number;
    buf->st_mode = ip->i_mode;
    buf->st_nlink = ip->i_nlink;
    buf->st_uid = ip->i_uid;
    buf->st_gid = ip->i_gid;
    buf->st_size = ip->i_size1;
    buf->st_modtime = ip->i_mtime;
    return;
}
/*
 * Write a directory entry: i_number fname, into the directory whose inode is in ip
 *
 * Assume strlen(fname) < LFS_NAMELEN!
 *
 * Returns 0 if success.
 * -1: error b/c cannot allocate block
 *
 * CALLER SHOULD HOLD lock on the inode, except for those new ones that are not linked into the fs yet!
 */
int wdir (inode_t *ip, const char *fname, uint32_t i_number) {
    assert (ip != NULL);
    // assert(islocked(ip)); // locked
    uint16_t offset = ip->i_size1 & 0777;

    uint16_t bn = ip->i_size1 >> 9; // unsigned right shift
    // assert (bn < 11); // TODO: support larger files

    rptr_t *bpp = get_block_ptr_addr(ip,bn);
    if (offset == 0) {
        // We are at block boundary
        void *bp = allocate_block();
        if (bp == NULL) {
            // Should return -1; panic right now
            panic("Cannot allocate block from in wdir");
        }
        assert (bpp != NULL);
        assert (*bpp == 0);
        *bpp = (rptr_t)ABS2REL(bp);
    }

    assert (ip->i_size1 % sizeof(struct libfs_dirent) == 0);

    struct libfs_dirent *direntp = (struct libfs_dirent *)((uintptr_t)REL2ABS(*bpp) + offset);

    /* Copy the pathname */
    int i;
    for (i = 0; fname[i] != '\0' && fname[i] != '/'; i++) {
        direntp->name[i] = fname[i];
    }
    while (i < LFS_NAMELEN) {
        direntp->name[i++] = '\0';
    }
    /******************* mfence **************************/
    asm volatile ("mfence" ::: "memory");
    /*****************************************************/

    direntp->i_number = i_number;
    ip->i_size1 += sizeof(struct libfs_dirent);
    ip->i_direntries++;
    return 0;
}

/*
 * Allocate a block. Returns the pointer to the block, or NULL on error.
 */
void *allocate_block() {
    uint8_t *freemap = (uint8_t *)(FREEMAP_START(root_addr));
    uintptr_t blocks =  (uintptr_t)(BLOCKS_START(root_addr));
    struct lfs_super *p = (struct lfs_super *) root_addr;
    void *retp = NULL;

    biased_lock(&p->bitmap_bmutex);
    if (p->next_block < p->s_nblocks) {
        // Just allocate from the end
        retp = (void *)((uintptr_t)blocks + (uintptr_t)(p->next_block * LFS_BLOCKSIZE));
        assert(get_bit(freemap, p->next_block) == 0);
        zero_block(retp);
        set_bit (freemap, p->next_block);
        p->next_block++;
        biased_unlock(&p->bitmap_bmutex);
        return retp;
    }

    assert(p->next_block == p->s_nblocks);

    // We reach the end of allocated blocks. Now search from the start.
    for (int i = 0; i < p->s_nblocks; i++) {
        if (get_bit(freemap, i) == 0) {
            set_bit (freemap, i);
            zero_block((void*)(blocks + i * LFS_BLOCKSIZE));
            biased_unlock(&p->bitmap_bmutex);
            return (void *)(blocks + i * LFS_BLOCKSIZE);
        }
    }

#ifdef USER_ALLOCATE_SPACE 
    panic("SBRK has been changed by non-lfs code.");
#endif

    // So we are running out of blocks; need to allocate new blocks.
    uint32_t block_in_bytes = PAGEALIGN_ROUNDUP(next_alloc_size());
    void *new_bp = MYSBRK(0); // check
    if (new_bp != (void*)REL2ABS(p->s_endaddr)) {
        // Sth is seriously wrong
        panic("SBRK has been changed by non-lfs code.");
    }
    // assert (new_bp == p->s_endaddr);

    new_bp = MYSBRK(block_in_bytes);
    if (new_bp == (void *)-1) {
        biased_unlock(&p->bitmap_bmutex);
        return NULL; // Cannot SBRK
    }

    p->s_nblocks += (block_in_bytes >> 9);
    p->s_endaddr = (rptr_t)ABS2REL(new_bp);
    assert (p->next_block < p->s_nblocks);
    retp = (void *)((uintptr_t)blocks + (uintptr_t)(p->next_block * LFS_BLOCKSIZE));
    assert(get_bit(freemap, p->next_block) == 0);
    zero_block(retp);
    set_bit (freemap, p->next_block);
    p->next_block++;
    biased_unlock(&p->bitmap_bmutex);
    return retp;
}

/*
 * Zeroing a block
 */
void zero_block (void *bp) {
    uint8_t *p = (uint8_t *) bp;
    for (int i =0; i < LFS_BLOCKSIZE; i++) {
       p[i] = 0;
    }
    return;
}

/*
 * Make a directory; implementing lfs_mkdir.
 * Return values:
 *   0 on success.
 *   -1 on error.
 * Errors:
 *   LFS_EACCES: The parent directory does not allow write permission
 *   LFS_EEXIST: pathname already exists
 *   LFS_EINVAL: the final component of the pathname is invalid ( contains invalid chars)
 *   LFS_ENAMETOOLONG: pathname is too long
 *   LFS_ENOENT:  A directory component in pathname does not exist
 *   LFS_ENOMEM: Not enough memory
 *   LFS_ENOTDIR: A component used as a directory in pathname is not, in fact, a directory.
 *   LFS_ENOEXEC: One of the directories in pathname did not allow search permission (x permission)
 */
int lfs_mkdir (const char *pathname, uint16_t mode) {
    // search the parent dir, locate the inode

    int newdirlen = 0;
    struct namei_data ndata;
    inode_t *parent_ip = namei (pathname, NCREATE, &ndata);
    time_t cur_time = 0;

    if (parent_ip == NULL) { // error, could be LFS_EEXIST, LFS_ENOENT, LFS_
        lfs_error = ndata.error;
        return -1;
    }

    if (ndata.parent_ip != NULL)
        iunlock(ndata.parent_ip);  // We don't need to touch this inode/file

    // At this point, we have ip pointing to the parent directory of the new dir
    // First, sanity check, whether the new dir name is valid
    assert (ndata.cp != NULL);
    ndata.cp--;
    while (*ndata.cp == '/')
        ndata.cp--; // remove trailing /

    while (*ndata.cp != '/' && newdirlen < LFS_NAMELEN && ndata.cp != pathname) {
        ndata.cp--;
        newdirlen++;
    }

    if (newdirlen >= LFS_NAMELEN) {
        lfs_error = LFS_ENAMETOOLONG;
        iunlock(parent_ip);
        return -1;
    }
    if (*ndata.cp == '/') 
        ndata.cp++; // skip the '/'; now cp points to the start of the new dir name

    // allocate a new inode
    inode_t *new_ip = ialloc();
    if (new_ip == NULL) {
        lfs_error = LFS_ENOMEM; // Running out of inodes!!
        iunlock(parent_ip);
        return -1;
    }
    new_ip->i_mode = (IFDIR | IRUSR | IWUSR | IXUSR | IRGRP | IXGRP | IROTH | IXOTH);
    // new_ip->i_nlink = 1;
    new_ip->i_uid = u.u_uid; // Global
    new_ip->i_gid = u.u_gid; // Global

    // add "." and ".." into this new dir. Note that block will be allocated in wdir
    wdir (new_ip, ".", new_ip->i_number); // write the first entry, ".", whose inumber is 0
    wdir (new_ip, "..", parent_ip->i_number); // write the first entry, ".", whose inumber is 0
    cur_time = current_time();
    new_ip->i_mtime = cur_time;

    // insert the dir entry: <inode number, dirname> into parent dir
    wdir (parent_ip, ndata.cp, new_ip->i_number);
    parent_ip->i_mtime = cur_time;
    iunlock(parent_ip);
    return 0;
}

/*
 * Changes the current working directory of the calling process to the directory specified in path.
 *
 * On success, zero is returned.  On error, -1 is returned, and lfs_error is set appropriately.
 */
int lfs_chdir (const char *pathname) {
    if (pathname[0] != '/') {
        lfs_error = LFS_NOTABS;
        return -1;
    }

    if (strlen(pathname)>=256) {
        lfs_error = LFS_ENAMETOOLONG;
        return -1;
    }

    struct namei_data ndata;
    inode_t *ip = namei(pathname, NSEARCH, &ndata);
    if (ip == NULL) {
        lfs_error = ndata.error;
        return -1;
    }
    if (ndata.parent_ip != NULL)
        iunlock(ndata.parent_ip);
    iunlock(ip);

    biased_lock(&u.u_bmutex);
    u.u_cdir = ip;
    strcpy(u.u_cdirStr, pathname);
    biased_unlock(&u.u_bmutex);

    lfs_error = 0;
    return 0;
}

char *lfs_getcwd(char *buf, int size) {
    biased_lock(&u.u_bmutex);
    if (strlen(u.u_cdirStr) >= size) {
        biased_unlock(&u.u_bmutex);
        lfs_error = LFS_ERANGE;
        return NULL;
    }
    strcpy (buf, u.u_cdirStr);
    biased_unlock(&u.u_bmutex);

    lfs_error = 0;
    return buf;
}

int lfs_access(const char *pathname, int mode) {
    struct namei_data ndata;
    inode_t *ip = namei(pathname, NSEARCH, &ndata);

    if (ip == NULL) {
        lfs_error = ndata.error;
        return -1;
    }

    if (ndata.parent_ip != NULL)
        iunlock(ndata.parent_ip);
    iunlock(ip);
    return 0;
}

/*
 * given the pathname, walk the fs to search for the pathname.
 * flag = NSEARCH (0) if name is sought
 *  NCREATE (1) if name is to be created
 *  NDELETE (2) if name is to be deleted
 *
 * ndata is a pointer to a *local variable* in the caller. It is used to output various information.
 * On success, this should point to the end of pathname
 *
 * return values:
 *  if there is an error, then a null value is returned, and error is properly set.
 *  if flag == NSEARCH, the return value is the pointer to the inode that is to be searched.
 *  if flag == NCREATE, and if the named file does not exist, then it returns a pointer to the inode of the directory which
 *      will point to the new file.  (If the file exists then error is returned.)
 *      ndata.parentip points to the parent of this inode.
 *      E.g., when pathname = /dir1/dir2/f, returned inode points to /dir1/dir2/, ndata.parentip points to /dir1/
 *  If flag == NDELETE, the value returned is an "inode" pointer for the pathname, and
 *      parent_ip properly points to the parent directory of the named file.
 *
 * Locking:
 *  - When non-null is returned, the return inode and the (non-null) parent inode will be locked
 *  - When NULL is returned, nothing is locked!
 *
 * Errors:
 *   LFS_EEXIST:  already exist
 *   LFS_ENOENT:  no such entry
 *   LFS_ENOTDIR: a component in path should be dir but it is not
 *   LFS_ENOEXEC: cannot cd into the directory b/c it lacks X permission
 */
inode_t *namei (const char *pathname, int flag, struct namei_data *ndata) {
    inode_t *dp;
    inode_t *inodes = (inode_t *) INODES_START(root_addr);
    assert (pathname != NULL);
    void *bp = NULL;

    assert(ndata != NULL);
    ndata->error = 0;
    ndata->cp = pathname;
    ndata->parent_ip = NULL;
    ndata->direntp = NULL;

    dp = (inode_t *) u.u_cdir;
    if (*(ndata->cp) == '/')
        dp = &inodes[1]; // inodes[1] is always the inode of root

    while (*(ndata->cp) == '/')
        (ndata->cp)++;
    if (*(ndata->cp)=='\0' && flag != 0) { // You cannot create or delete /
        ndata->error = LFS_EEXIST;
        return NULL;
    }

    while (1) { // Each iteration analyzes a component of pathname
        ilock(dp);
        if (ndata->error)
            return NULL;
        if (*(ndata->cp) == '\0') {
            if (flag == NCREATE) {
                ndata->error = LFS_EEXIST; // the entry already exist!!
                goto out;
            }
            return dp; // success!
        }

        /*
         * If there is another component,
         * dp must be a directory and
         * must have x permission
         */
        if ((dp->i_mode & IFMT) != IFDIR) {
            ndata->error = LFS_ENOTDIR;
            goto out;
        }

        if ((dp->i_mode & IXUSR) == 0) {
            ndata->error = LFS_ENOEXEC;
            goto out;
        }


        int dir_entries = dp->i_size1 / (sizeof (struct libfs_dirent)); // size of the file divided by the size of each dir entry
        uint32_t next_blk = 0;    // the next block, of the (dir) file pointed by inode dp, we are about to access
        uint16_t dir_offset = 0;  // address offset within the dir file block

        /*
         * This loop will iterate through every entry in the directory pointed by dp, and compare against the next
         * component of path.
         */
        while (1) {
            /*
             * If at the end of the directory,
             * the search failed. Report what
             * is appropriate as per flag.
             */
            if (dir_entries == 0) {
                if (flag == NCREATE) {
                    // create, check if the path component is the last one
                    while (*(ndata->cp) != '/' && *(ndata->cp) != '\0')
                        (ndata->cp)++;
                    while (*(ndata->cp) == '/')
                        (ndata->cp)++;
                    if (*(ndata->cp)=='\0') { // it is the last component, return success!
                        return dp;
                    } // else, fall through and return error.
                }
                // error in other cases
                ndata->error = LFS_ENOENT;
                goto out;
            }

            /*
             * If offset is on a block boundary,
             * read the next directory block.
             * Release previous if it exists.
             */

            if ((dir_offset&0777) == 0) {
                bp = bread(dp, next_blk); // return the next block, advance next_blk
                dir_offset = 0;
            }

            ndata->direntp = (struct libfs_dirent *) ((uintptr_t)bp + dir_offset);
            dir_offset += sizeof(struct libfs_dirent);
            dir_entries--;

            if (ndata->direntp->i_number == 0) {
                continue; // i_number 0: invalid, the dir entry was deleted.
            }
            // Now, search the directory
            int match = 1;
            int i = 0;
            for (; ndata->direntp->name[i] != '\0'; i++) {
                if (ndata->cp[i] != ndata->direntp->name[i]) {
                    // mismatch,
                    match = 0;
                    break;
                }
            }
            if (match) {
                if (ndata->cp[i] != '/' && ndata->cp[i] != '\0') {
                    match = 0;
                }
            }
            if (match) { // now we found the match
                ndata->cp += i;
                while (*(ndata->cp) == '/') { (ndata->cp)++; }
                assert (ndata->direntp->i_number < NINODES);
                if (ndata->parent_ip != NULL)
                    iunlock(ndata->parent_ip);

                ndata->parent_ip = dp;
                dp = &inodes[ndata->direntp->i_number]; // Do we need inode_list lock here?
                break; // matched
            }
        }
    }

out:
    iunlock(dp);
    if (ndata->parent_ip != NULL)
        iunlock(ndata->parent_ip);
    return NULL;
}

/*
 * Given the inode and the id of the next block to read, return a pointer to that block.
 *
 * Assume that caller holds lock!!
 */
void *bread (inode_t *dp, uint32_t next_blk) {
    rptr_t *p = get_block_ptr_addr(dp, next_blk);
    assert (islocked(dp));
    assert (p != NULL);
    return (void*)REL2ABS(*p);
}

/*
 * Check if the directory pointed by shortip is on the path leading to the file pointed
 * to by longip. Return 1 if it is prefix, 0 otherwise.
 *
 * No synchronization as both the lock is held for shortip and longip by the caller.
 * TODO: could there be a race on intermediate dirs?
 */
int isPrefix (inode_t *shortip, inode_t *longip) {
    assert ((shortip->i_mode & IFMT) == IFDIR);
    assert ((longip->i_mode & IFMT) == IFDIR);
    inode_t *ip = longip;
    inode_t *inodes = (inode_t *) INODES_START(root_addr);

    while (ip->i_number != 1) { // terminate when we reach the root
        if (ip->i_number == shortip->i_number)
            return 1;
        struct libfs_dirent *dp = (struct libfs_dirent *) ((uintptr_t)REL2ABS(ip->i_addr[0]) + sizeof(struct libfs_dirent));
        assert (dp->name[0] == '.');
        assert (dp->name[1] == '.');
        assert (dp->name[2] == '\0');
        ip = &inodes[dp->i_number];
    }
    return 0;
}

/*
 * I am not putting any locking in this function. It is meant for debugging purpose anyway.
 */
void lfs_dump(uint16_t inumber, char *namebuf, int endidx) {
    assert (inumber != 0);
    if (inumber == 1)
        printf ("Name: /, I-number: 1, ");
    inode_t *inodes = (inode_t *) INODES_START(root_addr);
    inode_t *ip = &inodes[inumber];

    if ((ip->i_mode & IFMT) == IFDIR)
        printf ("TYPE: DIR, ");
    else
        printf ("TYPE: REG, ");

    printf ("i_nlink: %d, i_size1: %d, i_direntries: %d, ", ip->i_nlink, ip->i_size1, ip->i_direntries);

    // for (uint16_t i=0; i <= (ip->i_size1 >> 9); i++)
    //    printf ("i_addr[%d]: %p, ", i, ip->i_addr[i]);

    printf ("\n");

    if ((ip->i_mode & IFMT) == IFDIR) {
        // printf ("Dir content: \n");
        uint32_t max_bn = ip->i_size1 >> 9;
        uint16_t max_offset = ip->i_size1 & 0777;
        assert (max_bn<11);
        for (uint32_t bn = 0; bn <= max_bn; bn++) {
            uint16_t offset = 0;
            do {
                if (bn==max_bn && offset >= max_offset)
                    break; // done
                rptr_t *bpp = get_block_ptr_addr(ip, bn);
                assert(*bpp!=0);
                struct libfs_dirent *direntp = (struct libfs_dirent *)((uintptr_t)REL2ABS(*bpp) + offset);

                offset += sizeof(struct libfs_dirent);
                // printf ("%d, %s\n", direntp->i_number, direntp->name);
                if (direntp->i_number == 0)
                    continue;
                if (direntp->name[0] == '.' && (direntp->name[1]=='\0' || direntp->name[1]=='.'))
                    continue;
                printf ("Name: %s/%s, I-number: %d, ", namebuf, direntp->name, direntp->i_number);
                int i;
                namebuf[endidx] = '/';
                for (i = 0; direntp->name[i] != '\0'; i++) {
                    namebuf[endidx+i+1] = direntp->name[i];
                }
                namebuf[endidx+i+1] = '\0';
                lfs_dump(direntp->i_number, namebuf, endidx+i+1);
                namebuf[endidx] = '\0';
            } while (offset & 0777);
        }
    }
    return;
}

/*
 * Given the block number, return the absolute address of that data block.
 * If the data block is not allocated, this function will allocate a block */
void *get_block_abs_addr (inode_t *ip, uint32_t bn) {
    rptr_t *bpp = get_block_ptr_addr(ip, bn);
    assert(bpp != NULL);
    if (*bpp == 0) {
        // The data block has not been allocated; Allocate it!
        void *retptr = allocate_block();
        if (retptr == NULL) {
           panic ("Cannot allocate data block!");
        }
        *bpp = (rptr_t) ABS2REL(retptr);
    }
    assert(*bpp != 0);
    return (void *) REL2ABS(*bpp);
}

/*
 * Given the block number, return the (absolute) address of *block pointer*.
 * For example, if bn < 12, it returns &ip->i_addr[bn];
 * if bn > 12, it returns the address in the singly/doubly/triply indirect block
 * that should store the address of that block.
 *
 * NOTE:
 *  - since the block addr is a relative address, the caller should convert
 *    the return value into a absolute address.
 *  - If the corresponding singly/doubly/triply indirect blocks have not been
 *    allocated, it will allocate it!
 *     - ** However, it does NOT allocate the data block! Caller should allocate data block!!
 *
 * ASSUME CALLER HOLDS THE LOCK ON ip.
 */
rptr_t *get_block_ptr_addr(inode_t *ip, uint32_t bn) {
    uint32_t bn_offset, singly_offset, doubly_offset, triply_offset;
    rptr_t *singly_blk_addr, *doubly_blk_addr;
    if (bn < DIRECT_LIMIT)
        return &ip->i_addr[bn];

    if (bn < SINGLY_LIMIT) {
        // Singly indirect
        if (ip->i_addr[12] == 0) {
            void *retptr = allocate_block();
            if (retptr == NULL) {
                panic ("Cannot allocate singly indirect block!");
            }
            ip->i_addr[12] = (rptr_t) ABS2REL(retptr);
        }
        return get_indirect_addr((void*)REL2ABS(ip->i_addr[12]), bn-12);
    }

    if (bn < DOUBLY_LIMIT) {
        // Doubly indirect
        bn_offset = bn - SINGLY_LIMIT;
        if (ip->i_addr[13] == 0) {
            void *retptr = allocate_block();
            if (retptr == NULL) {
                panic ("Cannot allocate doubly indirect block!");
            }
            ip->i_addr[13] = (rptr_t) ABS2REL(retptr);;
        }

        doubly_offset = bn_offset >> 6; // offset in the doubly indirect block
        assert (doubly_offset < PTRS_PER_BLOCK);
        singly_blk_addr = get_indirect_addr((void*)REL2ABS(ip->i_addr[13]), doubly_offset);

        singly_offset = bn_offset & 63;
        return get_indirect_addr((void*)REL2ABS(*singly_blk_addr), singly_offset);
    }

    assert (bn < TRIPLY_LIMIT);
    bn_offset = bn - DOUBLY_LIMIT;
    if (ip->i_addr[14] == 0) {
        void *retptr = allocate_block();
        if (retptr == NULL)
            panic ("Cannot allocate triply indirect block!");
        ip->i_addr[14] = (rptr_t) ABS2REL(retptr);
    }
    triply_offset = bn_offset >> 12;
    assert (triply_offset < PTRS_PER_BLOCK);
    doubly_blk_addr = get_indirect_addr((void*)REL2ABS(ip->i_addr[14]), triply_offset);

    doubly_offset = (bn_offset >> 6) & 63; // offset in the doubly indirect block
    assert (doubly_offset < PTRS_PER_BLOCK);
    singly_blk_addr = get_indirect_addr((void*)REL2ABS(*doubly_blk_addr), doubly_offset);

    singly_offset = bn_offset & 63;
    return get_indirect_addr((void*)REL2ABS(*singly_blk_addr), singly_offset);
}

/*
 * Given the address of an indirect block, and the nth pointer inside it,
 * return the *address* of the nth pointer inside this indirect block.
 *
 * If this nth ptr is NULL, meaning that the next-level block has not yet been
 * allocated, we allocate it!
 */
rptr_t *get_indirect_addr (void *indirect_bp, uint32_t nth) {
    assert (indirect_bp != NULL);

    rptr_t *retp = (rptr_t *)((uintptr_t)indirect_bp + nth*sizeof(rptr_t));
    if (*retp == 0) {
        // This block has not been allocated
        if (nth != 0) {
            // Then the previous ptr should not be NULL!!
            rptr_t *prev = (rptr_t*)((uintptr_t)retp - sizeof(rptr_t));
            assert (*prev != 0);
        }

        void *bptr = allocate_block();
        if (bptr == NULL) {
            panic("Cannot allocate indirect ptr block");
        }
        *retp = (rptr_t) ABS2REL(bptr);
    }
    return retp;
}

time_t current_time() {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return t.tv_sec;
}

void ilock (inode_t *ip) {
    biased_lock (&ip->i_bmutex);
}

void iunlock (inode_t *ip) {
    biased_unlock (&ip->i_bmutex);
}

int islocked(inode_t *ip) {
    return ip->i_bmutex.lock;
}
