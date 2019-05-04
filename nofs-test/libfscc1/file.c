//
// Created by Ding Yuan on 2018-01-28.
//

#include <stddef.h> // null
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "file.h"
#include "libfs.h"
#include "lfs_error.h"
#include "bitmap.h"
#include "lfs.h"
#include "sync.h"

int lfs_open (const char *pathname, int flags, ...) {
    struct namei_data ndata;
    inode_t *ip = namei(pathname, NSEARCH, &ndata);

    if (ip == NULL) {
        if (flags&LFS_O_CREAT) {
            // Create the file
            va_list arguments;
            va_start (arguments, flags);
            uint16_t mode = (uint16_t) va_arg (arguments, int); // assume you have the 3rd mode parameter!!
            va_end (arguments);
            return lfs_creat(pathname, mode);
        } else {
	  //printf("opening with error, no LFS_O_CREAT\n");
            lfs_error = ndata.error;
            return -1;
        }
    }

    // we use flags+1 as the internal flag value. This is because our internal permission values is off by 1
    // from the user flags. This design is the same as in Unix. I have no idea why this is the case in Unix.
    if (ndata.parent_ip != NULL)
        iunlock(ndata.parent_ip);

    // Holding the lock on ip
    return open1 (ip, flags+1);
}

/**
 * The lseek() function repositions the offset of the open file associated
 * with the file descriptor fd to the argument offset according to the directive whence as follows:
 *   SEEK_SET: The offset is set to offset bytes.
 *   SEEK_CUR: The offset is set to its current location plus offset bytes.
 *   SEEK_END: The offset is set to the size of the file plus offset bytes.
 *
 * TODO: The lseek() function allows the file offset to be set beyond the end of the file (but
 * this does not change the size of the file). If data is later written at this point,
 * subsequent reads of the data in the gap (a "hole") return null bytes (aq\0aq) until data
 * is actually written into the gap.
 *
 * Return Value:
 *   Upon successful completion, lseek() returns the resulting offset location as measured
 *   in bytes from the beginning of the file. On error, the value (off_t) -1 is returned and
 *   lfs_errno is set to indicate the error.
 *
 * Errors
 * LFS_EBADF:     fd is not an open file descriptor.
 * LFS_EINVAL:    whence is not valid.
 * LFS_EOVERFLOW: The resulting file offset cannot be represented in an off_t, or it's beyond file size.
 */
int lfs_lseek(int fd, int offset, int whence) {
    int new_offset = 0;
    lfs_error = 0;
    if (validate_fd(fd) != 0)
        return -1;

    struct file *fp = (struct file *)u.u_ofile[fd];
    flock(fp);
    inode_t *ip = (inode_t*)REL2ABS(fp->f_inode);
    if (whence == LFS_SEEK_SET) {
        new_offset = offset;
    } else if (whence == LFS_SEEK_CUR) {
        new_offset = offset + fp->f_offset;
        if (new_offset < 0) {
            funlock(fp);
            lfs_error = LFS_EOVERFLOW;
            return -1;
        }
    } else if (whence == LFS_SEEK_END) {
        if (offset > 0) {
            funlock(fp);
            lfs_error = LFS_EOVERFLOW;
            return -1;
        }
        new_offset = ip->i_size1 + offset;
    } else {
        funlock(fp);
        lfs_error = LFS_EINVAL;
        return -1;
    }

    if (new_offset > ip->i_size1) {
        funlock(fp);
        lfs_error = LFS_EOVERFLOW;
        return -1;
    }

    fp->f_offset = new_offset;
    funlock(fp);
    return new_offset;
}

