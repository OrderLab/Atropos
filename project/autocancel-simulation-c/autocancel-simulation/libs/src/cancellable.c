#include "cancellable.h"
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "stat.h"

cancellable *createCancellable(int isCancellable, uint key, int priority)
{
    if (!threadMap)
      return NULL;
    cancellable *c = (cancellable *) malloc(sizeof(cancellable));
    c->isCancellable = isCancellable;
    c->tid = -1;
    c->startTime = 0;
    c->priority = priority;
    c->estimatedRows = 0;
    c->finishedRows = 0;
    c->isBackground = 0;
    c->needKilled = 0;
    c->finishedRequests = 0;

    c->rscMap = createHashMapStrKey(40);
    c->occupyMap = createLocalHashMap(40);
    c->occupyMap->isLocal = 1;
    insert(threadMap, key, c);
    return c;
}

void freeCancellable(cancellable *c)
{
    destroyHashMapStrKey(c->rscMap);
    free(c);
}

cancellable* getCancellable(uint key) {
   return (cancellable *) get(threadMap, key);
}

hashMap* getCancellables() {
  return threadMap;
}

int getResources(cancellable *c){
  return c->rscMap->size;
}

void getRscList(cancellable *c, char **rscList)
{
    unsigned int i, j = 0;
    hashMapStrKeyItem *mapArray = c->rscMap->mapArray;
    for (i = 0; i < c->rscMap->arraySize; ++i){
        if (mapArray[i].inUse){
            // the key of resources should be on heap
            rscList[j++] = mapArray[i].key;
        }
    }
}

long long getUsage(cancellable *c, char *key) {
    void *val;
    val = getStrKeyItem(c->rscMap, key);
    if (val == NULL){
        return 0;
    }
    return (long long) val;
}

double getLeftProgress(cancellable *c)
{
    if (c->estimatedRows == 0 || c->finishedRows == 0){
        return 0;
    }

    if (c->finishedRows > c->estimatedRows){
        return 0;
    }
    
    double leftProgress =  (double)c->finishedRows / (double)c->estimatedRows;
    leftProgress = 1 - leftProgress;
    return leftProgress;
}

int predictUsageProgress(cancellable *c, char *rscKey)
{
    ;
}

void enableCancellable(int pid, cancellable *c)
{
    c->isCancellable = 1;
    if (clientMonitorMode)
        return;

    /*
    unsigned long long ctime = readThdCpuTime(pid, c->tid);
    unsigned long long ioDelay = readThreadIOStats(pid, c->tid);
    unsigned long long ioUsage = readThreadIOUsage(pid, c->tid);
    */
    FILE *fp;
	char thdFileName[50], line[512];
	int i;
	unsigned long long uTime = 0, sTime = 0;
    unsigned long long ioDelay = 0;
    unsigned long long readBytes = 0, writeBytes = 0, ioUsage = 0;

	genThreadStatsFileName(thdFileName, pid, c->tid);
    /* // tmp note
	if ((fp = fopen(thdFileName, "r")) == NULL)
		return 0;
    */
    /* tmp note
    int count = 0;
	while (fgets(line, sizeof(line), fp) != NULL) {
        if (count == 0){
            i = sscanf(line, "%*d %*s %*c %*d %*d \
                            %*d %*d %*d %*u %*lu \
                            %*lu %*lu %*lu %lu %lu \ 
                            %*ld %*ld %*ld %*ld %*ld \
                            %*ld %*llu %*lu %*ld %*lu \
                            %*lu %*lu %*lu %*lu %*lu \
                            %*lu %*lu %*lu %*lu %*lu \
                            %*lu %*lu %*d %*d %*u \
                            %*u %llu", \
                &uTime, &sTime, &ioDelay);
            break;
        }

        
        if (count == 5){
			i = sscanf(line, "%*s %llu", &readBytes);
		}
		else if (count == 6){
			i = sscanf(line, "%*s %llu", &writeBytes);
		}
        
	}
	fclose(fp);
    */
    
	unsigned long long ctime =  uTime + sTime;
    ioUsage = readBytes + writeBytes;

    for (unsigned int i = 0; i < c->rscMap->arraySize; ++i){
        hashMapStrKeyItem *rscItem = &(c->rscMap->mapArray[i]);
        if (rscItem->inUse && rscItem->type == CPUCYCLE){
            rscItem->lastTaskCpuTime = ctime;
            rscItem->rscMtc.usageVal = ctime;               // because pick policy thread may calculate slowdown at any time, can't wait for monitor thread to update it.
        }
        else if (rscItem->inUse && rscItem->type == IOWAIT){
            rscItem->lastTaskIoBytes = ioUsage;
            rscItem->lastTaskIoDelay = ioDelay;
            rscItem->rscMtc.usageVal = ioUsage;
            rscItem->rscMtc.waitVal = ioDelay;
        }
    }
}

