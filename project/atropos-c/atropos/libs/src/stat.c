#include "global.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <cancellable.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include "stat.h"

#define PERF_CD_UPPER_LIMIT 100
#define TPS_THRESHOLD 10

int currPid = 0;
pthread_key_t currTid;
int startAutoCancel = 0;
hashMap *threadMap = NULL;
hashMap *tidChangeMap = NULL;
hashMap *thdLockMap = NULL;
pthread_rwlock_t mapRWLock, tidChangeMapRWLock, thdLockMapRWLock;
volatile int monitorThdInterval = 4;
volatile int currentMonitorThdInterval = 4;
volatile int perfCrash = 0;
volatile int perfWarn = 0;
volatile int perfCD = 0;
volatile int abortMonitor = 0;
volatile int needTrackAnyway = 0;
volatile int c_tps=0;
int pickPolicyOption = 0;					// decide which cancel policy to use
int clientMonitorMode = 0;					// monitor system resource usage in client level rather than request level to decrease overhead brought by reading /procc/stat
long estimateOutputLineSize = 118;

// rerun data structure
char rerunQueryString[512];				// record the string of the request for rerun
char currQueryString[512];				// current executed request
long long currQueryStringLength = 0;	// current executed request length
long long rerunQueryStringLength = 0;	// rerun request length;
long long killedTime = 0;				// time of being killed

int killBackground = 0;

int (*cancellableSchedule)(hashMap candidateList);

shmht *m = NULL;
shmht* globalLockMap = NULL;

void (*activateCancel)(unsigned long);
void (*activatePolicy)(void);
void (*monitorProgress)(unsigned long, FILE *fp);

// multi-thread model

void destructorFunction(void *data) {
    free(data);
}

void setCurrTid(int tid)
{
	int* data = (int*)malloc(sizeof(int));
    *data = tid;
    pthread_setspecific(currTid, data);
}

inline int getCurrTid(void)
{
	if (!startAutoCancel){
		return 0;
	}

	int *data = (int*)pthread_getspecific(currTid);
	if (data == NULL){
		return 0;
	}
	return (*data);
}

// system rsc tracing

void readStat(unsigned int pid, pthread_t tid, cancellable *c)
{
	FILE *fp;
	char thdFileName[50], thdIoFileName[50], line[512];
	int i;
	unsigned long long uTime, sTime, ioDelay;
	unsigned long long readBytes = 0, writeBytes = 0;

	// thread tsta file: /proc/[pid]/task/[tid]/stat
	genThreadStatsFileName(thdFileName, pid, tid);
	genThreadIoFileName(thdIoFileName, pid, tid);
	
	if ((fp = fopen(thdFileName, "r")) == NULL)
		return;

	// we extract user time, system time and aggregated block io delay from stat file
	while (fgets(line, sizeof(line), fp) != NULL) {
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
	}

	int pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, "cpuUsage");
	if (pos == -1){
		insertStrKeyItem(c->rscMap, "cpuUsage", (void *)(uTime + sTime), 0, CPUCYCLE);
	}
	else{
		c->rscMap->mapArray[pos].value = (void *)(uTime + sTime);
		c->rscMap->mapArray[pos].clientVal = (void *)(uTime + sTime);
		c->rscMap->mapArray[pos].rscMtc.usageVal = uTime + sTime;
	}

	pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, "ioDelay");
	if (pos == -1){
		insertStrKeyItem(c->rscMap, "ioDelay", (void *)0, 0, IOWAIT);
		pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, "ioDelay");
		c->rscMap->mapArray[pos].rscMtc.waitVal = ioDelay;
		c->rscMap->mapArray[pos].clientVal = ioDelay;
	}
	else{
		c->rscMap->mapArray[pos].rscMtc.waitVal = ioDelay;
		c->rscMap->mapArray[pos].clientVal = ioDelay;
	}

	fclose(fp);
	
	if ((fp = fopen(thdIoFileName, "r")) == NULL)
		return;

	int count = 0;
	while (fgets(line, sizeof(line), fp) != NULL) {
		count++;

		if (count == 5){
			i = sscanf(line, "%*s %llu", &readBytes);
		}
		else if (count == 6){
			i = sscanf(line, "%*s %llu", &writeBytes);
		}
	}

	c->rscMap->mapArray[pos].value = readBytes + writeBytes;
	c->rscMap->mapArray[pos].rscMtc.usageVal = readBytes + writeBytes;
	fclose(fp);
}

// monitor Performance, but need revision to directly monitor in app

void closeMonitor(void)
{
	abortMonitor = 1;
}

static inline void detectPerfAbnormal(long long tps)
{
	FILE *fp;
    char *crashPath;
    crashPath = getenv("AUTOCANCEL_CRASH");

    if(!crashPath) {
      crashPath=CRASH_PATH;
    }
	fp = fopen(crashPath, "a");

	fprintf(fp, "tps: %lld\n", tps);
	if (!perfWarn){
		perfWarn = 1;
		monitorThdInterval = 1;	
	}
	else{
		perfCrash = 1;
		pthread_rwlock_rdlock(threadMap->rwLock);
		long long value = 0;
		if (pickPolicyOption == 0){
			value = pickPolicyDefault(fp);
			//value = shmhtPickPolicyDefault(fp);
		}
		else if (pickPolicyOption == 1){
			value = pickPolicyPredictBased(fp);
			//value = shmhtPickPolicyPredictBased(fp);
		}
		else if (pickPolicyOption == 2){
			value = pickPolicyMultiObj(fp);
			//value = shmhtPickPolicyMultiObj(fp);
		}
		else if (pickPolicyOption == 3){
			value = pickPolicyMultiObjPredictBased(fp);
			//value = shmhtPickPolicyMultiObjPredictBased(fp);
		}
		else{
			value = pickPolicyDefault(fp);
			//value = shmhtPickPolicyDefault(fp);
		}
		fprintf(fp, "pick: %d\n", value);
		//killBackground = 1;		// debug for trying to pause purge
		activateCancel(value);
		pthread_rwlock_unlock(threadMap->rwLock);

		/* debug */
		time_t st;
		struct tm *stimeinfo;  //结构体
		time(&st);
		stimeinfo = localtime(&st);
		fprintf(fp, "send kill signal：%s\n", asctime(stimeinfo));

		resetPerfSignal(0);
	}
	fclose(fp);
}

inline void resetPerfSignal(int setAll)
{
	// setAll means disable both perfWarn flag and PerfCrash flag and recover monitor interval to 5
	perfWarn = setAll ? 0 : perfWarn;
	perfCrash = 0;
	monitorThdInterval = setAll ? 5 : monitorThdInterval;
}

long long mysqlDetectAbnormal(void){
	FILE *fp;
    char *crashPath;
    crashPath = getenv("AUTOCANCEL_CRASH");

    if(!crashPath) {
      crashPath=CRASH_PATH;
    }
	fp = fopen(crashPath, "a");

	pthread_rwlock_rdlock(threadMap->rwLock);
	long long value = 0;
	if (pickPolicyOption == 0){
		value = pickPolicyDefault(fp);
		//value = shmhtPickPolicyDefault(fp);
	}
	else if (pickPolicyOption == 1){
		value = pickPolicyPredictBased(fp);
		//value = shmhtPickPolicyPredictBased(fp);
	}
	else if (pickPolicyOption == 2){
		value = pickPolicyMultiObj(fp);
		//value = shmhtPickPolicyMultiObj(fp);
	}
	else if (pickPolicyOption == 3){
		value = pickPolicyMultiObjPredictBased(fp);
		//value = shmhtPickPolicyMultiObjPredictBased(fp);
	}
	else{
		value = pickPolicyDefault(fp);
		//value = shmhtPickPolicyDefault(fp);
	}
	fprintf(fp, "pick: %d\n", value);
	activateCancel(value);
	pthread_rwlock_unlock(threadMap->rwLock);

	/* debug */
	time_t st;
	struct tm *stimeinfo;  //结构体
	time(&st);
	stimeinfo = localtime(&st);
	fprintf(fp, "send kill signal：%s\n", asctime(stimeinfo));
	fclose(fp);
	return value;
}

long long pgsqlDetectAbnormal(void){
	FILE *fp;
    char *crashPath;
    crashPath = getenv("AUTOCANCEL_CRASH");

    if(!crashPath) {
      crashPath=CRASH_PATH;
    }
	fp = fopen(crashPath, "a");

	long long value = 0;
	if (pickPolicyOption == 0){
		value = shmhtPickPolicyDefault(fp);
	}
	else if (pickPolicyOption == 1){
		value = shmhtPickPolicyPredictBased(fp);
	}
	else if (pickPolicyOption == 2){
		value = shmhtPickPolicyMultiObj(fp);
	}
	else if (pickPolicyOption == 3){
		value = shmhtPickPolicyMultiObjPredictBased(fp);
	}
	else{
		value = shmhtPickPolicyDefault(fp);
	}
	fprintf(fp, "pick: %d\n", value);
	fflush(fp);
	activateCancel(value);

	/* debug */
	time_t st;
	struct tm *stimeinfo;  //结构体
	time(&st);
	stimeinfo = localtime(&st);
	fprintf(fp, "send kill signal：%s\n", asctime(stimeinfo));
	fclose(fp);
	return value;
}

void *pgsqlMonitor(void *arg)
{
	struct timespec tim, tim2;
	tim.tv_sec  = 0;
	tim.tv_nsec = 100000000L;

	int killCD = 0;
	long long oldTps = 0;
	queue tpsQueue, bufferTps;
	initializeQueue(&tpsQueue, 300);
    initializeQueue(&bufferTps, 50);
	char *policyOption, *degradePortion, *outputResult;
	double portion = 0.5;
	policyOption = getenv("AUTOCANCEL_POLICY");
	if (policyOption){
		pickPolicyOption = atoi(policyOption);
	}
	degradePortion = getenv("AUTOCANCEL_PORTION");
	if (degradePortion){
		portion = atof(degradePortion);
	}
	FILE *res;
	float resTime = 0;
	outputResult = getenv("AUTOCANCEL_SELF_PRODUCE_RESULT");
	if (outputResult){
		res = fopen(outputResult, "a");
	}
	void *mapArray = (void *)m + sizeof(shmht);
	//FILE *ifp = fopen("/home/catbeat/cases/case12/tps", "a");
	//FILE *fp = fopen("/home/catbeat/cases/case12/real_tps", "a");
	while (!abortMonitor){
		nanosleep(&tim, &tim2);
		
		long long tps = 0;
		int cancellableTaskNum = 0;
		for (unsigned int i = 0; i < m->size; ++i){
			entry *e = (entry *)(mapArray + i * m->pairSize);
			if (!e->isUse){
				continue;
			}

			cancellable *c = (cancellable *)((void *)e + sizeof(entry));

			tps += c->finishedRequests;
			if (c->isCancellable){
				cancellableTaskNum++;
			}
		}

		tps /= 10;
		enqueue(&tpsQueue, tps - oldTps);
		enqueue(&bufferTps, tps - oldTps);
		if (outputResult){
			fprintf(res, "%fs: %lld\n", resTime, tps - oldTps);
			resTime += 0.1;
		}
		oldTps = tps;
		//long long avgNormalTps = monitorAvgNormalTps(&tpsQueue);
		//long long avgNormalTps = monitorAvgNormalTps(&tpsQueue);
		long long avgNormalTps = averageQueue(&tpsQueue);
		long long avgBufferTps = averageQueue(&bufferTps);
		c_tps = tps;

		if (killCD){
			killCD--;
		}

		if (tpsQueue.size < tpsQueue.maxSize-1){
			//fprintf(ifp, "queue size: %u\n", tpsQueue.size);
			continue;
		}

		// in case tpsThreshold is 0 so it needs to be less and equal
		long long normalTpsRes = avgNormalTps*portion;
		//fprintf(ifp, "tps: %lld avg Buffer Tps: %lld, avg Normal Tps: %lld %lld\n", tps, avgBufferTps, avgNormalTps, normalTpsRes);	
		int abnormal = 1;
		for (int i = 0; i < bufferTps.maxSize; i += 10){
			long long requestPersec = 0;
			for (int j = 0; j < 10; j++){
				int index = (bufferTps.head + i + j) % bufferTps.maxSize;
				requestPersec += bufferTps.array[index];
			}
			requestPersec /= 10;
			//fprintf(fp, "tps: %lld\n", requestPersec);
			if (requestPersec >= normalTpsRes){
				abnormal = 0;
				break;
			}
		}

		if (abnormal){
			needTrackAnyway = 1;
		}
		else{
			needTrackAnyway = 0;
		}
				
		if (!killCD && abnormal && cancellableTaskNum >= 3){
			//resetPerfSignal(0);
			long long value = pgsqlDetectAbnormal();
			killCD = 100;
		}
    }

	if (outputResult){
		fclose(res);
	}
	//fclose(ifp);
	//fclose(fp);
	return arg;
}