int lfs_creat (const char *pathname, uint16_t mode) {
    int newfnamelen = 0;
    struct namei_data ndata;

    assert ((mode & IFMT) != IFDIR);

    inode_t *parent_ip = namei(pathname, NCREATE, &ndata);
    if (parent_ip == NULL) {
        // Parent does not exist or other errors
        lfs_error = ndata.error;
        return -1;
    }

    if (ndata.parent_ip != NULL)
        iunlock(ndata.parent_ip); // we are not touching the grand parent dir of pathname
    // At this point, we have ndata.parent_ip pointing to the parent directory of the new file; ndata.cp pointing to the end of pathname
    // First, sanity check, whether the new file name is valid
    assert (ndata.cp != NULL);
    ndata.cp--; // skip the '\0'
    if (*(ndata.cp)=='/') {
        // Error, cannot create directory
       lfs_error = LFS_EINVAL;
       goto out;
    }

    // shift the pointer, ndata.cp, backward to locate the filename to be created.
    // Stop conditions: '/', length of the filename is larger than max, and ndata.cp == pathname
    // The last condition is to handle if pathname is a relative path name
    while (*(ndata.cp) != '/' && newfnamelen < LFS_NAMELEN && ndata.cp != pathname) {
        ndata.cp--;
        newfnamelen++;
    }

    if (newfnamelen >= LFS_NAMELEN) {
         lfs_error = LFS_ENAMETOOLONG;
         goto out;
    }

    if (*(ndata.cp) == '/')
        ndata.cp++; // skip the '/'; now cp points to the start of the new dir name

    // allocate a new inode
    inode_t *new_ip = ialloc();
    if (new_ip == NULL) {
         lfs_error = LFS_ENOMEM; // Running out of inodes!!
         goto out;
    }
    new_ip->i_mode = (IFBLK | mode);
    new_ip->i_uid = u.u_uid; // Global
    new_ip->i_gid = u.u_gid; // Global
    new_ip->i_mtime = current_time();

    ilock(new_ip); // lock it before linking it into the parent dir
    // insert the dir entry: <inode number, dirname> into parent dir
    wdir (parent_ip, ndata.cp, new_ip->i_number);
    parent_ip->i_mtime = current_time();

    // No longer needs to lock parent_ip
    iunlock(parent_ip);

    int flags = (LFS_O_WRONLY|LFS_O_TRUNC);
    return open1 (new_ip, flags+1);

out:
    iunlock(parent_ip);
    return -1;
}


/*
 * Open a directory; simply return a file descriptor.
 */
int lfs_opendir(const char *pathname) {
    struct namei_data ndata;
    inode_t *ip = namei(pathname, NSEARCH, &ndata);

    if (ip == NULL) {
       lfs_error = ndata.error;
       return -1;
    }

    if (ndata.parent_ip != NULL)
        iunlock(ndata.parent_ip);
    return open1(ip, FREAD);
}

/*
 * When end of dir is reached, it returns -1 with lfs_error set to.
 *
 * TODO: the entire dir file (ip) is locked during the process, eliminating concurrent reads. Might want to relax this.
 */
int lfs_readdir(int fd, struct libfs_dirent *dp) {
    lfs_error = 0;
    assert (dp != NULL);
    if (validate_fd(fd) != 0)
        return -1;

    struct file *fp = (struct file *)u.u_ofile[fd];
    if ((fp->f_flag & FREAD) == 0) {
        // Do not have read permission
        lfs_error = LFS_EBADF;
        return -1;
    }

    inode_t *ip = (inode_t*)REL2ABS(fp->f_inode);
    assert ((ip->i_mode & IFMT) == IFDIR);

    ilock(ip);
    flock(fp);
    while (1) {
        if (fp->f_offset >= ip->i_size1) {
            lfs_error = LFS_EENDDIR;
            funlock(fp);
            iunlock(ip);
            return -1;
        }
        uint16_t offset = fp->f_offset & 0777;
        fp->f_offset += sizeof(struct libfs_dirent);
        uint32_t bn = fp->f_offset >> 9; // unsigned right shift


        rptr_t *bpp = get_block_ptr_addr(ip, bn);
        assert (bpp != NULL);
        assert (*bpp != 0);
        // assert(ip->i_addr[bn] != NULL);
        struct libfs_dirent *nextp = (struct libfs_dirent *) ((uintptr_t)REL2ABS(*bpp) + offset);
        if (nextp->i_number == 0)
            continue;

        dp->i_number = nextp->i_number;
        for (int i = 0; i < LFS_NAMELEN; i++)
            dp->name[i] = nextp->name[i];

        funlock(fp);
        iunlock(ip);
        return 0;
    }

}