void disableCancellable(cancellable *c)
{
    c->isCancellable = 0;
    c->finishedRequests++;

    for (unsigned int i = 0; i < c->rscMap->arraySize; ++i){
        if (c->rscMap->mapArray[i].inUse){
            //c->rscMap->mapArray[i].oldValue = 0;
            c->rscMap->mapArray[i].value = 0;
            c->rscMap->mapArray[i].rscMtc.oldUsageVal = 0;
            // since we read cpu usage from file, for our picking thread, when it need to calculate the slowdown, 
            // the cpu usage may not be update, which leads to 0 but lastTaskCpu is not 0
            if (c->rscMap->mapArray[i].type != CPUCYCLE){           
                c->rscMap->mapArray[i].rscMtc.usageVal = 0;
            }
            c->rscMap->mapArray[i].rscMtc.oldWaitVal = 0;
            c->rscMap->mapArray[i].rscMtc.waitVal = 0;
            c->finishedRows = 0;
            c->estimatedRows = 0;
        }
    }
}

void taskStart(cancellable *c)
{
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    c->startTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;
}

void setEstimatedJob(unsigned int key, long long estimatedJob)
{
    cancellable *c = getCancellable(key);
    if (!c){
	    return ;
    }
    c->estimatedRows = estimatedJob;
}

void addFinishedJob(unsigned int key, long long newlyFinishedJob)
{
    cancellable *c = getCancellable(key);
    if (!c){
	    return ;
    }
    c->finishedRows += newlyFinishedJob;
}

void MysqlSelect(unsigned int threadId, unsigned int totalRow, cancellable *c)
{
    enableCancellable(threadId, c);
    taskStart(c);
    sleep(totalRow);
    disableCancellable(c);
}

void addShmhtTask(shmht *ctMap, int key, int isBackground)
{
    int pos;
    char keyStr[64];
    entry *e;
    cancellable *c;
    void *mapArray = (void *)ctMap + sizeof(shmht);

    sprintf(keyStr, "%d", key);
    pthread_rwlock_wrlock(ctMapRWLock);
    pos = shmhtFind(ctMap, keyStr);
    if (pos != -1){
        fprintf(stderr, "already exist cancellable task\n");
        pthread_rwlock_unlock(ctMapRWLock);
        exit(EXIT_FAILURE);
    }

    currPid = key;
    pos = _shmhtFindInsertPos(ctMap, keyStr);
    if (pos == -1){
        fprintf(stderr, "unfortunately ctmap is full\n");
        pthread_rwlock_unlock(ctMapRWLock);
        return ;
    }
    e = (entry *)(mapArray + pos * ctMap->pairSize);
    c = (cancellable *)( (void *) e + sizeof(entry) );

    e->isUse = 1;
    strcpy(e->key, keyStr);

    c->isCancellable = 0;
    c->estimatedRows = 0;
    c->finishedRows = 0;
    c->tid = key;
    c->startTime = 0;
    c->isBackground = isBackground;
    pthread_rwlock_unlock(ctMapRWLock);

    // also allocate lockmap for 
    mapArray = (void *)globalLockMap + sizeof(shmht);
    pthread_rwlock_wrlock(globalLockMapRWLock);
    pos = _shmhtFindInsertPos(globalLockMap, keyStr);
    if (pos == -1){
        fprintf(stderr, "error: globalLockMap is full but ctmap is not\n");
        pthread_rwlock_unlock(globalLockMapRWLock);
        return ;
    }
    e = (entry *)(mapArray + pos * globalLockMap->pairSize);
    e->isUse = 1;
    pthread_rwlock_unlock(globalLockMapRWLock);
    strcpy(e->key, keyStr);

}