void *mysqlMonitor(void *arg)
{
	struct timespec tim, tim2;
	tim.tv_sec  = 0;
	tim.tv_nsec = 100000000L;

	int killCD = 0;
	long long oldTps = 0;
	queue tpsQueue, bufferTps;
	initializeQueue(&tpsQueue, 300);
    initializeQueue(&bufferTps, 50);
	char *policyOption, *degradePortion;
	double portion = 0.5;
	policyOption = getenv("AUTOCANCEL_POLICY");
	if (policyOption){
		pickPolicyOption = atoi(policyOption);
	}
	degradePortion = getenv("AUTOCANCEL_PORTION");
	if (degradePortion){
		portion = atof(degradePortion);
	}
	//FILE *ifp = fopen("/data/zeyin/tps", "a");
	while (!abortMonitor){
		nanosleep(&tim, &tim2);
		
		long long tps = 0;
		int cancellableTaskNum = 0;
		for (unsigned int i = 0; i < threadMap->arraySize; ++i){
			if (threadMap->mapArray[i].inUse){
				cancellable *v = (cancellable *) threadMap->mapArray[i].value;

				if (v->tid == -1){
					//fprintf(fp, "\ntid: fucking equals to -1\n"); // debug
					continue;
				}

				tps += v->finishedRequests;
				if (v->isCancellable){
					cancellableTaskNum++;
				}
			}
		}

		tps /= 10;
		enqueue(&tpsQueue, tps - oldTps);
		enqueue(&bufferTps, tps - oldTps);
		oldTps = tps;
		//long long avgNormalTps = monitorAvgNormalTps(&tpsQueue);
		long long avgNormalTps = monitorAvgNormalTps(&tpsQueue);
		long long avgBufferTps = averageQueue(&bufferTps);
		c_tps = tps;

		if (killCD){
			killCD--;
		}

		if (tpsQueue.size < tpsQueue.maxSize-1){
			//fprintf(ifp, "queue size: %u\n", tpsQueue.size);
			continue;
		}

		// in case tpsThreshold is 0 so it needs to be less and equal
		long long normalTpsRes = avgNormalTps*portion;
		//fprintf(ifp, "tps: %lld avg Buffer Tps: %lld, avg Normal Tps: %lld %lld\n", tps, avgBufferTps, avgNormalTps/4, normalTpsRes);	
		int abnormal = 1;
		for (int i = 0; i < bufferTps.maxSize; i += 10){
			long long requestPersec = 0;
			for (int j = 0; j < 10; j++){
				int index = (bufferTps.head + i + j) % bufferTps.maxSize;
				requestPersec += bufferTps.array[index];
			}
			requestPersec /= 10;
			if (requestPersec >= normalTpsRes){
				abnormal = 0;
				break;
			}
		}
		
		if (abnormal){
			needTrackAnyway = 1;
		}
		else{
			needTrackAnyway = 0;
		}
				
		if (!killCD && abnormal && cancellableTaskNum > 3){
			//resetPerfSignal(0);
			long long value = mysqlDetectAbnormal();
			killCD = 100;
		}
		else if (!killCD && abnormal){
			killCD = 100;
		}
    }

	//fclose(ifp);
	return arg;
}

void *monitorPerfOutput(void *arg)
{
	struct timespec tim, tim2;
	tim.tv_sec  = 0;
	tim.tv_nsec = 99000000L;
	char *perfPath, *tpsThresholdStr, *policyOption;
	long long tpsThreshold = 0;
	perfPath = getenv("AUTOCANCEL_INPUT");
    if(!perfPath) {
      perfPath=PERF_OUTPUT;
    }
	tpsThresholdStr = getenv("AUTOCANCEL_TPS");
	if (!tpsThresholdStr){
		tpsThreshold = TPS_THRESHOLD;
	}
	else{
		tpsThreshold = atoi(tpsThresholdStr);
	}
	policyOption = getenv("AUTOCANCEL_POLICY");
	if (policyOption){
		pickPolicyOption = atoi(policyOption);
	}
	sleep(10);

	// check if result exist
	while (1 && !abortMonitor){
		if (!access(perfPath, 0)){
			break;
		}
		nanosleep(&tim, &tim2);
	}

	FILE *fp;
	fp = fopen(perfPath, "r");
	char *line = NULL;
	size_t len = 0, count = 0;
	ssize_t read;
	queue tpsQueue, currentTps, bufferTps;
	initializeQueue(&tpsQueue, 300);
    initializeQueue(&bufferTps, 50);

	// sleep for 20 seconds to let system warm up
	sleep(20);

	if (!fp){
		puts("No input performance report to monitor!!!\n");
		return arg;
	}

	while (!abortMonitor){
		nanosleep(&tim, &tim2);
		fseek(fp, -estimateOutputLineSize, SEEK_END);
		read = getline(&line, &len, fp);
		read = getline(&line, &len, fp);
		if (read < 90){
			continue;
		}
		else{
			char *p;

			// skip to tps
			p = strtok(line, ":");
			p = strtok(NULL, ":");
			p = strtok(NULL, " ");
			long long tps = atoll(p);

			enqueue(&tpsQueue, tps);
			enqueue(&bufferTps, tps);
			//long long avgNormalTps = monitorAvgNormalTps(&tpsQueue);
			long long avgNormalTps = averageQueue(&tpsQueue);
			long long avgBufferTps = averageQueue(&bufferTps);
            c_tps = tps;

            if(activatePolicy) {
              activatePolicy();
            } else {
              	if (perfWarn){
					if (perfCD < PERF_CD_UPPER_LIMIT){
						perfCD++;
					}
					else{
						perfCD = 0;
					}

					if (!perfCD && avgBufferTps >= avgNormalTps*8/10){
						resetPerfSignal(1);
					}
				}

				// in case tpsThreshold is 0 so it needs to be less and equal
				/* debug
				FILE *ifp = fopen("/data/zeyin/tps", "a");
				fprintf(ifp, "tps: %lld avg Buffer Tps: %lld, avg Normal Tps: %lld\n", tps, avgBufferTps, avgNormalTps/2);
				fclose(ifp);
				*/
				
				if (!perfCD && avgBufferTps < avgNormalTps*8/10){
					//resetPerfSignal(0);
					detectPerfAbnormal(tps);
				}
            }
		}

		if (read >= estimateOutputLineSize - 2){
			estimateOutputLineSize = read + 4;
		}
		count++;
	}

	fclose(fp);
	return arg;
}

int getThroughput() {
  return c_tps;
}

int isPerfCrash(void)
{
	return perfCrash;
}

// Instrumentation

void addRscUsage(long long value, char *rscName)
{
	int tid = getCurrTid();
	if (threadMap != NULL && tidChangeMap != NULL){
		void *appTid;
		if ((appTid = get(tidChangeMap, tid))){
			cancellable *c = (cancellable *) get(threadMap, (unsigned long) appTid);

			// there is possibility that the thread finished work but still have clearup procedure, so it will still call this function due to buffer reference
			if (!c){
				return ;
			}
			int pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, rscName);
			if (pos == -1){
				insertStrKeyItem(c->rscMap, rscName, (void *) value, 0, MEMUSAGE);
			}
			else{
				hashMapStrKeyItem *rscItem = &c->rscMap->mapArray[pos];
				c->rscMap->mapArray[pos].value = (void *)((long long) c->rscMap->mapArray[pos].value + value);
				rscItem->rscMtc.usageVal = rscItem->rscMtc.usageVal + value;
			}
		}
	}
}

void addRscUsageWait(long long value, char *rscName)
{
	int tid = getCurrTid();
	if (threadMap != NULL && tidChangeMap != NULL){
		void *appTid;
		if ((appTid = get(tidChangeMap, tid))){
			cancellable *c = (cancellable *) get(threadMap, (unsigned long) appTid);
			if (!c){
				return ;
			}
			int pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, rscName);
			if (pos == -1){
				return ;
			}
			else{
				hashMapStrKeyItem *rscItem = &c->rscMap->mapArray[pos];
				c->rscMap->mapArray[pos].clientVal = (void *)((long long) c->rscMap->mapArray[pos].clientVal + value);
				rscItem->rscMtc.waitVal = rscItem->rscMtc.waitVal + value;
			}
		}
	}
}

void addRscOccupy(void *thdPtr, char *rscName)
{
	int tid = getCurrTid();
	void *appTid;
	if (threadMap == NULL || tidChangeMap == NULL){
		return ;
	}
	appTid = get(tidChangeMap, tid);
	if (appTid == NULL){
		return ;
	}

	lockValue *lv = (lockValue *) malloc(sizeof(lockValue));
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	long long cTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;
	lv->id = tid;
	lv->timeStamp = cTime;
	strncpy(lv->relatedRsc, rscName, 50);
	if (thdLockMap){
		insert(thdLockMap, (unsigned long long) thdPtr, lv);
		if (threadMap != NULL && tidChangeMap != NULL && (appTid = get(tidChangeMap, tid))){
			cancellable *c = (cancellable *) get(threadMap, (unsigned long) appTid);
			if (!c){
				return ;
			}
			int pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, rscName);
			if (pos == -1){
				insertStrKeyItem(c->rscMap, rscName, 0, 0, QUEUEMODE);
			}
		}
	}
}

void getRscOccupy(void *thdPtr, char *rscName)
{
	int tid = getCurrTid();
	lockValue *lv = NULL;
	if (thdLockMap){
		lv = (lockValue *) get(thdLockMap, (unsigned long long) thdPtr);
	}
	else{
		return ;
	}

	if (!lv){
		return ;
	}

	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	unsigned long long cTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;

	unsigned long long value = cTime - lv->timeStamp;
	if (threadMap != NULL && tidChangeMap != NULL){
		void *appTid;
		if ((appTid = get(tidChangeMap, tid))){
			cancellable *c = (cancellable *) get(threadMap, (unsigned long) appTid);
			if (!c){
				return ;
			}
			int pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, rscName);
			if (pos == -1){
				insertStrKeyItem(c->rscMap, rscName, (void *) value, 0, QUEUEMODE);
			}
			else{
				hashMapStrKeyItem *rscItem = &c->rscMap->mapArray[pos];
				rscItem->rscMtc.usageVal = rscItem->rscMtc.usageVal + value;
			}
		}
	}

	if (thdLockMap){
		removeEntry(thdLockMap, (unsigned long long)thdPtr, 1);
	}
}

void addRscWait(char *rscName)
{
	int tid = getCurrTid();
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	unsigned long long cTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;

	if (threadMap != NULL && tidChangeMap != NULL){
		void *appTid;
		if ((appTid = get(tidChangeMap, tid))){
			cancellable *c = (cancellable *) get(threadMap, (unsigned long) appTid);
			int pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, rscName);
			if (pos == -1){
				insertStrKeyItem(c->rscMap, rscName, 0, cTime, QUEUEMODE);
			}
			else{
				c->rscMap->mapArray[pos].timeStamp = cTime;
				//printf("add %s %lld\n", rscName, cTime);
			}
		}
	}
}

void rmRscWait(char *rscName)
{
	int tid = getCurrTid();
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	unsigned long long cTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;

	if (threadMap != NULL && tidChangeMap != NULL){
		void *appTid;
		if ((appTid = get(tidChangeMap, tid))){
			cancellable *c = (cancellable *) get(threadMap, (unsigned long) appTid);
			int pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, rscName);
			if (pos == -1){
				perror("BEFORE REMOVE WE FIRST NEED ONE!!!\n");
				exit(-1);
			}
			else{
				hashMapStrKeyItem *rscItem = &c->rscMap->mapArray[pos];
				c->rscMap->mapArray[pos].value = (void *)((long long) c->rscMap->mapArray[pos].value + cTime - c->rscMap->mapArray[pos].timeStamp);
				c->rscMap->mapArray[pos].clientVal = (void *)((long long) c->rscMap->mapArray[pos].clientVal + cTime - c->rscMap->mapArray[pos].timeStamp);
				rscItem->rscMtc.waitVal = (rscItem->rscMtc.waitVal + cTime - rscItem->timeStamp);
				c->rscMap->mapArray[pos].timeStamp = 0;
			}
		}
	}
}

void addRscHold(void *rscPtr, char *rscName)
{
	long long cTime;
	struct timespec tp;
	int tid = getCurrTid();
	void *appTid;
	if (threadMap == NULL || tidChangeMap == NULL){
		return ;
	}
	appTid = get(tidChangeMap, tid);
	if (appTid == NULL){
		return ;
	}

	clock_gettime(CLOCK_REALTIME, &tp);
	cTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;

	lockValue *lv = (lockValue *) malloc(sizeof(lockValue));
	lv->id = tid;
	lv->timeStamp = cTime;
	strncpy(lv->relatedRsc, rscName, 50);
	if (threadMap != NULL && tidChangeMap != NULL){
		cancellable *c = (cancellable *) get(threadMap, (unsigned long) appTid);
		if (!c){
			return ;
		}

		insert(c->occupyMap, (unsigned long long) rscPtr, lv);
		
		int pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, rscName);
		if (pos == -1){
			insertStrKeyItem(c->rscMap, rscName, 0, 0, THREAD);
		}
	}
}

void getRscHold(void *rscPtr, char *rscName)
{
	unsigned long long cTime, value;
	void *appTid;
	struct timespec tp;
	cancellable *c = NULL;
	lockValue *lv = NULL;
	hashMapStrKeyItem *rscItem;
	int tid = getCurrTid();
	if (threadMap == NULL || tidChangeMap == NULL){
		return ;
	}

	if ((appTid = get(tidChangeMap, tid))){
		c = (cancellable *) get(threadMap, (unsigned long) appTid);
		if (!c){
			return ;
		}

		clock_gettime(CLOCK_REALTIME, &tp);
		cTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;
		lv = (lockValue *) get(c->occupyMap, (unsigned long long) rscPtr);
		value = cTime - lv->timeStamp;

		int pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, rscName);
		if (pos == -1){
			insertStrKeyItem(c->rscMap, rscName, (void *) value, 0, THREAD);
		}
		else{
			rscItem = &c->rscMap->mapArray[pos];
			rscItem->rscMtc.usageVal = rscItem->rscMtc.usageVal + value;
		}

		removeEntry(c->occupyMap, (unsigned long long) rscPtr, 1);
	}
}

