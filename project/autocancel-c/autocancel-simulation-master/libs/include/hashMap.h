#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include "utils.h"

typedef struct hashMapItem{
    int inUse;                      // 0 means not used
    unsigned long long key;
    void *value;
} hashMapItem;

typedef struct hashMap{
    int isLocal;                    // indicate this local map just used in one cancellable task
    unsigned int size;
    unsigned int arraySize;
    hashMapItem *mapArray;
    pthread_rwlock_t *rwLock;
} hashMap;

/**
 * in various stage, we may need different value to represent the usage condition of different resources
 * TODO: maybe need a better name
*/
typedef struct rscMetric {
    long long usageVal;             // usage or occupy
    long long waitVal;              // wait time to get access to the usage
    long long oldUsageVal;          // old usage value
    long long oldWaitVal;           // old wait value
} rscMetric;

typedef struct hashMapStrKeyItem{
    int inUse;                      // if is use
    int isTaskLevel;                // if the resource is collected at task level, but may be removed later
    long long timeStamp;            // timestamp for resource such as lock wait
    long long oldValue;             // used for integer or long value
    char *key;                      // resource key
    union{
        void *value;                    // each task's value of a specific rsc
        double dValue;                  // dValue for traditional hash map usage
    };
    void *clientVal;                // each client's overall value of a specific rsc
    void *oldClientVal;             // each client's overall value of a specific rsc a time interval before
    enum rscType type;              // type of the resource
    long long lastTaskCpuTime;      // only used for CPUCYCLE type  
    long long lastTaskIoBytes;      // only used for IOWAIT type
    long long lastTaskIoDelay;      // only used for IOWAIT type
    rscMetric rscMtc;               // resource metric
} hashMapStrKeyItem;

typedef struct hashMapStrKey{
    unsigned int size;
    unsigned int arraySize;
    hashMapStrKeyItem *mapArray;
} hashMapStrKey;



hashMap *createHashMap(int size);
hashMap *createLocalHashMap(int size);
void resizeHashMap(hashMap *map);
unsigned int hashKey(unsigned long long key, unsigned int size);
void _insert(hashMapItem *mapArray, unsigned int size, unsigned long long key, void *value);
void insert(hashMap *map, unsigned long long key, void *value);
int _get(hashMapItem *mapArray, unsigned int size, unsigned long long key);
void *get(hashMap *map, unsigned long long key);
void removeCancellable(hashMap *map, unsigned long long key);
void removeEntry(hashMap *map, unsigned long long key, int needClean);
void destroyHashMap(hashMap *map);
void printHashMap(hashMap *map);

hashMapStrKey *createHashMapStrKey(int size);
unsigned int hashStr(char *s, unsigned int size);
void resizeHashMapStrKey(hashMapStrKey *map);
void _insertStrKeyItem(hashMapStrKeyItem *mapArray, unsigned int size, char *key, void *value, long long oldValue, int directCopy, long long timeStamp, enum rscType type);
void insertStrKeyItem(hashMapStrKey *map, char *key, void *value, long long timeStamp, enum rscType type);
void insertStrKeyDouble(hashMapStrKey *map, char *key, double value);
int _getStrKeyItem(hashMapStrKeyItem *mapArray, unsigned int size, char *key);
void *getStrKeyItem(hashMapStrKey *map, char *key);
double getStrKeyDouble(hashMapStrKey *map, char *key);
void destroyHashMapStrKey(hashMapStrKey *map);

#endif