int lfs_closedir (int fd) {
    return lfs_close(fd);
}

/*
 * Close the file identified by fd.
 */
int lfs_close (int fd) {
    if (validate_fd(fd) != 0)
        return -1;

    struct file *fp = (struct file *) u.u_ofile[fd];

    flock(fp);
    fp->f_count--;
    if (fp->f_count == 0) {
        fp->f_offset = 0;
        fp->f_flag = 0;
        fp->f_inode = 0;
    }
    u.u_ofile[fd] = NULL;
    funlock(fp);
    return 0;
}

/*
 * implementation of the lfs_write.
 * Writes up to count bytes from the buffer pointed buf to the file referred to by the file
 * descriptor fd. The number of bytes written may be less than count if there is insufficient space.
 *
 * The writing takes place at the current file offset, and the file offset is incremented by the number of
 * bytes actually written. If the file was open(2)ed with O_APPEND, the file offset is first set to the end of the
 * file before writing. The adjustment of the file offset and the write operation are performed as an atomic step.
 *
 * Return Value
 *   On success, the number of bytes written is returned (zero indicates nothing was written). On error, -1 is returned,
 *   and lfs_errno is set appropriately.
 *
 * Errors
 *   LFS_EBADF: fd is not a valid file descriptor or is not open for writing.
 *   LFS_EFAULT: buf is outside your accessible address space.
 *   LFS_EFBIG: An attempt was made to write a file that exceeds the implementation-defined maximum file size or
 *          the process's file size limit, or to write at a position past the maximum allowed offset.
 */

int lfs_write (int fd, const void *buf, int count) {
    lfs_error = 0;
    if (validate_fd(fd) != 0)
        return -1;

    // TODO: check permission

    struct file *fp = (struct file *)u.u_ofile[fd];
    if ((fp->f_flag & FWRITE) == 0) {
        // Do not have write permission
        lfs_error = LFS_EBADF;
        return -1;
    }
    int copied_size = 0;

    flock(fp);
    inode_t *ip = (inode_t*)REL2ABS(fp->f_inode);

    ilock(ip);
    uint16_t offset = fp->f_offset & 0777;
    uint32_t bn = fp->f_offset >> 9; // unsigned right shift

    char *block = (char *) get_block_abs_addr(ip, bn);
    char *src = (char *)buf;

    /* If we are not at the block boundary, copy byte-by-byte
     * until we reach the block boundary. */
    if(fp->f_offset & 0777) {
        while(copied_size < count) {
	        if((offset & 0777) == 0) {
	             break;
	        }
	        assert(block != NULL);
	        block[offset++] = src[copied_size++];
        }
    } else {
        bn --;
    }

    /* We are at block boundary. This loop copies 512 bytes at a time. */
    while((count - copied_size) >= LFS_BLOCKSIZE) {
        bn ++;
        if (bn > MAX_BLOCKS) {
	        lfs_error = LFS_EFBIG;
            goto out;
        }

        block = (char *) get_block_abs_addr(ip, bn);
        memcpy(block, src+copied_size, LFS_BLOCKSIZE);
        copied_size += LFS_BLOCKSIZE;
    }

    if (copied_size == count)
        goto out; // done copy!

    /* Copy the remainder. */
    assert((offset & 0777) == 0);
    offset = 0; // reset it to 0 of the new block

    int remainder = count - copied_size;
    assert (remainder < LFS_BLOCKSIZE);
    assert (remainder > 0);

    bn++;
    if (bn > MAX_BLOCKS) {
        lfs_error = LFS_EFBIG;
        goto out;
    }

    block = (char *) get_block_abs_addr(ip, bn);
    memcpy (block, src+copied_size, remainder);
    copied_size = count;

out:
    // This fence makes sure that append is atomic.
    /******************* mfence **************************/
    asm volatile ("mfence" ::: "memory");
    /*****************************************************/

    fp->f_offset += copied_size;
    if (fp->f_offset > ip->i_size1)
        ip->i_size1 = fp->f_offset;

    ip->i_mtime = current_time();

    iunlock(ip);
    funlock(fp);
    return copied_size; // success!!
}