void addRscHoldWait(char *rscName)
{
	int tid = getCurrTid();
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	unsigned long long cTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;

	if (threadMap != NULL && tidChangeMap != NULL){
		void *appTid;
		if ((appTid = get(tidChangeMap, tid))){
			cancellable *c = (cancellable *) get(threadMap, (unsigned long) appTid);
			int pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, rscName);
			if (pos == -1){
				insertStrKeyItem(c->rscMap, rscName, 0, cTime, THREAD);
			}
			else{
				c->rscMap->mapArray[pos].timeStamp = cTime;
				//printf("add %s %lld\n", rscName, cTime);
			}
		}
	}
}

void rmRscHoldWait(char *rscName)
{
	int tid = getCurrTid();
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	unsigned long long cTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;

	if (threadMap != NULL && tidChangeMap != NULL){
		void *appTid;
		if ((appTid = get(tidChangeMap, tid))){
			cancellable *c = (cancellable *) get(threadMap, (unsigned long) appTid);
			int pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, rscName);
			if (pos == -1){
				perror("BEFORE REMOVE WE FIRST NEED ONE!!!\n");
				exit(-1);
			}
			else{
				hashMapStrKeyItem *rscItem = &c->rscMap->mapArray[pos];
				c->rscMap->mapArray[pos].value = (void *)((long long) c->rscMap->mapArray[pos].value + cTime - c->rscMap->mapArray[pos].timeStamp);
				c->rscMap->mapArray[pos].clientVal = (void *)((long long) c->rscMap->mapArray[pos].clientVal + cTime - c->rscMap->mapArray[pos].timeStamp);
				rscItem->rscMtc.waitVal = (rscItem->rscMtc.waitVal + cTime - rscItem->timeStamp);
				c->rscMap->mapArray[pos].timeStamp = 0;
			}
		}
	}
}

// shmht

static void shmhtReadStat(int pid, cancellable *c)
{
	int i;
	FILE *fp;
	shmht *rscMap;
	rscValue *valPtr;
	long long val;
	unsigned long long uTime, sTime, ioDelay;
	unsigned long long readBytes = 0, writeBytes = 0;
	char procFileName[50], procIoFileName[50], line[512];

	// read cpu cycle and iodelay
	genProcStatsFileName(procFileName, pid);
	if ((fp = fopen(procFileName, "r")) == NULL)
		return ;

	while (fgets(line, sizeof(line), fp) != NULL) {
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
	}
	fclose(fp);

	// insert cpu cycle value
	val = uTime + sTime;
	rscMap = (shmht *)((void *)c + sizeof(cancellable));
	valPtr = (rscValue *)shmhtGet(rscMap, "cpu");
	if (valPtr == NULL){
		valPtr = (rscValue *) shmhtInsert(rscMap, "cpu", val);
		valPtr->type = CPUCYCLE;
		valPtr->oldValue = 0;
		c->maxRscNum++;
	}
	else{
		valPtr->oldValue = valPtr->value;
		valPtr->value = val;
	}

	// read iobytes read and iobytes write
	genProcIoFileName(procIoFileName, pid);
	if ((fp = fopen(procIoFileName, "r")) == NULL)
		return;

	int count = 0;
	while (fgets(line, sizeof(line), fp) != NULL) {
		count++;

		if (count == 5){
			i = sscanf(line, "%*s %llu", &readBytes);
		}
		else if (count == 6){
			i = sscanf(line, "%*s %llu", &writeBytes);
		}
	}
	fclose(fp);

	// insert io value
	val = readBytes + writeBytes;
	valPtr = (rscValue *)shmhtGet(rscMap, "io");
	if (valPtr == NULL){
		valPtr = (rscValue *) shmhtInsert(rscMap, "io", val);
		valPtr->type = IOWAIT;
		valPtr->waitValue = ioDelay;
		valPtr->oldValue = 0;
		valPtr->oldWaitValue = 0;
		c->maxRscNum++;
	}
	else{
		valPtr->oldValue = valPtr->value;
		valPtr->oldWaitValue = valPtr->waitValue;
		valPtr->value = val;
		valPtr->waitValue = ioDelay;
	}
}

static void _addShmhtRscUsage(int pid, shmht *ctMap, long long val, char *rscName, enum rscType type)
{
	char keyStr[MAX_KEY_SIZE];
	cancellable *c;
	rscValue *valPtr;
	shmht *rscMap;

	// I find this problem because when we pkill the postgres, the shared memory becomes all zero even though some process still need to do some clearup work
	if (ctMap->size == 0){
		return ;
	}

	if (ctMap != NULL){
    	sprintf(keyStr, "%d", pid);

		c = (cancellable *)shmhtGet(ctMap, keyStr);
		if (c != NULL){
			rscMap = (shmht *)((void *)c + sizeof(cancellable));
			valPtr = (rscValue *)shmhtGet(rscMap, rscName);
			if (valPtr == NULL){
				valPtr = (rscValue *) shmhtInsert(rscMap, rscName, val);
				valPtr->type = type;
				c->maxRscNum++;
				if (type == QUEUEMODE){
					//fprintf(stderr, "add queue mode usage: %d %d %s\n", valPtr->type, type, rscName);
					rscMap = (shmht *)((void *)c + sizeof(cancellable));
					valPtr = (rscValue *)shmhtGet(rscMap, rscName);
					//fprintf(stderr, "valptr: %p rsc type: %d\n", valPtr, valPtr->type);	//debug
				}
				//shmhtGeneralInsert(rscMap, rscName, rscValue, value, val, NULL);
			}
			else{
				valPtr->value += val;
			}
		}
	}
}

void addShmhtRscUsage(shmht *ctMap, long long val, char *rscName)
{
	int pid = currPid;
	char keyStr[MAX_KEY_SIZE];
	cancellable *c;
	rscValue *valPtr;
	shmht *rscMap;

	if (ctMap->size == 0){
		return ;
	}

	if (ctMap != NULL){
    	sprintf(keyStr, "%d", pid);

		c = (cancellable *)shmhtGet(ctMap, keyStr);
		if (c != NULL){
			rscMap = (shmht *)((void *)c + sizeof(cancellable));
			valPtr = (rscValue *)shmhtGet(rscMap, rscName);
			if (valPtr == NULL){
				valPtr = (rscValue *) shmhtInsert(rscMap, rscName, val);
				valPtr->type = MEMUSAGE;
				c->maxRscNum++;
				//shmhtGeneralInsert(rscMap, rscName, rscValue, value, val, NULL);
			}
			else{
				valPtr->value += val;
			}
		}
	}
}

void addShmhtRscUsageWait(shmht *ctMap, long long val, char *rscName)
{
	int pid = currPid;
	char keyStr[MAX_KEY_SIZE];
	cancellable *c;
	rscValue *valPtr;
	shmht *rscMap;

	if (ctMap->size == 0){
		return ;
	}

	if (ctMap != NULL){
    	sprintf(keyStr, "%d", pid);

		c = (cancellable *)shmhtGet(ctMap, keyStr);
		if (c != NULL){
			rscMap = (shmht *)((void *)c + sizeof(cancellable));
			valPtr = (rscValue *)shmhtGet(rscMap, rscName);
			if (valPtr == NULL){
				valPtr = (rscValue *) shmhtInsert(rscMap, rscName, 0);
				valPtr->type = MEMUSAGE;
				valPtr->waitValue = val;
				c->maxRscNum++;
				//shmhtGeneralInsert(rscMap, rscName, rscValue, value, val, NULL);
			}
			else{
				valPtr->waitValue += val;
			}
		}
	}
}

static void _addShmhtRscUsageWait(int pid, shmht *ctMap, long long val, char *rscName, enum rscType type)
{
	char keyStr[MAX_KEY_SIZE];
	cancellable *c;
	rscValue *valPtr;
	shmht *rscMap;

	if (ctMap->size == 0){
		return ;
	}

	if (ctMap != NULL){
    	sprintf(keyStr, "%d", pid);

		c = (cancellable *)shmhtGet(ctMap, keyStr);
		if (c != NULL){
			rscMap = (shmht *)((void *)c + sizeof(cancellable));
			valPtr = (rscValue *)shmhtGet(rscMap, rscName);
			if (valPtr == NULL){
				valPtr = (rscValue *) shmhtInsert(rscMap, rscName, 0);
				valPtr->type = type;
				valPtr->waitValue = val;
				c->maxRscNum++;
				//shmhtGeneralInsert(rscMap, rscName, rscValue, value, val, NULL);
			}
			else{
				valPtr->waitValue += val;
			}
		}
	}
}

void addShmhtRscOccupy(void *rscPtr, char *rscName)
{
	int pid = currPid;
	char IdStr[MAX_KEY_SIZE];
	//char keyStr[MAX_KEY_SIZE];
	//char PtrStr[MAX_KEY_SIZE - PID_TO_KEY_MAX_SIZE];
	char PtrStr[MAX_KEY_SIZE];
	void *mapArray;
	lockValue *lockVal;
	struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    long long currentTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;

	// first check if current process is a registered task or just a process but we don't care
	if (m == NULL || m->size == 0){
		return ;
	}
	sprintf(IdStr, "%d", pid);
	cancellable *c = (cancellable *)shmhtGet(m, IdStr);
	if (c == NULL){
		return ;
	}

	if (globalLockMap ==NULL || globalLockMap->size == 0){
		return ;
	}

	if (globalLockMap != NULL){
		/* original add lock value */
		/* use pid and virtual address of the rsc to form the key */
		/*
		sprintf(keyStr, "%d", pid);
		sprintf(PtrStr, "%lld", (long long) rscPtr);

		strncat(keyStr, PtrStr, MAX_KEY_SIZE - PID_TO_KEY_MAX_SIZE);
		lockVal = (lockValue *)shmhtGet(globalLockMap, keyStr);
		if (lockVal != NULL){
			return ;
		}
		*/
		/* shouldn't be in the global Lock Map already */
		/*
		assert(lockVal == NULL);

		// insert lock value and add additional value such pid and rscName.
		shmhtGeneralInsert(globalLockMap, keyStr, lockValue, lockVal, timeStamp, currentTime, globalLockMapRWLock);
		lockVal->id = pid;
		strncpy(lockVal->relatedRsc, rscName, 50);

		// ask rscmap to create a slot for rsc
		_addShmhtRscUsage(pid, m, 0, rscName, QUEUEMODE);
		*/
		volatile shmht *shmhtLockMap = (shmht *) shmhtGet(globalLockMap, IdStr);
		if (shmhtLockMap == NULL){
			return ;
		}

		sprintf(PtrStr, "%lld", rscPtr);
		lockVal = (lockValue *) shmhtGet(shmhtLockMap, PtrStr);
		if (lockVal != NULL){
			return ;
		}

		assert(lockVal == NULL);
		shmhtGeneralInsert(shmhtLockMap, PtrStr, lockValue, lockVal, timeStamp, currentTime, NULL);
		lockVal->id = pid;
		strncpy(lockVal->relatedRsc, rscName, 50);

		_addShmhtRscUsage(pid, m, 0, rscName, QUEUEMODE);
	}
}

void getShmhtRscOccupy(void *rscPtr, char *rscName)
{
	int pid = currPid;
	long long ts;
	char keyStr[MAX_KEY_SIZE];
	//char PtrStr[MAX_KEY_SIZE - PID_TO_KEY_MAX_SIZE];
	char PtrStr[MAX_KEY_SIZE];
	void *lockValPtr;
	lockValue *lockVal;
	struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    long long currentTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;

	if (globalLockMap->size == 0){
		return ;
	}

	if (globalLockMap != NULL){
		/* original get shmht rsc occupy
		sprintf(keyStr, "%d", pid);
		sprintf(PtrStr, "%lld", (long long) rscPtr);

		strncat(keyStr, PtrStr, MAX_KEY_SIZE - PID_TO_KEY_MAX_SIZE);

		// previously I find pos = -1 but lockValPtr != NULL, so double check
		int pos = shmhtFind(globalLockMap, keyStr);
		if (pos == -1){
			return ;
		}

		lockValPtr = shmhtGet(globalLockMap, keyStr);
		if (lockValPtr == NULL){
			return ;
		}
		lockVal = (lockValue *) lockValPtr;
		if (lockVal == NULL){
			return ;
		}

		ts = lockVal->timeStamp;
		_addShmhtRscUsage(pid, m, currentTime - ts, rscName, QUEUEMODE);
		shmhtRemove(globalLockMap, keyStr);
		*/

		sprintf(keyStr, "%d", pid);
		/* hope this won't cause problem if comment
		int pos = shmhtFind(globalLockMap, keyStr);
		if (pos == -1){
			return ;
		}
		*/

		void *shmhtLockMapPtr = shmhtGet(globalLockMap, keyStr);
		if (shmhtLockMapPtr == NULL){
			return ;
		}
		shmht *shmhtLockMap = (shmht *) shmhtLockMapPtr;

		sprintf(PtrStr, "%lld", rscPtr);
		lockValPtr = shmhtGet(shmhtLockMap, PtrStr);
		if (lockValPtr == NULL){
			return ;
		}
		lockVal = (lockValue *) lockValPtr;
		if (lockVal == NULL){
			return ;
		}

		ts = lockVal->timeStamp;
		_addShmhtRscUsage(pid, m, currentTime - ts, rscName, QUEUEMODE);
		shmhtRemove(shmhtLockMap, PtrStr);
	}
}