void removeShmhtTask(shmht *map, int key)
{
    int pos;
    entry *e;
    shmht *shmhtRscMap, *shmhtLockMap;
    cancellable *c;
    void *mapArray = (void *)map + sizeof(shmht);

    if (map == NULL || map->size == 0){
        return ;
    }
    char keyStr[64];
    sprintf(keyStr, "%d", key);
    pthread_rwlock_wrlock(ctMapRWLock);
    //shmhtRemove(map, keyStr);
    pos = shmhtFind(map, keyStr);
    if (pos == -1){
        return ;
    }
    e = (entry *)(mapArray + pos * map->pairSize);
    e->isUse = 0;
    bzero(e->key, MAX_KEY_SIZE);

    c = (cancellable *)( (void *) e + sizeof(entry) );
    bzero(c, sizeof(cancellable));

    shmhtRscMap = (shmht *)((void *) c + sizeof(cancellable));
    mapArray = (void *)shmhtRscMap + sizeof(shmht);
    bzero(mapArray, shmhtRscMap->size * shmhtRscMap->pairSize);

    pthread_rwlock_unlock(ctMapRWLock);

    // also remove from global lock map
    mapArray = (void *)globalLockMap + sizeof(shmht);
    pthread_rwlock_wrlock(globalLockMapRWLock);
    pos = shmhtFind(globalLockMap, keyStr);
    if (pos == -1){
        pthread_rwlock_unlock(globalLockMapRWLock);
        return ;
    }
    e = (entry *)(mapArray + pos * map->pairSize);
    e->isUse = 0;
    bzero(e->key, MAX_KEY_SIZE);

    shmhtLockMap = (shmht *)((void *)e + sizeof(entry));
    mapArray = (void *)shmhtLockMap + sizeof(shmht);
    bzero(mapArray, shmhtLockMap->size * shmhtLockMap->pairSize);
    pthread_rwlock_unlock(globalLockMapRWLock);

}

void shmhtStartTask(shmht *ctMap)
{
    int pid = currPid;
    char keyStr[64];
    cancellable *c;
    rscValue *rsc;
    shmht *shmhtRscMap;
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);

    sprintf(keyStr, "%d", pid);
    c = (cancellable *)shmhtGet(ctMap, keyStr);
    if (c == NULL){
        return ;
    }

    c->startTime = tp.tv_sec * 1000000000L + tp.tv_nsec;
    // we need to save last task information
    c->isCancellable = 1;
    if (clientMonitorMode){
        shmhtRscMap = (shmht *)((void *) c + sizeof(cancellable));
        rsc = (rscValue *)shmhtGet(shmhtRscMap, "cpu");
        if (rsc == NULL){
            rsc = (rscValue *) shmhtInsert(shmhtRscMap, "cpu", 0);
            rsc->type = CPUCYCLE;
            c->maxRscNum++;
        }
        rsc = (rscValue *)shmhtGet(shmhtRscMap, "io");
        if (rsc == NULL){
            rsc = (rscValue *) shmhtInsert(shmhtRscMap, "io", 0);
            rsc->type = IOWAIT;
            rsc->waitValue = 0;
            c->maxRscNum++;
        }
        return ;
    }
    unsigned long long ctime = readThdCpuTime(pid, pid);
    unsigned long long ioDelay = readThreadIOStats(pid, pid);
    unsigned long long ioUsage = readThreadIOUsage(pid, pid);
    c->lastTaskCpuTime = ctime;
    c->lastTaskIoDelay = ioDelay;
    c->lastTaskIoBytes = ioUsage;

    shmhtRscMap = (shmht *)((void *) c + sizeof(cancellable));

    rsc = (rscValue *)shmhtGet(shmhtRscMap, "cpu");
    if (rsc == NULL){
		rsc = (rscValue *) shmhtInsert(shmhtRscMap, "cpu", ctime);
		rsc->type = CPUCYCLE;
        c->maxRscNum++;
	}
    /*
    // TODO: since this api and readShmhtTask will be scheduled in different thread, this may cause conflict
	else{
		rsc->value = ctime;
	}
    */

    rsc = (rscValue *)shmhtGet(shmhtRscMap, "io");
	if (rsc == NULL){
		rsc = (rscValue *) shmhtInsert(shmhtRscMap, "io", ioUsage);
		rsc->type = IOWAIT;
		rsc->waitValue = ioDelay;
        c->maxRscNum++;
	}
	else{
		rsc->value = ioUsage;
		rsc->waitValue = ioDelay;
	}
}