/*
 * Implementation of lfs_read.
 * lfs_read() attempts to read up to count bytes from file descriptor fd into the buffer starting at buf.
 * The read operation commences at the current file offset, and the file offset is incremented by the
 * number of bytes read. If the current file offset is at or past the end of file, no bytes are read, and
 * read() returns zero.
 *
 * Return Value:
 * On success, the number of bytes read is returned (zero indicates end of file), and the file position
 * is advanced by this number.
 *
 * Errors:
 * LFS_EBADF: fd is not a valid file descriptor or is not open for reading.
 * LFS_EISDIR: fd refers to a directory.
 */
int lfs_read(int fd, void *buf, int count) {
    lfs_error = 0;
    if (validate_fd(fd) != 0)
        return -1;

    struct file *fp = (struct file *)u.u_ofile[fd];
    if ((fp->f_flag & FREAD) == 0) {
        // Do not have read permission
        lfs_error = LFS_EBADF;
        return -1;
    }

    inode_t *ip = (inode_t*)REL2ABS(fp->f_inode);
    if ((ip->i_mode & IFMT) == IFDIR) {
        lfs_error = LFS_EISDIR;
        return -1;
    }

    flock(fp);
    ilock(ip);

    int copied_size = 0;
    uint16_t offset = fp->f_offset & 0777;
    uint32_t bn = fp->f_offset >> 9; // unsigned right shift
    rptr_t *bpp = get_block_ptr_addr(ip, bn);
    assert (bpp!=NULL);
    if (*bpp == 0) {
        // This is an empty file, so we directly go out (it returns 0)
        goto out;
    }

    char *block = (char *)REL2ABS(*bpp);
    char *dest = (char *)buf;

    if (fp->f_offset + count > ip->i_size1)
        count = ip->i_size1 - fp->f_offset;

    assert (block != NULL);
    if(fp->f_offset & 0777) {
        while (copied_size < count) {
            if ((offset & 0777) == 0) {
                break;
             }
             dest[copied_size++] = block[offset++];
        }
    } else {
         bn --;
    }
    
    while((count - copied_size) >= LFS_BLOCKSIZE) {
        bn ++;
        assert(bn <= MAX_BLOCKS);
        bpp = get_block_ptr_addr(ip, bn);
        assert(bpp!=NULL);
        assert(*bpp!=0);
        block = (char *) REL2ABS(*bpp);
        memcpy(dest+copied_size, block, LFS_BLOCKSIZE);
        copied_size += LFS_BLOCKSIZE;
    }

    if (copied_size == count)
        goto out;

    /* Copy the remainder. */
    assert((offset&0777) == 0);
    offset = 0; // reset it to 0 of the new block

    int remainder = count - copied_size;
    assert (remainder < LFS_BLOCKSIZE);
    assert (remainder > 0);

    bn++;
    assert(bn <= MAX_BLOCKS);
    bpp = get_block_ptr_addr(ip, bn);

    assert(bpp!=NULL);
    assert(*bpp!=0);
    block = (char *) REL2ABS(*bpp);
    memcpy(dest+copied_size, block, remainder);

    copied_size = count;

out:
    fp->f_offset += copied_size;
    iunlock(ip);
    funlock(fp);

    return copied_size;
}

/*
 * Common code for open and creat.
 *
 * Caller holds the lock on ip
 */