void addShmhtRscWait(char *rscName)
{
	int pid = currPid;
	char keyStr[MAX_KEY_SIZE];
	cancellable *c;
	rscValue *valPtr;
	shmht *rscMap;

	struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    long long currentTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;

	if (m->size == 0){
		return ;
	}

	if (m != NULL){
    	sprintf(keyStr, "%d", pid);

		c = (cancellable *)shmhtGet(m, keyStr);
		if (c != NULL){
			rscMap = (shmht *)((void *)c + sizeof(cancellable));
			valPtr = (rscValue *)shmhtGet(rscMap, rscName);
			if (valPtr == NULL){
				shmhtGeneralInsert(rscMap, rscName, rscValue, valPtr, timeStamp, currentTime, NULL);
				c->maxRscNum++;
			}
			else{
				valPtr->timeStamp = currentTime;
			}
		}
	}
}

void rmShmhtRscWait(char *rscName)
{
	int pid = currPid;
	long long ts;
	char keyStr[MAX_KEY_SIZE];
	cancellable *c;
	rscValue *valPtr;
	shmht *rscMap;

	struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    long long currentTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;

	if (m->size == 0){
		return ;
	}

	if (m != NULL){
		sprintf(keyStr, "%d", pid);
		c = (cancellable *)shmhtGet(m, keyStr);
		if (c == NULL){
			//printf("not find cancellable: %d\n", pid);
			return ;
		}

		rscMap = (shmht *)((void *)c + sizeof(cancellable));
		valPtr = (rscValue *)shmhtGet(rscMap, rscName);
		if (valPtr == NULL){
			//printf("not find value\n");
			return ;
		}

		ts = valPtr->timeStamp;
		valPtr->timeStamp = 0;			// no longer wait so no timestamp until next wait
		_addShmhtRscUsageWait(pid, m, currentTime - ts, rscName, QUEUEMODE);
	}
}

static void shmhtMonitorTask(cancellable *c, shmht *map, FILE *fp)
{
	double slowdown;
    long long rscVal, waitVal, lastTaskVal, lastTaskWaitVal, rscUsage;
    entry *e, *lockEntry;
    lockValue *lv;
    void *globalLockMapArray;
    void *mapArray = (void *)map + sizeof(shmht);
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    long long currentTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;
	//fprintf(fp, "finished rows: %lld estimated rows: %lld\n", c->finishedRows, c->estimatedRows);
	/*
    for (unsigned int j = 0; j < map->size; ++j){
        e = (entry *)(mapArray + j * map->pairSize);
        rscValue *rsc = (rscValue *)((void *)e + sizeof(entry));
        if (!e->isUse){
            continue;
        }
        
        fprintf(fp, "rscName: %s ", e->key);
		if (rsc->type == CPUCYCLE){
			rscVal = rsc->value;
            waitVal = rsc->waitValue;
			lastTaskVal = c->lastTaskCpuTime;
			rscUsage = shmhtGetRscUsage(c, e->key);
			slowdown = shmhtAttributeSlowdown(c, e->key);

            fprintf(fp, "value: %lld wait value: %lld last task value: %lld rsc usage: %lld slowdown: %f\n", rscVal, waitVal, lastTaskVal, rscUsage, slowdown);
		}
		else if (rsc->type == IOWAIT){
			rscVal = rsc->value;
            waitVal = rsc->waitValue;
			lastTaskVal = c->lastTaskIoBytes;
			lastTaskWaitVal = c->lastTaskIoDelay;
			rscUsage = shmhtGetRscUsage(c, e->key);
			slowdown = shmhtAttributeSlowdown(c, e->key);

            fprintf(fp, "value: %lld wait value: %lld last task value: %lld last task wait value: %lld rsc usage: %lld slowdown: %f\n", rscVal, waitVal, lastTaskVal, lastTaskWaitVal, rscUsage, slowdown);
		}
        else if (rsc->type == QUEUEMODE){
            globalLockMapArray = (void *) globalLockMap + sizeof(shmht);
            waitVal = rsc->timeStamp != 0 ? rsc->waitValue + currentTime - rsc->timeStamp : rsc->waitValue;
            rscVal = rsc->value;

            pthread_rwlock_wrlock(globalLockMapRWLock);
            for (unsigned int l = 0; l < globalLockMap->size; ++l){
                lockEntry = (entry *)(globalLockMapArray + l * globalLockMap->pairSize);
                if (!lockEntry->isUse){
                    continue;
                }

                lv = (lockValue *) ((void *)lockEntry + sizeof(entry));
                if (lv && lv->id == c->tid && strcmp(lv->relatedRsc, e->key) == 0){
                    rscVal += currentTime - (unsigned long long) lv->timeStamp;
                }
            }
            pthread_rwlock_unlock(globalLockMapRWLock);

			rscUsage = shmhtGetRscUsage(c, e->key);
			slowdown = shmhtAttributeSlowdown(c, e->key);
            fprintf(fp, "value: %lld wait value: %lld rsc usage: %lld slowdown: %f\n", rscVal, waitVal, rscUsage, slowdown);
        }
        else if (rsc->type == MEMUSAGE){
            rscVal = rsc->value;
            waitVal = rsc->waitValue;
			rscUsage = shmhtGetRscUsage(c, e->key);
			slowdown = shmhtAttributeSlowdown(c, e->key);

            fprintf(fp, "value: %lld wait value: %lld rsc usage: %lld slowdown: %f\n", rscVal, waitVal, rscUsage, slowdown);
        }

        //
        //if (rsc->timeStamp){
            //fprintf(fp, "value: %lld wait value: %lld\n", rsc->value, rsc->waitValue + currentTime - rsc->timeStamp);
            //continue;
        }
        //fprintf(fp, "value: %lld wait value: %lld\n", rsc->value, rsc->waitValue);
        //
    }
	*/
}

// Api For Picking Policy

long long pickPolicyDefault(FILE *fp)
{
	double contentionLevel, maxContentionLevel = 0;
	cancellable *c, *maxRscCancel = NULL;

	// since some task may not use certain resource yet, so I try to get the task which uses the most number of resources.
	unsigned int maxRscSize = 0;
	for (unsigned int i = 0; i < threadMap->arraySize; ++i){
		if (threadMap->mapArray[i].inUse){
			c = (cancellable *) threadMap->mapArray[i].value;
			unsigned int currSize = c->rscMap->size;
			if (currSize > maxRscSize){
				maxRscSize = currSize;
				maxRscCancel = c;
			}
		}
	}

	if (maxRscCancel == NULL){
		return 0;
	}

	// get the resource which has worst contention level
	unsigned int mapSize = maxRscCancel->rscMap->arraySize;
	char *rscKey, *maxContentionRscKey = NULL;
	for (unsigned int i = 0; i < mapSize; ++i){
		if (!maxRscCancel->rscMap->mapArray[i].inUse){
			continue;
		}

		hashMapStrKeyItem *rscItem = &maxRscCancel->rscMap->mapArray[i];
		rscKey = rscItem->key;

		contentionLevel = getContentionLevel(rscKey);
		if (contentionLevel > maxContentionLevel){
			maxContentionLevel = contentionLevel;
			maxContentionRscKey = rscKey;
		}
	}

	if (!maxContentionRscKey){
		fprintf(fp, "No max contention rsc!!!\n");
		return 0;
	}
	fprintf(fp, "max contention rsc: %s\n", maxContentionRscKey);

	long long maxFinalVal = 0;
	long long result = 0;
	for (unsigned int i = 0; i < threadMap->arraySize; ++i){
		if (threadMap->mapArray[i].inUse){
			c = (cancellable *) threadMap->mapArray[i].value;
			if (!c->isCancellable){
				continue;
			}

			long long value = getRscUsage(c, maxContentionRscKey);
			fprintf(fp, "c: %d %lld\n", c->tid, value);
			if (value > maxFinalVal){
				maxFinalVal = value;
				result = threadMap->mapArray[i].key;
			}
		}
	}

	return result;
}

long long pickPolicyPredictBased(FILE *fp)
{
	double contentionLevel, maxContentionLevel = 0;
	cancellable *c, *maxRscCancel = NULL;
	unsigned int maxRscSize = 0;
	for (unsigned int i = 0; i < threadMap->arraySize; ++i){
		if (threadMap->mapArray[i].inUse){
			c = (cancellable *) threadMap->mapArray[i].value;
			unsigned int currSize = c->rscMap->size;
			if (currSize > maxRscSize){
				maxRscSize = currSize;
				maxRscCancel = c;
			}
		}
	}

	if (maxRscCancel == NULL){
		return 0;
	}

	unsigned int mapSize = maxRscCancel->rscMap->arraySize;
	char *rscKey, *maxContentionRscKey = NULL;
	fprintf(fp, "max contention level: %f\n", maxContentionLevel);
	for (unsigned int i = 0; i < mapSize; ++i){
		if (!maxRscCancel->rscMap->mapArray[i].inUse){
			continue;
		}

		hashMapStrKeyItem *rscItem = &maxRscCancel->rscMap->mapArray[i];
		rscKey = rscItem->key;

		contentionLevel = getContentionLevel(rscKey);
		fprintf(fp, "%s: cl: %f\n", rscKey, contentionLevel);
		if (contentionLevel > maxContentionLevel){
			maxContentionLevel = contentionLevel;
			maxContentionRscKey = rscKey;
		}
	}

	if (!maxContentionRscKey){
		return 0;
	}

	fprintf(fp, "max contention rsc: %s\n", maxContentionRscKey);

	long long maxFinalVal = 0;
	long long result = 0;
	for (unsigned int i = 0; i < threadMap->arraySize; ++i){
		if (threadMap->mapArray[i].inUse){
			c = (cancellable *) threadMap->mapArray[i].value;
			if (!c->isCancellable){
				continue;
			}

			// unlike baseline, we predict the future usage of that resource
			long long value = getRscUsage(c, maxContentionRscKey);
			double lp = getLeftProgress(c);
			fprintf(fp, "%d: %f %lld\n", c->tid, lp, value);
			// if left progress < 20% then don't kill it.
			if (lp <= 0.2){
				continue;
			}
			lp = lp/(1-lp);
			value = value * lp;
			if (value > maxFinalVal){
				maxFinalVal = value;
				result = threadMap->mapArray[i].key;
			}
		}
	}

	return result;
}

static long long getBenefit(cancellable *c, char *rscName, hashMapStrKey *contentionMap)
{
	int pos = _getStrKeyItem(contentionMap->mapArray, contentionMap->arraySize, rscName);
	double contentionLevel;
	if (pos == -1){
		contentionLevel = getContentionLevel(rscName);
		insertStrKeyDouble(contentionMap, rscName, contentionLevel);
	}
	else{
		contentionLevel = contentionMap->mapArray[pos].dValue;
	}

	if (contentionLevel > 0){
		return getRscUsage(c, rscName);
	}
	else{
		return 0;
	}

	return 0;
}

static long long getPredictBasedBenefit(cancellable *c, char *rscName, hashMapStrKey *contentionMap)
{
	int pos = _getStrKeyItem(contentionMap->mapArray, contentionMap->arraySize, rscName);
	double contentionLevel;
	if (pos == -1){
		contentionLevel = getContentionLevel(rscName);
		insertStrKeyDouble(contentionMap, rscName, contentionLevel);
	}
	else{
		contentionLevel = contentionMap->mapArray[pos].dValue;
	}

	if (contentionLevel > 0){
		return getFutureRscUsage(c, rscName);
	}
	else{
		return 0;
	}

	return 0;
}

