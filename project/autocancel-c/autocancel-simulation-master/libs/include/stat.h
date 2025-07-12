#ifndef _STAT_H
#define _STAT_H

#include "global.h"
#include "cancellable.h"
#include "hashMap.h"
#include <sys/syscall.h>

#define PROC_STAT "/proc/stat"
#define PERF_OUTPUT "/tmp/sysbenchOutput"
#define STAE_PATH "/tmp/selfstat"
#define CRASH_PATH "/tmp/crashInf"

extern hashMap *threadMap;
extern hashMap *tidChangeMap;                   // typically some application use their own thread id to manage, I provide a map for convenient change
extern volatile int perfCrash;
extern volatile int abortMonitor;
extern volatile int needTrackAnyway;
extern pthread_rwlock_t mapRWLock, tidChangeMapRWLock, thdLockMapRWLock;

// rerun data structure
extern char rerunQueryString[512];				// record the string of the request for rerun
extern char currQueryString[512];				// current executed request
extern long long currQueryStringLength;	        // current executed request length
extern long long rerunQueryStringLength;	    // rerun request length
extern long long killedTime;				    // time of being killed

// system rsc tracing

void readStat(unsigned int pid, pthread_t tid, cancellable *c);

// monitor Performance, but need revision to directly monitor in app

/**
 * @brief initialize a thread to call monitor
*/
void *initCancellableManagerNoSharedMemory(void *arg);

/**
 * @brief notify the monitor to stop 
*/
void closeMonitor(void);

/**
 * @brief reset perf warn or perf crash or just perf crash
*/
void resetPerfSignal(int setAll);

/**
 * @brief monitor benchmark output
*/
void *monitorPerfOutput(void *arg);
void *mysqlMonitor(void *arg);
void *pgsqlMonitor(void *arg);

/**
 * @brief ret perfCrash
*/
int isPerfCrash(void);

// Instrumentation

void addRscUsage(long long value, char *rscName);
void addRscUsageWait(long long value, char *rscName);
void addRscOccupy(void *thdPtr, char *rscName);
void getRscOccupy(void *thdPtr, char *rscName);
void addRscWait(char *rscName);
void rmRscWait(char *rscName);
void addRscHold(void *rscPtr, char *rscName);
void getRscHold(void *rscPtr, char *rscName);
void addRscHoldWait(char *rscName);
void rmRscHoldWait(char *rscName);

int getThroughput();

// shmht

void *initCancellableManager(void *arg);
void addShmhtRscUsage(shmht* ctMap, long long val, char *rscName);
void addShmhtRscUsageWait(shmht *ctMap, long long val, char *rscName);
void addShmhtRscOccupy(void *rscPtr, char *rscName);
void getShmhtRscOccupy(void *rscPtr, char *rscName);
void addShmhtRscWait(char *rscName);
void rmShmhtRscWait(char *rscName);

// Api For Picking Policy

double attributeSlowdown(cancellable *c, char *resource_key);
double attributeClientIntervalSlowdown(cancellable *c, char *resource_key);
long long getRscUsage(cancellable *c, char *rscKey);
long long getFutureRscUsage(cancellable *c, char *rscKey);
long long pickPolicyDefault(FILE *fp);
long long pickPolicyPredictBased(FILE *fp);
long long pickPolicyMultiObj(FILE *fp);
long long pickPolicyMultiObjPredictBased(FILE *fp);

long long shmhtGetRscUsage(cancellable *c, char *rscKey) __attribute__((optimize("O0")));
long long shmhtGetFutureRscUsage(cancellable *c, char *rscKey) __attribute__((optimize("O0")));
long long shmhtPickPolicyDefault(FILE *fp);
long long shmhtPickPolicyPredictBased(FILE *fp);
long long shmhtPickPolicyMultiObj(FILE *fp);
long long shmhtPickPolicyMultiObjPredictBased(FILE *fp);

double shmhtAttributeSlowdown(cancellable *c, char * rscKey);
double shmhtGetContentionLevel(char *rscKey);

// debug
int isBackground(int appTid);
void setBackgroundNeedKilled(int appTid);
int needKillBackground(void);

//=========================================================
void setClientMonitorMode(void);

void setMonitorProgress(void (*func)(unsigned long, FILE *));

void setCancelAction(void (*func)(unsigned long) );

void deployCancellationPolicy(int (*func)(hashMap));

// potential function for future use
/**
 * @brief judge if there is any idle cpu
*/
int CPUsIdle(void);

#endif