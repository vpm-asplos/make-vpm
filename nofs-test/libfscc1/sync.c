//
// Created by Ding Yuan on 2018-01-15.
//

#include <assert.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <stdio.h>

#include "sync.h"
#include "lfs.h"

/* TODO: Currently this design is not optimal: we init the lock to be owned by
 * the calling process. We should really initialize it to NEUTRAL state.
 */
void biased_lock_init (bmutex_t *l) {
    l->pid = bmutex_pid;
    l->tid_contention = 0;
    l->lock = 0;
}

void biased_lock (bmutex_t *l) {
    if (GET_CONTENTION(l->tid_contention) == 0) {
        if (l->pid == bmutex_pid) { // fast path
            // TODO: further check tid match
            // assert(l->lock == 0); // debug purpose
            l->lock = 1;
            return;
        } 
        printf("[WARNING] *** FIRST TIME CONTENTION!!! ****\n\n\n");
        
    }

    printf("[WARNING] *** Entering slow path!!! ****\n\n\n");
    // Slow path
    l->tid_contention |= 0x01; // set contention bit

    nonbiased_lock(&l->lock);
}

void biased_unlock (bmutex_t *l) {

    // assert(l->lock == 1); // debug purpose
    l->lock = 0;
    /* __sync_bool_compare_and_swap() was described in comments above */

    /*if (__sync_bool_compare_and_swap(futexp, 1, 0)) {
        s = futex(futexp, FUTEX_WAKE, 1, NULL, NULL, 0);
        if (s  == -1) {
            perror("futex-FUTEX_WAKE");
            exit(1);
        }
    }*/
}

void nonbiased_lock (volatile int *p) {
    while (1) {
        while (*p != 0)
            ; // wait for the lock holder to release the lock; this includes the holder of the biased lock
     /* __sync_bool_compare_and_swap(ptr, oldval, newval) is a gcc
        built-in function.  It atomically performs the equivalent of:

        if (*ptr == oldval)
           *ptr = newval;

        It returns true if the test yielded true and *ptr was updated.
        The alternative here would be to employ the equivalent atomic
        machine-language instructions.  For further information, see
        the GCC Manual. */

        if (__sync_bool_compare_and_swap(p, 0, 1))
            break;      /* We have acquired the lock */
    }
}

void nonbiased_unlock (volatile int *p) {
    *p = 0;
}





/*static int
futex(int *uaddr, int futex_op, int val,
      const struct timespec *timeout, int *uaddr2, int val3)
{
    return syscall(SYS_futex, uaddr, futex_op, val,
                   timeout, uaddr, val3);
} */