long long pickPolicyMultiObj(FILE *fp)
{
	// store the ptr of cancellable
	unsigned int cursor = 0, cArraySize = 0, dArraySize = 0;
	long long lval = 0, rval = 0;
	double maxVal = 0;
	long long candidateArray[1000], dominatorArray[1000];
	char *rscName;
	cancellable *c, *rc, *result = NULL;
	hashMapStrKeyItem *mapArray;
	hashMapStrKey *contentionMap;
	contentionMap = createHashMapStrKey(40);

	for (unsigned int i = 0; i < threadMap->arraySize; ++i){
		if (threadMap->mapArray[i].inUse){
			c = (cancellable *) threadMap->mapArray[i].value;
			if (c->isCancellable){
				candidateArray[cursor] = c;
				cursor++;
			}
		}
	}

	// spawn domintator set
	cArraySize = cursor;
	cursor = 0;
	for (unsigned int i = 0; i < cArraySize; ++i){
		int beDominated;
		c = (cancellable *)candidateArray[i];
		mapArray = c->rscMap->mapArray;
		for (unsigned int j = 0; j < cArraySize; ++j){
			if (j == i){
				continue;
			}

			// suppose rc can dominate c
			beDominated = 1;
			rc = (cancellable *)candidateArray[j];
			for (unsigned int k = 0; k < c->rscMap->arraySize; ++k){
				if (!mapArray[k].inUse){
					continue;
				}

				rscName = mapArray[k].key;
				lval = getBenefit(c, rscName, contentionMap);
				rval = getBenefit(rc, rscName, contentionMap);

				// this means rc doesn't dominates c
				if (lval >= rval){
					beDominated = 0;
					break;
				}
			}

			// find a rc dominates c
			if (beDominated){
				break;
			}
		}

		if (!beDominated){
			dominatorArray[cursor] = c;
			cursor++;
		}
	}

	dArraySize = cursor;
	cursor = 0;

	for (; cursor < dArraySize; ++cursor){
		c = (cancellable *) dominatorArray[cursor];
		fprintf(fp, "dominator: %d\n", c->tid);

		double sVal = 0;
		mapArray = c->rscMap->mapArray;
		for (unsigned int i = 0; i < c->rscMap->arraySize; ++i){
			if (!mapArray[i].inUse){
				continue;
			}

			rscName = mapArray[i].key;
			long long sRscVal = 0;
			for (unsigned int j = 0; j < dArraySize; ++j){
				rc = (cancellable *) dominatorArray[j];

				sRscVal += getBenefit(rc, rscName, contentionMap);
			}
			long long taskRscVal = getBenefit(c, rscName, contentionMap);
			double val;
			if (sRscVal == 0){
				val = 0;
			}
			else{
				val = (double) taskRscVal / sRscVal * getStrKeyDouble(contentionMap, rscName);
			}
			if (mapArray[i].type == CPUCYCLE){
				fprintf(fp, "%s usage: %lld, last usage: %lld\n", rscName, mapArray[i].rscMtc.usageVal, mapArray[i].lastTaskCpuTime);
			}
			fprintf(fp, "%s benefit: %f slowdown: %f ctl: %f real ctl: %f tskRscVal: %lld sumRscVal: %lld usagw: %lld\n", rscName, val, attributeSlowdown(c, rscName), getStrKeyDouble(contentionMap, rscName), getContentionLevel(rscName), taskRscVal, sRscVal, getRscUsage(c, rscName));
			sVal += val;
		}

		if (sVal > maxVal){
			result = c;
			maxVal = sVal;
		}
	}

	if (result == NULL){
		return 0;
	}

	fprintf(fp, "pick: %d\n", result->tid);

	for (unsigned int i = 0; i < threadMap->arraySize; ++i){
		if (threadMap->mapArray[i].inUse){
			c = (cancellable *) threadMap->mapArray[i].value;
			if (c == result){
				return threadMap->mapArray[i].key;
			}
		}
	}

	return 0;
}

long long pickPolicyMultiObjPredictBased(FILE *fp)
{
	// store the ptr of cancellable
	unsigned int cursor = 0, cArraySize = 0, dArraySize = 0;
	long long lval = 0, rval = 0;
	double maxVal = 0;
	long long candidateArray[1000], dominatorArray[1000];
	char *rscName;
	cancellable *c, *rc, *result = NULL;
	hashMapStrKeyItem *mapArray;
	hashMapStrKey *contentionMap;
	contentionMap = createHashMapStrKey(40);

	for (unsigned int i = 0; i < threadMap->arraySize; ++i){
		if (threadMap->mapArray[i].inUse){
			c = (cancellable *) threadMap->mapArray[i].value;
			double lp = getLeftProgress(c);
			fprintf(fp, "mp: %d: %f\n", c->tid, lp);
			if (lp <= 0.2){
				continue;
			}

			if (c->isCancellable){
				candidateArray[cursor] = c;
				cursor++;
			}
		}
	}

	// spawn domintator set
	cArraySize = cursor;
	cursor = 0;
	if (cArraySize > 1){
		for (unsigned int i = 0; i < cArraySize; ++i){
			int beDominated;
			c = (cancellable *)candidateArray[i];
			mapArray = c->rscMap->mapArray;
			for (unsigned int j = 0; j < cArraySize; ++j){
				if (j == i){
					continue;
				}

				// suppose rc can dominate c
				beDominated = 1;
				rc = (cancellable *)candidateArray[j];
				for (unsigned int k = 0; k < c->rscMap->arraySize; ++k){
					if (!mapArray[k].inUse){
						continue;
					}

					rscName = mapArray[k].key;
					lval = getPredictBasedBenefit(c, rscName, contentionMap);
					rval = getPredictBasedBenefit(rc, rscName, contentionMap);

					// this means rc doesn't dominates c
					if (lval >= rval){
						beDominated = 0;
						break;
					}
				}

				// find a rc dominates c
				if (beDominated){
					break;
				}
			}

			if (!beDominated){
				dominatorArray[cursor] = c;
				cursor++;
			}
		}
	}
	else if (cArraySize == 1){
		// since we only have one candidate, there is no need to continue
		fprintf(fp, "we only have one candidate!!!: ");
		result = (cancellable *) candidateArray[0];
		fprintf(fp, "real tid: %d ", result->tid);
		for (unsigned int i = 0; i < threadMap->arraySize; ++i){
			if (threadMap->mapArray[i].inUse){
				c = (cancellable *) threadMap->mapArray[i].value;
				if (c == result){
					fprintf(fp, "ret tid: %llu\n", threadMap->mapArray[i].key);
					return threadMap->mapArray[i].key;
				}
			}
		}
	}


	dArraySize = cursor;
	cursor = 0;

	for (; cursor < dArraySize; ++cursor){
		c = (cancellable *) dominatorArray[cursor];
		fprintf(fp, "dominator: %d\n", c->tid);

		double sVal = 0;
		mapArray = c->rscMap->mapArray;
		for (unsigned int i = 0; i < c->rscMap->arraySize; ++i){
			if (!mapArray[i].inUse){
				continue;
			}

			rscName = mapArray[i].key;
			long long sRscVal = 0;
			for (unsigned int j = 0; j < dArraySize; ++j){
				rc = (cancellable *) dominatorArray[j];

				sRscVal += getPredictBasedBenefit(rc, rscName, contentionMap);
			}
			long long taskRscVal = getPredictBasedBenefit(c, rscName, contentionMap);
			double val;
			if (sRscVal == 0){
				val = 0;
			}
			else{
				val = (double) taskRscVal / sRscVal * getStrKeyDouble(contentionMap, rscName);
			}
			if (mapArray[i].type == CPUCYCLE){
				fprintf(fp, "%s usage: %lld, last usage: %lld\n", rscName, mapArray[i].rscMtc.usageVal, mapArray[i].lastTaskCpuTime);
			}
			fprintf(fp, "%s benefit: %f slowdown: %f ctl: %f real ctl: %f tskRscVal: %lld sumRscVal: %lld usagw: %lld\n", rscName, val, attributeSlowdown(c, rscName), getStrKeyDouble(contentionMap, rscName), getContentionLevel(rscName), taskRscVal, sRscVal, getRscUsage(c, rscName));
			sVal += val;
		}

		if (sVal > maxVal){
			result = c;
			maxVal = sVal;
		}
	}

	if (result == NULL){
		return 0;
	}

	for (unsigned int i = 0; i < threadMap->arraySize; ++i){
		if (threadMap->mapArray[i].inUse){
			c = (cancellable *) threadMap->mapArray[i].value;
			if (c == result){
				return threadMap->mapArray[i].key;
			}
		}
	}

	return 0;
}

long long shmhtPickPolicyDefault(FILE *fp)
{
	entry *e;
	char *rscName, *maxContentionRscKey = NULL;
	void *mapArray, *rscMapArray;
	shmht *shmhtRscMap;
	double contentionLevel, maxContentionLevel = 0;
	cancellable *c, *maxRscCancel = NULL;

	// since some task may not use certain resource yet, so I try to get the task which uses the most number of resources.
	unsigned int maxRscSize = 0;
	mapArray = (void *)m + sizeof(shmht);
	for (unsigned int i = 0; i < m->size; ++i){
		e = (entry *)(mapArray + i * m->pairSize);
		if (!e->isUse){
			continue;
		}

		c = (cancellable *)((void *)e + sizeof(entry));
		if (!c->isCancellable){
			continue;
		}

		if (c->maxRscNum > maxRscSize){
			maxRscSize = c->maxRscNum;
			maxRscCancel = c;
		}
	}

	if (maxRscCancel == NULL){
		return 0;
	}

	shmhtRscMap = (shmht *)((void *) maxRscCancel + sizeof(cancellable));
	rscMapArray = (void *)shmhtRscMap + sizeof(shmht);
	for (unsigned int i = 0; i < shmhtRscMap->size; ++i){
		e = (entry *)(rscMapArray + i * shmhtRscMap->pairSize);
		if (!e->isUse){
			continue;
		}

		rscName = e->key;
		contentionLevel = shmhtGetContentionLevel(rscName);
		fprintf(fp, "%s: %f\n", rscName, contentionLevel);
		if (contentionLevel > maxContentionLevel){
			maxContentionLevel = contentionLevel;
			maxContentionRscKey = rscName;
		}
	}

	if (!maxContentionRscKey){
		return 0;
	}
	
	fprintf(fp, "max contentioned rsc: %s\n", maxContentionRscKey);

	long long maxFinalVal = 0;
	long long result = 0; 
	for (unsigned int i = 0; i < m->size; ++i){
		e = (entry *)(mapArray + i * m->pairSize);
		if (!e->isUse){
			continue;
		}

		c = (cancellable *)((void *)e + sizeof(entry));
		if (!c->isCancellable){
			continue;
		}

		long long value = shmhtGetRscUsage(c, maxContentionRscKey);
		fprintf(fp, "%s: %lld\n", e->key, value);
		if (value > maxFinalVal){
			maxFinalVal = value;
			result = atol(e->key);
		}
	}

	fprintf(fp, "pick :%lld\n", result);
	return result;
}

long long shmhtPickPolicyPredictBased(FILE *fp)
{
	entry *e;
	char *rscName, *maxContentionRscKey = NULL;
	void *mapArray, *rscMapArray;
	shmht *shmhtRscMap;
	double contentionLevel, maxContentionLevel = 0;
	cancellable *c, *maxRscCancel = NULL;

	// since some task may not use certain resource yet, so I try to get the task which uses the most number of resources.
	unsigned int maxRscSize = 0;
	mapArray = (void *)m + sizeof(shmht);
	for (unsigned int i = 0; i < m->size; ++i){
		e = (entry *)(mapArray + i * m->pairSize);
		if (!e->isUse){
			continue;
		}

		c = (cancellable *)((void *)e + sizeof(entry));
		if (!c->isCancellable){
			continue;
		}

		if (c->maxRscNum > maxRscSize){
			maxRscSize = c->maxRscNum;
			maxRscCancel = c;
		}
	}

	if (maxRscCancel == NULL){
		return 0;
	}

	shmhtRscMap = (shmht *)((void *) maxRscCancel + sizeof(cancellable));
	rscMapArray = (void *)shmhtRscMap + sizeof(shmht);
	for (unsigned int i = 0; i < shmhtRscMap->size; ++i){
		e = (entry *)(rscMapArray + i * shmhtRscMap->pairSize);
		if (!e->isUse){
			continue;
		}

		rscName = e->key;
		contentionLevel = shmhtGetContentionLevel(rscName);
		if (contentionLevel > maxContentionLevel){
			maxContentionLevel = contentionLevel;
			maxContentionRscKey = rscName;
		}
	}

	if (!maxContentionRscKey){
		return 0;
	}
	
	long long maxFinalVal = 0;
	long long result = 0; 
	double lp = 0;
	for (unsigned int i = 0; i < m->size; ++i){
		e = (entry *)(mapArray + i * m->pairSize);
		if (!e->isUse){
			continue;
		}

		c = (cancellable *)((void *)e + sizeof(entry));
		if (!c->isCancellable){
			continue;
		}

		long long value = shmhtGetRscUsage(c, maxContentionRscKey);
		lp = getLeftProgress(c);
		fprintf(fp, "%d: %f %lld\n", c->tid, lp, value);
		if (lp <= 0.2){
			continue;
		}

		lp = lp/(1-lp);
		value = value *lp;
		if (value > maxFinalVal){
			maxFinalVal = value;
			result = atol(e->key);
		}
	}

	fprintf(fp, "pick :%lld\n", result);
	return result;
}

static long long shmhtGetBenefit(cancellable *c, char *rscName, hashMapStrKey *contentionMap)
{
	int pos = _getStrKeyItem(contentionMap->mapArray, contentionMap->arraySize, rscName);
	double contentionLevel;
	if (pos == -1){
		contentionLevel = shmhtGetContentionLevel(rscName);
		insertStrKeyDouble(contentionMap, rscName, contentionLevel);
	}
	else{
		contentionLevel = contentionMap->mapArray[pos].dValue;
	}

	if (contentionLevel > 0){
		return shmhtGetRscUsage(c, rscName);
	}
	else{
		return 0;
	}

	return 0;
}

static long long shmhtGetPredictBasedBenefit(cancellable *c, char *rscName, hashMapStrKey *contentionMap)
{
	int pos = _getStrKeyItem(contentionMap->mapArray, contentionMap->arraySize, rscName);
	double contentionLevel;
	if (pos == -1){
		contentionLevel = shmhtGetContentionLevel(rscName);
		insertStrKeyDouble(contentionMap, rscName, contentionLevel);
	}
	else{
		contentionLevel = contentionMap->mapArray[pos].dValue;
	}

	if (contentionLevel > 0){
		return shmhtGetFutureRscUsage(c, rscName);
	}
	else{
		return 0;
	}

	return 0;
}

