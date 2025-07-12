#include <stdlib.h>
#include "utils.h"
#include "hashMap.h"
#include "shmht.h"
#include <pthread.h>

#ifndef CANCELLABLE_H
#define CANCELLABLE_H

#define RERUN_GAP 9

extern pthread_key_t currTid;

extern int clientMonitorMode;

typedef struct cancellable{
    int isCancellable;
    int tid;
    long startTime;                                     // start time of a task or a request

    long long finishedRows;
    long long estimatedRows;

    int needRerun;                                      // need rerun flag
    long long lastKillTime;                             // last kill time for reference if rerun policy need it

    int isBackground;                                   // background
    int needKilled;                                     // the background need to killed

    hashMap *occupyMap;                                 // local occupy map  TODO: later I may transfer thdLockMap to here
    hashMapStrKey *rscMap;                              // resource map
    long long maxRscNum;
    long long finishedRequests;
    int priority;

    // shmht usage
    long long lastTaskCpuTime;
    long long lastTaskIoBytes;
    long long lastTaskIoDelay;
} cancellable;

// multi-thread model

void destructorFunction(void *data);
void setCurrTid(int tid);
int getCurrTid(void);

cancellable *createCancellable(int isCancellable,uint key,int priority);
void freeCancellable(cancellable *c);
cancellable* getCancellable(uint key);
hashMap* getCancellables();
int getResources(cancellable *c);
/**
 * to give user a iterable name list of resources
*/
void getRscList(cancellable *c, char **rscList);    
long long getUsage(cancellable *c, char *resource_key);
double getContentionLevel(char *resource_key);
//double attributeSlowdown(cancellable *c, char *resource_key);
double getLeftProgress(cancellable *c);
int predictUsageProgress(cancellable *c, char *resource_key);
int getPriority(cancellable *c);
void enableCancellable(int pid, cancellable *c);
void disableCancellable(cancellable *c);
void taskStart(cancellable *c);
void setEstimatedJob(unsigned int key, long long estimatedJob);
void addFinishedJob(unsigned int key, long long newlyFinishedJob);

void MysqlSelect(unsigned int threadId, unsigned int totalRow, cancellable *c);


void addShmhtTask(shmht *ctMap, int key, int isBackground);
void removeShmhtTask(shmht *map, int key);
void shmhtStartTask(shmht *ctMap);
void shmhtEndTask(shmht *ctMap);
void shmhtSetEstimatedJob(long long estimatedJob);
void shmhtAddFinishedJob(long long finishedJob);

cancellable *shmhtGetCancellable(int key);
long long shmhtGetUsage(cancellable *c, char *key);
int getshmhtContentionLevel(char *resource_key);
int shmhtPredictUsageProgress(cancellable *c, char *resource_key);
int shmhtgetPriority(cancellable *c);

/**
 * The size of each rsc key is limited to maximum MAX_KEY_SIZE
 * therefore the user should allocate enough space for the rscList
*/
int shmhtGetRscList(cancellable *c, char **rscList);

// rerun

int rerunPolicy();

void disableRerun();

#endif