int open1 (inode_t *ip, int mode) {
    int fid, ufid;
    struct file *file = (struct file *)SFILE_START(root_addr);
    if (mode&FREAD) {
        if (_access(ip, IRUSR)) {
            lfs_error = LFS_EACCES;
	    printf("Error IRUSR\n");
            goto out0;
        }
    }

    if (mode&FWRITE) {
        if (_access(ip, IWUSR)) {
            lfs_error = LFS_EACCES;

	    printf("Error IWUSR\n");
            goto out0;
        }
    }

    if (mode&LFS_O_TRUNC) {
        if (!(mode&FWRITE))  {
            lfs_error = LFS_EACCES;
	    
	    printf("Error O_TRUNC\n");
            goto out0;
        }
        itrunc(ip); // truncate the file
    }

    if ((fid = falloc(file)) == -1) {
        lfs_error = LFS_ENOMEM; // run out of file descriptors
        goto out0;
    }

    flock(&file[fid]);
    if ((ufid = ufalloc()) == -1) {
        lfs_error = LFS_ENOMEM; // run out of user file descriptors

	printf("Error ufid\n");
        goto out1;
    }

    u.u_ofile[ufid] = &(file[fid]);
    file[fid].f_flag = (uint8_t)(mode &(FREAD|FWRITE)); // Simply indicates whether it's read or write
    file[fid].f_inode = (rptr_t)ABS2REL(ip);
    if (mode&LFS_O_APPEND) {
        if (_access(ip, IWUSR)) { // Must have write permission
            lfs_error = LFS_EACCES;

	    printf("Error APPEND IWUSR\n");
            goto out1;
        }
        file[fid].f_offset = ip->i_size1;
        // Give it write permission
        file[fid].f_flag |= FWRITE;
    } else {
        file[fid].f_offset = 0;
    }

    funlock(&file[fid]);
    iunlock(ip);
    return ufid;

out1:
    funlock(&file[fid]);
out0:
    iunlock(ip);
    return -1;
}

/*
 * Allocate a file descriptor and a file structure.
 * Set the f_count of this FD.
 */
int falloc(struct file *file) {
    for (int i = 0; i < NFILE; i++) {
        if (file[i].f_count == 0) {
            flock(&file[i]);
            if (file[i].f_count != 0) {
                funlock(&file[i]); // OK, there is a race; this is now longer available
                continue;
            }
            file[i].f_count++;
            funlock(&file[i]);
            return i;
        }
    }

    return -1;
}

/*
 * Allocate a user file descriptor
 */
int ufalloc() {
    biased_lock(&u.u_bmutex);
    for (int i = 0; i < NOFILE; i++)
       if (u.u_ofile[i] == NULL) {
           biased_unlock(&u.u_bmutex);
           return i;
       }

    // Cannot find any; all use up
    biased_unlock(&u.u_bmutex);
    return -1;
}

/*
 * Check mode permission on inode pointer.
 * Return 0 if passed permission check. 1 otherwise.
 * TODO: permission support
 */
int _access(inode_t *ip, uint16_t mode) {
    return 0;
}

uint32_t lfs_used_blocks() {
    uint8_t *freemap = (uint8_t *)(FREEMAP_START(root_addr));
    uint32_t used = 0;
    for (uint32_t i = 0; i < BMAP_BYTES*8; i++) {
        if (get_bit(freemap, i) == 1) {
           used++;
        }
    }
    return used;
}

/*
 * Truncate the file. It should not fail!!
 *
 * Caller holds the lock on ip.
 */