long long shmhtPickPolicyMultiObj(FILE *fp)
{
	entry *e;
	char *rscName;
	shmht *shmhtRscMap;
	void *mapArray, *rscMapArray;
	cancellable *c, *rc, *result = NULL;
	unsigned int cursor = 0, cArraySize = 0, dArraySize = 0;
	long long lval = 0, rval = 0;
	double maxVal = 0;
	long long candidateArray[1000], dominatorArray[1000];
	hashMapStrKey *contentionMap;
	contentionMap = createHashMapStrKey(40);

	// spawn candidate set
	mapArray = (void *)m + sizeof(shmht);
	for (unsigned int i = 0; i < m->size; ++i){
		e = (entry *)(mapArray + i * m->pairSize);
		if (!e->isUse){
			continue;
		}

		c = (cancellable *)((void *)e + sizeof(entry));
		if (!c->isCancellable){
			continue;
		}

		candidateArray[cursor] = (long long) c;
		cursor++;
	}

	cArraySize = cursor;
	cursor = 0;

	for (unsigned int i = 0; i < cArraySize; ++i){
		int beDominated;
		c = (cancellable *) candidateArray[i];
		shmhtRscMap = (shmht *)((void *) c + sizeof(cancellable));
		rscMapArray = (void *)shmhtRscMap + sizeof(shmht);

		for (unsigned int j = 0; j < cArraySize; ++j){
			if (i == j){
				continue;
			}

			beDominated = 1;
			rc = (cancellable *) candidateArray[j];
			for (unsigned int k = 0; k < shmhtRscMap->size; ++k){
				e = (entry *)(rscMapArray + k * shmhtRscMap->pairSize);
				if (!e->isUse){
					continue;
				}

				rscName = e->key;
				lval = shmhtGetBenefit(c, rscName, contentionMap);
				rval = shmhtGetBenefit(rc, rscName, contentionMap);

				if (lval >= rval){
					beDominated = 0;
					break;
				}
			}

			if (beDominated){
				break;
			}
		}

		if (!beDominated){
			dominatorArray[cursor] = (long long) c;
			cursor++;
		}
	}

	dArraySize = cursor;
	cursor = 0;

	for (; cursor < dArraySize; ++cursor){
		c = (cancellable *) dominatorArray[cursor];
		fprintf(fp, "dominator: %d\n", c->tid);

		double sVal = 0, val = 0;
		long long sRscVal = 0, taskRscVal = 0;
		shmhtRscMap = (shmht *)((void *) c + sizeof(cancellable));
		rscMapArray = (void *)shmhtRscMap + sizeof(shmht);

		for (unsigned int i = 0; i < shmhtRscMap->size; ++i){
			e = (entry *)(rscMapArray + i * shmhtRscMap->pairSize);
			if (!e->isUse){
				continue;
			}

			rscName = e->key;
			sRscVal = 0;
			for (unsigned int j = 0; j < dArraySize; ++j){
				rc = (cancellable *) dominatorArray[j];
				sRscVal += shmhtGetBenefit(rc, rscName, contentionMap);
			}

			taskRscVal = shmhtGetBenefit(c, rscName, contentionMap);
			if (sRscVal == 0){
				val = 0;
			}
			else{
				val = (double) taskRscVal / sRscVal * getStrKeyDouble(contentionMap, rscName);
			}
			fprintf(fp, "%s benefit: %f\n", rscName, val);
			sVal += val;
		}

		if (sVal > maxVal){
			result = c;
			maxVal = sVal;
		}
	}

	if (result == NULL){
		return 0;
	}

	fprintf(fp, "multi-obj pick: %d\n", result->tid);

	return result->tid;
}

long long shmhtPickPolicyMultiObjPredictBased(FILE *fp)
{
	entry *e;
	char *rscName;
	shmht *shmhtRscMap;
	void *mapArray, *rscMapArray;
	cancellable *c, *rc, *result = NULL;
	unsigned int cursor = 0, cArraySize = 0, dArraySize = 0;
	long long lval = 0, rval = 0;
	double maxVal = 0;
	long long candidateArray[1000], dominatorArray[1000];
	hashMapStrKey *contentionMap;
	contentionMap = createHashMapStrKey(40);

	// spawn candidate set
	mapArray = (void *)m + sizeof(shmht);
	for (unsigned int i = 0; i < m->size; ++i){
		e = (entry *)(mapArray + i * m->pairSize);
		if (!e->isUse){
			continue;
		}

		c = (cancellable *)((void *)e + sizeof(entry));
		if (!c->isCancellable){
			continue;
		}

		candidateArray[cursor] = (long long) c;
		cursor++;
	}

	cArraySize = cursor;
	cursor = 0;

	for (unsigned int i = 0; i < cArraySize; ++i){
		int beDominated;
		c = (cancellable *) candidateArray[i];
		shmhtRscMap = (shmht *)((void *) c + sizeof(cancellable));
		rscMapArray = (void *)shmhtRscMap + sizeof(shmht);

		for (unsigned int j = 0; j < cArraySize; ++j){
			if (i == j){
				continue;
			}

			beDominated = 1;
			rc = (cancellable *) candidateArray[j];
			for (unsigned int k = 0; k < shmhtRscMap->size; ++k){
				e = (entry *)(rscMapArray + k * shmhtRscMap->pairSize);
				if (!e->isUse){
					continue;
				}

				rscName = e->key;
				lval = shmhtGetPredictBasedBenefit(c, rscName, contentionMap);
				rval = shmhtGetPredictBasedBenefit(rc, rscName, contentionMap);

				if (lval >= rval){
					beDominated = 0;
					break;
				}
			}

			if (beDominated){
				break;
			}
		}

		if (!beDominated){
			dominatorArray[cursor] = (long long) c;
			cursor++;
		}
	}

	dArraySize = cursor;
	cursor = 0;

	for (; cursor < dArraySize; ++cursor){
		c = (cancellable *) dominatorArray[cursor];
		fprintf(fp, "dominator: %d\n", c->tid);

		double sVal = 0, val = 0;
		long long sRscVal = 0, taskRscVal = 0;
		shmhtRscMap = (shmht *)((void *) c + sizeof(cancellable));
		rscMapArray = (void *)shmhtRscMap + sizeof(shmht);

		for (unsigned int i = 0; i < shmhtRscMap->size; ++i){
			e = (entry *)(rscMapArray + i * shmhtRscMap->pairSize);
			if (!e->isUse){
				continue;
			}

			rscName = e->key;
			sRscVal = 0;
			for (unsigned int j = 0; j < dArraySize; ++j){
				rc = (cancellable *) dominatorArray[j];
				sRscVal += shmhtGetPredictBasedBenefit(rc, rscName, contentionMap);
			}

			taskRscVal = shmhtGetPredictBasedBenefit(c, rscName, contentionMap);
			if (sRscVal == 0){
				val = 0;
			}
			else{
				val = (double) taskRscVal / sRscVal * getStrKeyDouble(contentionMap, rscName);
			}
			fprintf(fp, "%s benefit: %f\n", rscName, val);
			sVal += val;
		}

		if (sVal > maxVal){
			result = c;
			maxVal = sVal;
		}
	}

	if (result == NULL){
		return 0;
	}

	fprintf(fp, "multi-obj predict pick: %d\n", result->tid);

	return result->tid;
}

// Resource Tracing

static void monitorThreads(int pid, FILE *fp)
{
	struct timeval t;
    gettimeofday(&t, NULL);
	/* debug
    fprintf(fp,"\ncurrent time: %lu\n", t.tv_sec*1000000 + t.tv_usec);
	fprintf(fp, "map size: %d\n", threadMap->size);
	fprintf(fp, "lock map size: %u lock map array size: %u\n", thdLockMap->size, thdLockMap->arraySize);
	*/
	for (unsigned int i = 0; i < threadMap->arraySize; ++i){
		if (threadMap->mapArray[i].inUse){
			unsigned long appTid = threadMap->mapArray[i].key;
			cancellable *v = (cancellable *) threadMap->mapArray[i].value;

			if (v->tid == -1){
				//fprintf(fp, "\ntid: fucking equals to -1\n"); // debug
				continue;
			}

			//fprintf(fp, "\ntid: %d\n", v->tid); //debug
			readStat(pid, v->tid, v);

			if ( ((cancellable *) threadMap->mapArray[i].value)->isCancellable ){
				//monitorProgress(threadMap->mapArray[i].key, fp);
				//fprintf(fp, "c estimated rows: %lld c finished rows: %lld\n", v->estimatedRows, v->finishedRows); // debug
			}
			
			int mapSize = v->rscMap->arraySize;
			
			for (int i = 0; i < mapSize; ++i){
				if (!v->rscMap->mapArray[i].inUse || !v->isCancellable){
					continue;
				}

				struct timespec tp;
    			clock_gettime(CLOCK_REALTIME, &tp);

				if (v->rscMap->mapArray[i].type == CPUCYCLE){
					/* debug
					hashMapStrKeyItem *rscItem = &v->rscMap->mapArray[i];
					long long rscVal = rscItem->rscMtc.usageVal;
					long long oldRscVal = rscItem->rscMtc.oldUsageVal;
					rscItem->rscMtc.oldUsageVal = rscItem->rscMtc.usageVal;
					long long atime  = tp.tv_sec * 1000000000L + tp.tv_nsec;
					double slowdown = attributeSlowdown(v, rscItem->key);
					double clientIntervalSlowdown = attributeClientIntervalSlowdown(v, rscItem->key);
					fprintf(fp, "task start time: %lldns current time: %lldns  task total time: %fs\n", atime, v->startTime, (double) (atime - v->startTime) / 1000000000L);
					fprintf(fp,"%s: old cpu cycle: %lld current cpu cycle: %lld cpu cycle when last task finished: %lld slowdown: %f client value: %lld old client value: %lld client interval slow down: %f\n", rscItem->key, oldRscVal, rscVal, rscItem->lastTaskCpuTime, slowdown, (long long) rscItem->clientVal, (long long) rscItem->oldClientVal, clientIntervalSlowdown);
					rscItem->oldClientVal = rscItem->clientVal;
					*/
				}
				else if (v->rscMap->mapArray[i].type == IOWAIT){
					/* debug
					hashMapStrKeyItem *rscItem = &v->rscMap->mapArray[i];
					long long rscVal = rscItem->rscMtc.usageVal;
					long long oldRscVal = rscItem->rscMtc.oldUsageVal;
					long long rscWaitVal = rscItem->rscMtc.waitVal;
					long long oldRscWaitVal = rscItem->rscMtc.oldWaitVal;
					long long clientVal = (long long) rscItem->clientVal;
					long long oldClientVal = (long long) rscItem->oldClientVal;
					rscItem->rscMtc.oldUsageVal = rscItem->rscMtc.usageVal;

					double slowdown = attributeSlowdown(v, rscItem->key);
					double clientIntervalSlowdown = attributeClientIntervalSlowdown(v, rscItem->key);
					fprintf(fp, "%s: old Read Bytes: %lldB Read Bytes: %lldB Last Task's io usage: %lld old io delay: %lldcs io delay: %lldcs Last Task's io delay: %lld slowdown: %f client value: %lld old client value: %lld client slowdown: %f\n", rscItem->key, oldRscVal, rscVal, rscItem->lastTaskIoBytes, oldRscWaitVal, rscWaitVal, rscItem->lastTaskIoDelay, slowdown, clientVal, oldClientVal, clientIntervalSlowdown);
					rscItem->oldClientVal = rscItem->clientVal;
					*/
				}
				else if (v->rscMap->mapArray[i].type == MEMUSAGE){
					/* debug
					hashMapStrKeyItem *rscItem = &v->rscMap->mapArray[i];
					long long rscVal = rscItem->rscMtc.usageVal;
					long long oldRscVal = rscItem->rscMtc.oldUsageVal;
					long long rscWaitVal = rscItem->rscMtc.waitVal;
					long long oldRscWaitVal = rscItem->rscMtc.oldWaitVal;

					rscItem->rscMtc.oldUsageVal = rscItem->rscMtc.usageVal;
					rscItem->rscMtc.oldWaitVal = rscItem->rscMtc.waitVal;

					double slowdown = attributeSlowdown(v, rscItem->key);
					double clientIntervalSlowdown = attributeClientIntervalSlowdown(v, rscItem->key);
					fprintf(fp, "%s: old part time: %lldns part time: %lldns old part wait: %lldns part wait: %lldns slow down: %f client value: %lld old client valule: %lld client interval slow down: %f\n", rscItem->key, oldRscVal, rscVal, oldRscWaitVal, rscWaitVal, slowdown, (long long)rscItem->clientVal, (long long) rscItem->oldClientVal, clientIntervalSlowdown);
					rscItem->oldClientVal = rscItem->clientVal;
					*/
				}
				else if (v->rscMap->mapArray[i].type == QUEUEMODE){
					/* debug
					hashMapStrKeyItem *rscItem = &v->rscMap->mapArray[i];
					long long tid = v->tid;
					unsigned long long rscVal = rscItem->rscMtc.usageVal;
					// find lock occupy time
					unsigned long long cTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;
					
					pthread_rwlock_rdlock(thdLockMap->rwLock);
					for(unsigned int j = 0; j < thdLockMap->arraySize; ++j){
						hashMapItem *lockItem = &thdLockMap->mapArray[j];
						lockValue *lv;
						
						if (lockItem->inUse){
							lv = (lockValue *) lockItem->value;
							if (lv && lv->id == tid && strcmp(lv->relatedRsc, rscItem->key) == 0){
								rscVal += cTime - (unsigned long long)lv->timeStamp;
							}
							
						}
						
					}
					pthread_rwlock_unlock(thdLockMap->rwLock);
					
					long long waitVal = rscItem->timeStamp == 0 ? 0 : cTime - rscItem->timeStamp;
					long long rscWaitVal = rscItem->rscMtc.waitVal + waitVal;
					fprintf(fp, "%s: part time: %lluns part wait: %lluns slow down: %f client value: %lld old client valule: %lld client interval slow down: %f\n", rscItem->key, rscVal, rscWaitVal, attributeSlowdown(v, rscItem->key), (long long)rscItem->clientVal, (long long)rscItem->oldClientVal, attributeClientIntervalSlowdown(v, rscItem->key));
					rscItem->oldClientVal = rscItem->clientVal + waitVal;
					*/
				}
				else if (v->rscMap->mapArray[i].type == THREAD){
					/*
					hashMapStrKeyItem *rscItem = &v->rscMap->mapArray[i];
					long long tid = v->tid;
					unsigned long long rscVal = rscItem->rscMtc.usageVal;
					// find thread occupy time
					unsigned long long cTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;

					for(unsigned int j = 0; j < v->occupyMap->arraySize; ++j){
						hashMapItem *lockItem = &v->occupyMap->mapArray[j];
						lockValue *lv;
						
						if (lockItem->inUse){
							lv = (lockValue *) lockItem->value;
							if (lv && lv->id == tid && strcmp(lv->relatedRsc, rscItem->key) == 0){
								rscVal += cTime - (unsigned long long)lv->timeStamp;
							}
						}
					}

					long long waitVal = rscItem->timeStamp == 0 ? 0 : cTime - rscItem->timeStamp;
					long long rscWaitVal = rscItem->rscMtc.waitVal + waitVal;
					fprintf(fp, "%s: part time: %lluns part wait: %lluns slow down: %f client value: %lld old client valule: %lld client interval slow down: %f\n", rscItem->key, rscVal, rscWaitVal, attributeSlowdown(v, rscItem->key), (long long)rscItem->clientVal, (long long)rscItem->oldClientVal, attributeClientIntervalSlowdown(v, rscItem->key));
					rscItem->oldClientVal = rscItem->clientVal + waitVal;
					*/
				}
				else if (v->rscMap->mapArray[i].type == UNKNOWNTYPE){
					;
				}
			}
			
		}
	}
}

