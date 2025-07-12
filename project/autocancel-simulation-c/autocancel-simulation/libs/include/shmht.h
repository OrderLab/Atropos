#ifndef __SHMHT_H_
#define __SHMHT_H_

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <pthread.h>
#include "utils.h"

#define SHM_PATH "/tmp/map"
#define SHM_LOCK_MAP_PATH "/tmp/lockMap"
#define SHM_CTMAP_RWLOCK_PATH "/tmp/ctMapRWLock"
#define SHM_GLOBLOCKMAP_RWLOCK_PATH "/tmp/globLockMapRWLock"
#define MAX_KEY_SIZE 64
#define GLOBAL_LOCK_MAP_SIZE 128
#define PID_TO_KEY_MAX_SIZE 7
#define shmhtGeneralInsert(map, keyStr, valStruct, valPtr, member, val, rwLock) do { \
    int pos; \
    entry *e; \
    pthread_rwlock_t *lk = rwLock; \
    if (map->isGlobal && lk != NULL){ \
        pthread_rwlock_wrlock(lk); \
    } \
    void *mapArray = (void *)map + sizeof(shmht); \
    pos = _shmhtFindInsertPos(map, keyStr); \
    if (pos == -1){ \
        fprintf(stderr, "map is so full!!\n"); \
        break; \
    } \
    e = (entry *)(mapArray + pos * map->pairSize); \
    valPtr = (valStruct *)((void *) e + sizeof(entry)); \
    e->isUse = 1; \
    strcpy(e->key, keyStr); \
    valPtr->member = val; \
    if (map->isGlobal && lk != NULL){ \
        pthread_rwlock_unlock(lk); \
    } \
} while(0)

extern struct shmht *m;
extern struct shmht *globalLockMap;
extern int ctMapRWLockShmId;
extern pthread_rwlock_t *ctMapRWLock;
extern int globalLockMapRWLockShmId;
extern pthread_rwlock_t *globalLockMapRWLock;

typedef struct entry
{
    int isUse;                  /* if this entry is used */
    char key[MAX_KEY_SIZE];     /* key */
} entry;

typedef struct rscValue
{
    int isTaskLevel;            /* if the resource should be monitored at task level, if is, it will be cleared at EndTask */
    long long timeStamp;        /* time stamp, for storage and also flag */
    long long oldValue;         /* old value */
    long long value;            /* current value */
    long long waitValue;        /* wait value */
    long long oldWaitValue;     /* old wait value*/
    enum rscType type;          /* rsc type */
    //queue rscHQ;                /* history queue of resource value */
} rscValue;

/**
 * since based on the project usage, the small hash table is used in struct cancellable,
 * it must be put at shared memory, as a result, it can't be extended dynamically 
*/
typedef struct shmht
{
    unsigned int size;          /* overall size of the hash table */
    int valSize;                /* size of the value */
    int pairSize;               /* sum size of entry and value */
    int shmhtId;                /* used for detach and delete when it's independently created by shmget */
    int isGlobal;               /* optimize for read/write lock over map, if not global we don't require lock sync */
} shmht;


/**
 * create ctmap rwlock
*/
void createMapRWLock(void);

/**
 * init ctmap rwlock, called by initCancellableTaskShmht
*/
void initMapRWLock(void);

/**
 * directly create our mapping from task(thread, connection, process) to cancellable
 * @param size the total number of task or connections allowed.
 * @param rscNum the number of resource need to to trace for each cancellable task
*/
shmht *createCancellableTaskShmht(long long size, long long rscNum);

/**
 * initialize the cancellable task map, should be called only once at master process
 * @param rscNum the number of resource need to to trace for each cancellable task
 * @param ctMap the ptr to cancellable task map
*/
void initCancellableTaskShmht(long long rscNum, shmht *ctMap);

/**
 * hash function
*/
unsigned int shmhtHash(char *s, unsigned int size);

/**
 * shmht find function
 * return pos if find, else return -1;
*/
int shmhtFind(shmht *map, char *key);

/**
 * return a index where we can insert value
*/
int _shmhtFindInsertPos(shmht *map, char *key);

/**
 * shmht get function
*/
void *shmhtGet(shmht *map, char *key);

/**
 * shmht remove function
*/
void shmhtRemove(shmht *map, char *key);

/**
 * shmht insert function
*/
void *shmhtInsert(shmht *map, char *key, long long value);

/**
 * shmht print
 * can only entry + long long not entry + cancellable
*/
void shmhtPrint(shmht *map);

/**
 * debug print backtrace
*/
void printCallers(void);

void detachShmhtCancellable(void);

#endif