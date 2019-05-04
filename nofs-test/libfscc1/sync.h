//
// Created by Ding Yuan on 2018-01-15.
//

#ifndef LIBFS_SYNC_H
#define LIBFS_SYNC_H

#include <stdint.h>

#define GET_CONTENTION(byte) (byte & 0x01)

/*
 * We implement the biased lock. The first 7 bits of tid_contention
 * is used for thread ID, while the last bit is used to test contention.
 * Once it is set to 1, it will never to set back to 0, and it always revert to CAS.
 *
 * TODO: support thread ID.
 * */
typedef struct biased_mutex {
    volatile uint8_t pid;
    volatile uint8_t tid_contention;
    volatile int     lock;
} bmutex_t;

void biased_lock_init(bmutex_t *l);
void biased_lock(bmutex_t *l);
void biased_unlock(bmutex_t *l);
void nonbiased_lock (volatile int *p);
void nonbiased_unlock (volatile int *p);

//void readlock_acquire (void *block);
//void readlock_release (void *block);
//void writelock_acquire (void *block);
//void writelock_release (void *block);
#endif //LIBFS_SYNC_H