void shmhtEndTask(shmht *ctMap)
{
    int pid = currPid;
    char keyStr[64];
    entry *e;
    cancellable *c;
    rscValue *rsc;
    shmht *shmhtRscMap;
    void *mapArray;

    sprintf(keyStr, "%d", pid);
    c = (cancellable *)shmhtGet(ctMap, keyStr);
    if (c == NULL){
        return ;
    }

    c->isCancellable = 0;
    c->estimatedRows = 0;
    c->finishedRows = 0;
    if (!c->isBackground)
        c->finishedRequests++;
    
    shmhtRscMap = (shmht *)((void *) c + sizeof(cancellable));
    mapArray = (void *)shmhtRscMap + sizeof(shmht);
    for (unsigned int j = 0; j < shmhtRscMap->size; ++j){
        e = (entry *)(mapArray + j * shmhtRscMap->pairSize);
        rsc = (rscValue *)((void *)e + sizeof(entry));

        if (!e->isUse){
            continue;
        }

        if (clientMonitorMode && (rsc->type == CPUCYCLE || rsc->type == IOWAIT)){
            continue;
        }
        rsc->value = 0;
        rsc->waitValue = 0;
        rsc->oldValue = 0;
        rsc->oldWaitValue = 0;
    }

}

cancellable *shmhtGetCancellable(int key)
{
    char keyStr[64];
    cancellable *c;

    sprintf(keyStr, "%d", key);
    c = (cancellable *)shmhtGet(m, keyStr);
    return c;
}

long long shmhtGetUsage(cancellable *c, char *key)
{
    shmht *shmhtRscMap;
    rscValue *val;
    shmhtRscMap = (shmht *)((void *) c + sizeof(cancellable));

    val = (rscValue *) shmhtGet(shmhtRscMap, key);
    return val->value;
}

int shmhtGetRscList(cancellable *c, char **rscList)
{
    unsigned int i, j = 0;
    shmht *shmhtRscMap = (shmht *)((void *) c + sizeof(cancellable));
    void *mapArray = (void *)shmhtRscMap + sizeof(shmht);
    for (i = 0; i < shmhtRscMap->size; ++i){
        entry *e = (entry *)(mapArray + i * shmhtRscMap->pairSize);
        if (e->isUse){
            strcpy(rscList[j++], e->key);
        }
    }

    return j;       //return number of resources
}

void shmhtSetEstimatedJob(long long estimatedJob)
{
    int pid = currPid;
	char keyStr[MAX_KEY_SIZE];
	cancellable *c;

	// I find this problem because when we pkill the postgres, the shared memory becomes all zero even though some process still need to do some clearup work
	if (m->size == 0){
		return ;
	}

	if (m == NULL){
        return ;
    }

    sprintf(keyStr, "%d", pid);

    c = (cancellable *)shmhtGet(m, keyStr);
    if (c == NULL){
        return ;
    }

    c->estimatedRows = estimatedJob;
}

void shmhtAddFinishedJob(long long finishedJob)
{
    int pid = currPid;
	char keyStr[MAX_KEY_SIZE];
	cancellable *c;

	// I find this problem because when we pkill the postgres, the shared memory becomes all zero even though some process still need to do some clearup work
	if (m->size == 0){
		return ;
	}

	if (m == NULL){
        return ;
    }

    sprintf(keyStr, "%d", pid);

    c = (cancellable *)shmhtGet(m, keyStr);
    if (c == NULL){
        return ;
    }
    
    c->finishedRows += finishedJob;
}

// rerun

void disableRerun()
{
    void *appTid;
    int tid = getCurrTid();
    if (threadMap == NULL || tidChangeMap == NULL){
        return ;
    }

    if ((appTid = get(tidChangeMap, tid))){
        cancellable *c = (cancellable *) get(threadMap, (unsigned long) appTid);
        c->needRerun = 0;
    }
}

int rerunPolicy()
{
    void *appTid;
    struct timespec tp;
    int ctime;

    int tid = getCurrTid();
    if (threadMap == NULL || tidChangeMap == NULL){
        return 0;
    }

    if ((appTid = get(tidChangeMap, tid))){
        cancellable *c = (cancellable *) get(threadMap, (unsigned long) appTid);
        if (!c->needRerun){
            return 0;
        }

        clock_gettime(CLOCK_REALTIME, &tp);
        ctime = tp.tv_sec;
        if (ctime - c->lastKillTime >= RERUN_GAP){
            return 1;
        }
    }


    return 0;
}