void *initCancellableManagerNoSharedMemory(void *arg)			//initCancellableManager
{
    char *statPath;
	static int time = 0;
	int pid = *(int *)arg;

	// for cancellable function use
	//currPid = pid;
	FILE *fp;

    statPath = getenv("AUTOCANCEL_OUTPUT");
    if(!statPath)
      statPath = STAE_PATH;

    threadMap = createHashMap(60);
    tidChangeMap = createHashMap(60);
	thdLockMap = createHashMap(180);

	threadMap->rwLock = &mapRWLock;
	tidChangeMap->rwLock = &tidChangeMapRWLock;
	thdLockMap->rwLock = &thdLockMapRWLock;
	//fp = fopen(statPath,"w");
	//fprintf(fp, "process id: %d\n\n", pid);
	// begin to monitor performance schema
	startAutoCancel = 1;

	while (!abortMonitor){
		fp = fopen(statPath,"a");
		if (fp == NULL){
			int errNum = 0;
			errNum = errno;
			printf("open fail errno = %d reason = %s \n", errNum, strerror(errNum));
			return arg;
		}
		//fprintf(fp, "\n-----------------------------------\ntime: %d\n-----------------------------------\n", time); //debug
		pthread_rwlock_rdlock(threadMap->rwLock);
		monitorThreads(pid, fp);
		pthread_rwlock_unlock(threadMap->rwLock);

		int monitorInterval = monitorThdInterval;
		fclose(fp);
		sleep(monitorInterval);
		time += monitorInterval;
		currentMonitorThdInterval = monitorInterval;
	}
	
	fp = fopen(statPath,"a");
	fclose(fp);
	
	return arg;
}

void *initCancellableManager(void *arg)
{
	char *statPath;
	FILE *fp;
	entry *e;
	cancellable *c;
	static int time = 0;

	statPath = getenv("AUTOCANCEL_OUTPUT");
    if(!statPath)
      statPath=STAE_PATH;
    //threadMap = createHashMap(10);
    //tidChangeMap = createHashMap(20);
	void *mapArray = (void *)m + sizeof(shmht);
	//fp = fopen(statPath,"w");
	while (!abortMonitor){
		//fp = fopen(statPath,"a");
		/*
		if (fp == NULL){
			int errNum = 0;
			errNum = errno;
			printf("open fail errno = %d reason = %s \n", errNum, strerror(errNum));
			return arg;
		}
		*/
		pthread_rwlock_rdlock(ctMapRWLock);
		//fprintf(fp, "\n-----------------------------------\ntime: %d\n-----------------------------------\n", time);
		for (unsigned int i = 0; i < m->size; ++i){
			e = (entry *)(mapArray + i * m->pairSize);
			if (e->isUse){
				//fprintf(fp, "key: %s\n", e->key);
				c = (cancellable *)((void *)e + sizeof(entry));
				//fprintf(fp, "c: %p, %d\n", c, c->tid);
				//fputs("-----------------------------\n",fp);
				if (c->isCancellable){
					shmht *shmhtRscMap = (shmht *)((void *) c + sizeof(cancellable));
					shmhtReadStat(c->tid, c);
                	shmhtMonitorTask(c, shmhtRscMap, fp);
				}
				//fputs("-----------------------------\n\n",fp);
			}
		}
		pthread_rwlock_unlock(ctMapRWLock);
		//fclose(fp);
		int monitorInterval = monitorThdInterval;
		sleep(monitorInterval);
		time += monitorInterval;
		currentMonitorThdInterval = monitorInterval;
	}
	//fclose(fp);
	return arg;
}

// Api for Picking Policy

long long getRscUsage(cancellable *c, char *rscKey)
{
	int pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, rscKey);
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    long long currentTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;
	if (pos == -1){
		return 0;
	}

    hashMapStrKeyItem *rscItem = &c->rscMap->mapArray[pos];
    if (rscItem->type == CPUCYCLE){
        long long rscVal = rscItem->rscMtc.usageVal - rscItem->lastTaskCpuTime;
        double second = (double) rscVal / 100;

        if (second == 0){
            return 0;
        }

        return rscVal;
    }
	else if (rscItem->type == IOWAIT){
		long long rscVal = rscItem->rscMtc.usageVal - rscItem->lastTaskIoBytes;

		return rscVal;
	}
    else if (rscItem->type == MEMUSAGE){
        long long rscVal = rscItem->rscMtc.usageVal;
        long long rscWaitVal = rscItem->rscMtc.waitVal;
        long long actual = rscVal - rscWaitVal;

		if (actual < 0){
			return 0;
		}

        return actual;
    }
	else if (rscItem->type == THREAD){
		struct timespec tp;
        long long tid;
        unsigned long long cTime, rscVal;
        hashMapItem *lockItem;
        lockValue *lv;

        tid = c->tid;
        rscVal = rscItem->rscMtc.usageVal;

        /* find lock occupy time */
        clock_gettime(CLOCK_REALTIME, &tp);
        cTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;
        
        for(unsigned int j = 0; j < c->occupyMap->arraySize; ++j){
            lockItem = &c->occupyMap->mapArray[j];
            if (lockItem->inUse){
                lv = (lockValue *) lockItem->value;
                if (lv && lv->id == tid && strcmp(lv->relatedRsc, rscItem->key) == 0){
                    rscVal += cTime - (unsigned long long)lv->timeStamp;
                }
                
            }
        }

        return rscVal;
	}
    else if (rscItem->type == QUEUEMODE){
        struct timespec tp;
        long long tid;
        unsigned long long cTime, rscVal, waitVal, rscWaitVal;
        hashMapItem *lockItem;
        lockValue *lv;

        tid = c->tid;
        rscVal = rscItem->rscMtc.usageVal;

        /* find lock occupy time */
        clock_gettime(CLOCK_REALTIME, &tp);
        cTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;
        
		pthread_rwlock_rdlock(thdLockMap->rwLock);
        for(unsigned int j = 0; j < thdLockMap->arraySize; ++j){
            lockItem = &thdLockMap->mapArray[j];
            if (lockItem->inUse){
                lv = (lockValue *) lockItem->value;
                if (lv && lv->id == tid && strcmp(lv->relatedRsc, rscItem->key) == 0){
                    rscVal += cTime - (unsigned long long)lv->timeStamp;
                }
                
            }
        }
		pthread_rwlock_unlock(thdLockMap->rwLock);

        /* find lock wait time */
        waitVal = rscItem->timeStamp == 0 ? 0 : cTime - rscItem->timeStamp;
		rscWaitVal = rscItem->rscMtc.waitVal + waitVal;

		//TODO: we still need to determine the usage of lock due to many cases
		//1. e.g. table lock, we can only monitor this, but itself will not be blocked, so the waiting time may just equal to user time if only one lock
		// actually we may need to assume there is no usage
		//2. However, for cases like mdl lock, although flush is blocked, but its global lock block others, so both its waiting time and usage time may need to be high.
		if (c->finishedRows == 0){
			return 0;
		}

        return rscVal - rscWaitVal;
    }

    return 0;
}

long long getFutureRscUsage(cancellable *c, char *rscKey)
{
	long long usage = getRscUsage(c, rscKey);
	double lp = getLeftProgress(c);
	lp = lp/(1-lp);
	usage = usage * lp;

	return usage;
}

double attributeSlowdown(cancellable *c, char *rscKey)
{
    int pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, rscKey);
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    long long currentTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;
	if (pos == -1){
		return 0;
	}

    hashMapStrKeyItem *rscItem = &c->rscMap->mapArray[pos];
    if (rscItem->type == CPUCYCLE){
        long long rscVal = rscItem->rscMtc.usageVal - rscItem->lastTaskCpuTime;

		// two cases for it to be zero: 
		// 1. the time span of task is to small to collect cycle used.
		// 2. the task don't need cycle, for example sleep, then it's nothing related to slowdown if it doesn't even need cpu cycle.
		if (rscVal == 0){
			return 0;
		}

        double second = (double) rscVal / 100;
        double realTimeConsump = (double) (currentTime - c->startTime) / 1000000000L;
		double actual = realTimeConsump - second;

		// incorrect or inaccurate tracing
		if (actual < 0){
			return 0;
		}

		double slowdown = (realTimeConsump - second) / realTimeConsump;
		if (slowdown > 1){
			return 1;
		}

		return (realTimeConsump - second) / realTimeConsump;
    }
	else if (rscItem->type == IOWAIT){
		long long rscWaitVal = (rscItem->rscMtc.waitVal - rscItem->lastTaskIoDelay) * 10000000L;
		long long realTime = currentTime - c->startTime;

		return (double) rscWaitVal / realTime;
	}
    else if (rscItem->type == MEMUSAGE){
        long long rscWaitVal = rscItem->rscMtc.waitVal;
		long long realTime = currentTime - c->startTime;

        return (double)rscWaitVal / realTime;
    }
	else if (rscItem->type == THREAD){
		long long realTime = currentTime - c->startTime;
		long long waitVal = rscItem->timeStamp == 0 ? 0 : currentTime - rscItem->timeStamp;

		return (double) waitVal / realTime;
	}
    else if (rscItem->type == QUEUEMODE){
		/* old slowdown calculation algorithm
        struct timespec tp;
        long long tid;
        unsigned long long cTime, rscVal, waitVal, rscWaitVal;
        hashMapItem *lockItem;
        lockValue *lv;

        tid = c->tid;
        rscVal = rscItem->rscMtc.usageVal;

        // find lock occupy time //
        clock_gettime(CLOCK_REALTIME, &tp);
        cTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;
        
		pthread_rwlock_rdlock(thdLockMap->rwLock);
        for(unsigned int j = 0; j < thdLockMap->arraySize; ++j){
            lockItem = &thdLockMap->mapArray[j];
            if (lockItem->inUse){
                lv = (lockValue *) lockItem->value;
                if (lv && lv->id == tid){
                    rscVal += cTime - (unsigned long long)lv->timeStamp;
                }
                
            }
        }
		pthread_rwlock_unlock(thdLockMap->rwLock);

        // find lock wait time //
        waitVal = rscItem->timeStamp == 0 ? 0 : cTime - rscItem->timeStamp;
		rscWaitVal = rscItem->rscMtc.waitVal + waitVal;

        if (rscVal == 0){
            return rscWaitVal;
        }

        return (double)(rscVal + rscWaitVal) / rscVal;
		*/
		long long realTime = currentTime - c->startTime;
		long long waitVal = rscItem->timeStamp == 0 ? 0 : currentTime - rscItem->timeStamp;

		return (double) waitVal / realTime;
    }

    return 0;
}

