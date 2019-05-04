//
// Created by Ding Yuan on 2018-01-26.
//
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "lfs.h"
#include "bitmap.h"
#include "inode.h"
#include "file.h"

/*
 * Test that init has done its job correctly
 */
void test_init (void *root) {
    struct lfs_super *rootp = (struct lfs_super *) root;

    // Super block
    assert (rootp->s_magic == 0xabadf001);
    assert (rootp->s_nblocks == rootp->s_endaddr/LFS_BLOCKSIZE);
#ifndef USER_ALLOCATE_SPACE
    void *brkp = MYSBRK(0);
    assert (rootp->s_endaddr == ABS2REL(brkp));
#endif
    // assert (rootp->next_inode == 2); // Allocated inode for /
    assert (rootp->next_block == 1); // Allocated a block for /

    // Free block bitmap, at this point, block 0 should have been allocated for inode.
    uint8_t *freemap = (uint8_t *)(FREEMAP_START(root));
    assert (get_bit (freemap, 0) == 1);
    for (int i = 1; i < rootp->s_nblocks; i++) {
        assert (get_bit(freemap, i) == 0);
    }

    // Inodes: the first inode should be properly allocated and set (for root). Not the others.
    inode_t *inodes = (inode_t *) INODES_START(root);
    assert (inodes[1].i_nlink == 1);
    assert (inodes[1].i_mode == (IFDIR | IRUSR | IWUSR | IXUSR | IRGRP | IXGRP | IROTH | IXOTH));
    assert (inodes[1].i_size1 == (2 * sizeof (struct libfs_dirent))); // . and ..
    // TODO: check uid and gid

    // Root dir
    void *blocks =  (void *)(BLOCKS_START(root));
    assert ((uintptr_t)blocks == (uintptr_t)REL2ABS(inodes[1].i_addr[0]));
    struct libfs_dirent *direntp = (struct libfs_dirent *)REL2ABS(inodes[1].i_addr[0]);
    assert (direntp[0].i_number == 1);
    assert (strcmp(direntp[0].name, ".") == 0);
    assert (direntp[1].i_number == 1);
    assert (strcmp(direntp[1].name, "..") == 0);
    printf ("[PASSED] test_init\n");
}

void lfs_test(void *root) {
    test_init(root);
}
