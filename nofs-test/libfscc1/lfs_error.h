//
// Created by Ding Yuan on 2018-01-02.
//

#ifndef LIBFS_LFS_ERROR_H
#define LIBFS_LFS_ERROR_H

extern int lfs_error;

#define LFS_EALIGN       180   /* Address is not aligned */
#define LFS_EENV         181   /* Error from underlying library calls. Check errno. */
#define LFS_EENDDIR      182   /* End of dir is reached. */
#define LFS_ETOOMANY     183   /* Too many processes using this fs concurrently! */
#define LFS_NOTABS       184   /* Not an absolute pathname */

#define LFS_EPERM        1  /* Operation not permitted */
#define LFS_EBADF        9      /* Bad file number */
#define LFS_EFAULT       14  /* Bad memory address */
#define LFS_EINVAL       22  /* Invalid argument */
#define LFS_EALREADY     114 /* Operation already been done or in progress */
#define LFS_ENOENT       2   /* No such file or directory */
#define LFS_ENOTDIR      20  /* Not a directory */
#define LFS_ENOEXEC      8   /* No exec permission */
#define LFS_ENAMETOOLONG 36 /* File name too long*/
#define LFS_ENOMEM       12 /* No memory */
#define LFS_EEXIST       17 /* File exist */
#define LFS_EACCES       13 /* Permission denied */
#define LFS_EISDIR       21 /* Is a directory */
#define LFS_EFBIG        27  /* File too large */
#define LFS_EMLINK       31 /* Too many links. */
#define LFS_ERANGE       34  /* Math result not representable */
#define LFS_ENOTEMPTY    39      /* Directory not empty */
#define LFS_EOVERFLOW   75  /* Value too large for defined data type */
#endif //LIBFS_LFS_ERROR_H