void itrunc(inode_t *ip) {
    if (ip->i_size1 == 0)  // already empty :)
        return;

    struct lfs_super *p = (struct lfs_super *) root_addr;

    biased_lock(&p->bitmap_bmutex);
    uint8_t *freemap = (uint8_t *)(FREEMAP_START(root_addr));
    uintptr_t blocks =  (uintptr_t)(BLOCKS_START(root_addr));
    uint32_t bindex;

    uint32_t nblocks = ip->i_size1>>9;

    if (ip->i_size1&0777)
        nblocks++;

    ip->i_size1 = 0;

    // This fence makes sure that if it crashes in the middle of truncating
    // the file, we can use the i_size1 to recover (if i_size1 is 0, then during
    // recovery we will finish the truncation
    /******************* mfence **************************/
    asm volatile ("mfence" ::: "memory");
    /*****************************************************/

    for (uint32_t i = 0; i < nblocks; i++) {
        rptr_t *bpp = get_block_ptr_addr(ip, i);
        if(bpp!=0) {
            bindex = (uint32_t)(((uintptr_t)REL2ABS(*bpp) - (uintptr_t)blocks)/LFS_BLOCKSIZE);
            assert (get_bit(freemap, bindex) == 1);
            clear_bit(freemap, bindex);
            *bpp=0;
        } else {
            break; // Cannot have gap!!
        }
    }

    if (nblocks > DIRECT_LIMIT) {
        assert(ip->i_addr[12] != 0);
        bindex = (uint32_t)(((uintptr_t)REL2ABS(ip->i_addr[12]) - (uintptr_t)blocks)/LFS_BLOCKSIZE);
        assert (get_bit(freemap, bindex) == 1);
        clear_bit(freemap, bindex);
        ip->i_addr[12] = 0;
    }

    if (nblocks > SINGLY_LIMIT) {
        assert(ip->i_addr[13] != 0);
        rptr_t *bpp = (rptr_t *) REL2ABS(ip->i_addr[13]);
        for (int i = 0; i < PTRS_PER_BLOCK; i++) {
            if (*bpp==0)
                break;
            bindex = (uint32_t)(((uintptr_t)REL2ABS(*bpp) - (uintptr_t)blocks)/LFS_BLOCKSIZE);
            assert (get_bit(freemap, bindex)==1);
            clear_bit(freemap, bindex);
            bpp++;
        }
        bindex = (uint32_t)(((uintptr_t)REL2ABS(ip->i_addr[13]) - (uintptr_t)blocks)/LFS_BLOCKSIZE);
        assert (get_bit(freemap, bindex) == 1);
        clear_bit(freemap, bindex);
        ip->i_addr[13]=0;
    }

    if (nblocks > DOUBLY_LIMIT) {
       assert(nblocks<=MAX_BLOCKS);
       rptr_t *bpp = (rptr_t *) REL2ABS(ip->i_addr[14]);
       for (int i = 0; i < PTRS_PER_BLOCK; i++) {
           rptr_t *doubly_bpp = (rptr_t *)REL2ABS(*bpp);
           if (doubly_bpp==NULL)
               break;
           for (int j=0; j < PTRS_PER_BLOCK; j++) {
               if (*doubly_bpp == 0)
                   break;
               bindex = (uint32_t)(((uintptr_t)REL2ABS(*doubly_bpp) - (uintptr_t) blocks) / LFS_BLOCKSIZE);
               assert(get_bit(freemap, bindex) == 1);
               clear_bit(freemap, bindex);
               doubly_bpp++;
           }

           bindex = (uint32_t)(((uintptr_t)REL2ABS(*bpp) - (uintptr_t) blocks) / LFS_BLOCKSIZE);
           assert(get_bit(freemap, bindex) == 1);
           clear_bit(freemap, bindex);
           bpp++;
        }

        bindex = (uint32_t)(((uintptr_t)REL2ABS(ip->i_addr[14]) - (uintptr_t)blocks)/LFS_BLOCKSIZE);
        assert (get_bit(freemap, bindex) == 1);
        clear_bit(freemap, bindex);
        ip->i_addr[14]=0;
    }
    biased_unlock(&p->bitmap_bmutex);
    return;
}

int validate_fd (int fd) {
    if (fd < 0 || fd >= NOFILE || u.u_ofile[fd] == NULL) {
        // invalid fd
        lfs_error = LFS_EBADF;
        return -1;
    }
    return 0;
}

void flock (struct file *fp) {
    biased_lock (&fp->f_bmutex);
}

void funlock (struct file *fp) {
    biased_unlock (&fp->f_bmutex);
}