double attributeClientIntervalSlowdown(cancellable *c, char *rscKey)
{
	int pos = _getStrKeyItem(c->rscMap->mapArray, c->rscMap->arraySize, rscKey);
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    long long currentTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;
	if (pos == -1){
		return 0;
	}

    hashMapStrKeyItem *rscItem = &c->rscMap->mapArray[pos];
	if (rscItem->type == CPUCYCLE){
		double clientVal = (rscItem->clientVal - rscItem->oldClientVal);
		double timeInterval = (currentMonitorThdInterval * 100);
		if (timeInterval - clientVal < 0){
			return 0;
		}
		return (double) (timeInterval - clientVal) / timeInterval;
	}
	else if (rscItem->type == IOWAIT){
		double clientVal = (rscItem->clientVal - rscItem->oldClientVal);
		double timeInterval = (currentMonitorThdInterval * 100);

		return clientVal / timeInterval;
	}
	else if (rscItem->type == MEMUSAGE){
		long long cVal = (long long) rscItem->clientVal;
		long long oCVal = (long long) rscItem->oldClientVal;
		return (double)(cVal - oCVal) / (currentMonitorThdInterval * 1000000000L);
	}
	else if (rscItem->type == THREAD){
		double waitVal = rscItem->timeStamp == 0 ? 0 : currentTime - rscItem->timeStamp;
		long long cVal = (long long) rscItem->clientVal;
		long long oCVal = (long long) rscItem->oldClientVal;
		return (double)(cVal + waitVal - oCVal) / (currentMonitorThdInterval * 1000000000L);
	}
	else if (rscItem->type == QUEUEMODE){
		double waitVal = rscItem->timeStamp == 0 ? 0 : currentTime - rscItem->timeStamp;
		long long cVal = (long long) rscItem->clientVal;
		long long oCVal = (long long) rscItem->oldClientVal;
		return (double)(cVal + waitVal - oCVal) / (currentMonitorThdInterval * 1000000000L);
	}
    
    return (double)(rscItem->clientVal - rscItem->oldClientVal) / currentMonitorThdInterval;
}

double getContentionLevel(char *rscKey)
{
	int count = 0;
	double slowdown, totSlowdown = 0;

	if (!threadMap || !tidChangeMap){
		return 0;
	}

	for (unsigned int i = 0; i < threadMap->arraySize; ++i){
		if (!threadMap->mapArray[i].inUse){
			continue;
		}

		cancellable *c = (cancellable *) threadMap->mapArray[i].value;
		if (!c->isCancellable){
			continue;
		}
		slowdown = attributeSlowdown(c, rscKey);
		
		totSlowdown += slowdown;
		count += 1;
	}

	if (count == 0){
		assert(totSlowdown == 0);
		return 0;
	}

	return totSlowdown / count;
}

// shmht api pick policy
long long shmhtGetRscUsage(cancellable *c, char *rscKey)
{
	volatile rscValue *valPtr;
	volatile shmht *rscMap;
	struct timespec tp;
	double second;
	entry *lockEntry;
    volatile lockValue *lv;
	//void *globalLockMapArray;
	volatile void *shmhtLockMapArray;
	char keyStr[MAX_KEY_SIZE];
	long long rscVal, rscWaitVal, actual, currentTime;
    clock_gettime(CLOCK_REALTIME, &tp);
	currentTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;
	/* debug
	FILE *fp = fopen("/data/zeyin/debug", "a");
	fprintf(fp, "shmhtgetrscusage: %d\n", c->tid);
	fflush(fp);
	*/

	rscMap = (shmht *)((void *) c + sizeof(cancellable));
	valPtr = (rscValue *) shmhtGet(rscMap, rscKey);
	if (valPtr == NULL){
		/*
		fprintf(fp, "tid: %d\n", c->tid);
		fprintf(fp, "lack rsc: %s\n", rscKey);
		fflush(fp);
		fclose(fp);
		*/
		return 0;
	}

	volatile enum rscType currType = valPtr->type;
	/*
	fprintf(fp, "valptr: %p rsc type: %d\n", valPtr, currType);	//debug
	fflush(fp);	// debug
	*/
	switch (currType)
	{
		case CPUCYCLE:
			if (clientMonitorMode){
				rscVal = valPtr->value - valPtr->oldValue;
				second = (double) rscVal / 100;
			}
			else{
				rscVal = valPtr->value - c->lastTaskCpuTime;
				second = (double) rscVal / 100;
			}

			if (second == 0){
				return 0;
			}

			if (rscVal < 0){
				return 0;
			} 

			return rscVal;
			break;

		case IOWAIT:
			if (clientMonitorMode){
				rscVal = valPtr->value - valPtr->oldValue;
			}
			else{
				rscVal = valPtr->value - c->lastTaskIoBytes;
			}
			return rscVal;
			break;

		case MEMUSAGE:
			rscVal = valPtr->value;
			rscWaitVal = valPtr->waitValue;
			actual = rscVal - rscWaitVal;

			if (actual < 0){
				return 0;
			}

			return actual;
			break;

		case QUEUEMODE:
			/*original queue mode usage get*/
			/*
			globalLockMapArray = (void *) globalLockMap + sizeof(shmht);
            rscWaitVal = valPtr->timeStamp != 0 ? valPtr->waitValue + currentTime - valPtr->timeStamp : valPtr->waitValue;
            rscVal = valPtr->value;
			*/
			/*
			FILE *fp = fopen("/data/zeyin/debug", "a");	// debug
			fprintf(fp, "tid: %d\n", c->tid);				// debug
			*/

			/*
            pthread_rwlock_wrlock(globalLockMapRWLock);
            for (unsigned int l = 0; l < globalLockMap->size; ++l){
                lockEntry = (entry *)(globalLockMapArray + l * globalLockMap->pairSize);
                if (!lockEntry->isUse){
                    continue;
                }

                lv = (lockValue *) ((void *)lockEntry + sizeof(entry));
				//fprintf(fp, "lv key: %s lv id: %d lv rsc: %s\n", lockEntry->key, lv->id, lv->relatedRsc);	//debug
                if (lv && lv->id == c->tid && strcmp(lv->relatedRsc, rscKey) == 0){
                    rscVal += currentTime - lv->timeStamp;
                }
            }
            pthread_rwlock_unlock(globalLockMapRWLock);
			//fclose(fp);	//debug
			// TODO: judge usage based on finished rows since a task may get lock but meanwhile get stuck

			if (rscVal < rscWaitVal){
				return 0;
			}

			return rscVal - rscWaitVal;
			break;
			*/
			sprintf(keyStr, "%d", c->tid);
			rscWaitVal = valPtr->timeStamp != 0 ? valPtr->waitValue + currentTime - valPtr->timeStamp : valPtr->waitValue;
            rscVal = valPtr->value;

			volatile shmht *shmhtLockMap = (shmht *) shmhtGet(globalLockMap, keyStr);
			if (shmhtLockMap == NULL){
				fprintf(stderr, "error: can't find queuemode rsc at global lock map %d\n", c->tid);
				return 0;
			}
			shmhtLockMapArray = (void *) shmhtLockMap + sizeof(shmht);
			pthread_rwlock_wrlock(globalLockMapRWLock);
            for (unsigned int l = 0; l < shmhtLockMap->size; ++l){
                lockEntry = (entry *)(shmhtLockMapArray + l * shmhtLockMap->pairSize);
                if (!lockEntry->isUse){
                    continue;
                }

                lv = (lockValue *) ((void *)lockEntry + sizeof(entry));
				//fprintf(fp, "lv key: %s lv id: %d lv rsc: %s\n", lockEntry->key, lv->id, lv->relatedRsc);	//debug
                if (lv && lv->id == c->tid && strcmp(lv->relatedRsc, rscKey) == 0){
                    rscVal += currentTime - lv->timeStamp;
                }
            }
            pthread_rwlock_unlock(globalLockMapRWLock);

			if (rscVal < rscWaitVal){
				return 0;
			}

			return rscVal - rscWaitVal;
			break;

		default:
			break;
	}

	//fclose(fp);	// debug
	return 0;
}

long long shmhtGetFutureRscUsage(cancellable *c, char *rscKey)
{
	long long usage = shmhtGetRscUsage(c, rscKey);
	double lp = getLeftProgress(c);
	lp = lp / (1-lp);
	usage = usage * lp;

	return usage;
}

double shmhtAttributeSlowdown(cancellable *c, char *rscKey)
{
	rscValue *valPtr;
	shmht *rscMap;
	struct timespec tp;
	double second;
	long long rscVal, rscWaitVal, actual, currentTime, actualTime;
    clock_gettime(CLOCK_REALTIME, &tp);
	currentTime  = tp.tv_sec * 1000000000L + tp.tv_nsec;

	rscMap = (shmht *)((void *) c + sizeof(cancellable));
	valPtr = (rscValue *) shmhtGet(rscMap, rscKey);

	if (valPtr == NULL){
		return 0;
	}

	switch (valPtr->type)
	{
		case CPUCYCLE:
			if (clientMonitorMode){
				rscVal = valPtr->value - valPtr->oldValue;
				second = (double) rscVal / 100;
				actualTime = currentMonitorThdInterval * 1000000000L;
			}
			else{
				rscVal = readThdCpuTime(c->tid, c->tid);
				rscVal = rscVal - c->lastTaskCpuTime;
				second = (double) rscVal / 100;
				actualTime = currentTime - c->startTime;
			}

			if (second == 0 || actualTime == 0 || rscVal < 0){
				return 0;
			}

			double result = (double) (actualTime - rscVal * 10000000L);
			if (result < 0){
				return 0;
			}

			result = result / actualTime;
			return result;
			break;

		case IOWAIT:
			if (clientMonitorMode){
				rscWaitVal = (valPtr->waitValue - valPtr->oldWaitValue) * 10000000L;
				actual = currentMonitorThdInterval * 1000000000L;
			}
			else{
				rscWaitVal = (valPtr->waitValue - c->lastTaskIoDelay) * 10000000L;
				actual = currentTime - c->startTime;
			}

			return (double) rscWaitVal / actual;
			break;

		case MEMUSAGE:
			rscWaitVal = valPtr->waitValue;
			actual = currentTime - c->startTime;

			return (double) rscWaitVal / actual;
			break;

		case QUEUEMODE:
			actual = currentTime - c->startTime;
			rscWaitVal = valPtr->timeStamp == 0 ? 0 : currentTime - valPtr->timeStamp + valPtr->waitValue;

			return (double) rscWaitVal / actual;
			break;

		default:
			break;			
	}

	return 0;
}

double shmhtGetContentionLevel(char *rscKey)
{
	entry *e;
	cancellable *c;
	void *mapArray;
	int count = 0;
	double slowdown, totSlowdown = 0;

	if (m == NULL || globalLockMap == NULL){
		return 0;
	}

	mapArray = (void *) m + sizeof(shmht);
	for (unsigned int i = 0; i < m->size; ++i){
		e = (entry *)(mapArray + i * m->pairSize);
		if (!e->isUse){
			continue;
		}

		c = (cancellable *)((void *)e + sizeof(entry));
		if (!c->isCancellable){
			continue;
		}

		slowdown = shmhtAttributeSlowdown(c, rscKey);
		totSlowdown += slowdown;
		count += 1;
	}

	if (count == 0){
		assert(totSlowdown == 0);
		return 0;
	}

	return totSlowdown / count;
}

// debug
int isBackground(int appTid)
{
	cancellable *c = (cancellable *) get(threadMap, (unsigned long) appTid);
	if (!c){
		return 0;
	}
	return c->isBackground;
}

void setBackgroundNeedKilled(int appTid)
{
	cancellable *c = (cancellable *) get(threadMap, (unsigned long) appTid);
	if (!c){
		return ;
	}
	c->needKilled = 1;
}

int needKillBackground(void)
{
	int tid = getCurrTid();
	if (threadMap == NULL || tidChangeMap == NULL){
		return 0;
	}

	void *appTid;
	if ((appTid = get(tidChangeMap, tid))){
		cancellable *c = (cancellable *) get(threadMap, (unsigned long) appTid);
		if (c->needKilled){
			c->needKilled = 0;
			return 1;
		}
		else{
			return 0;
		}
	}

	return 0;
}

//==========================================================

void setClientMonitorMode(void)
{
	clientMonitorMode = 1;
}

void setMonitorProgress(void (*func)(unsigned long, FILE *))
{
	monitorProgress = func;
}

void setCancelAction(void (*func)(unsigned long) )
{
	activateCancel = func;
}

void deployCancellationPolicy(int (*func)(hashMap))
{
	cancellableSchedule = func;
}

// potential function for future use
int CPUsIdle(void)
{
    FILE *fp;
    char line[8192];
    unsigned long long cpu_user;
    unsigned long long cpu_idle;

	if ((fp = fopen(PROC_STAT, "r")) == NULL) {
        exit(2);
	}

	while (fgets(line, sizeof(line), fp) != NULL) {

		if (!strncmp(line, "cpu ", 4)) {

			sscanf(line + 5, "%llu %*llu %*llu %llu",
                   &cpu_user,
			       &cpu_idle
			       );

            if (cpu_user < cpu_idle){
                return 1;
            }
		}

		else if (!strncmp(line, "cpu", 3)) {
			sscanf(line + 3, "%*d %llu %*llu %*llu %llu",
			       &cpu_user,
			       &cpu_idle);

            if (cpu_user < cpu_idle){
                return 1;
            }
		}
	}

	fclose(fp);
	return 0;
}
