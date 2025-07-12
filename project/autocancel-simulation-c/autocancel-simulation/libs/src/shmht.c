#include "shmht.h"
#include <execinfo.h>
#include <stdio.h>
#include <strings.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <unistd.h>
#include "cancellable.h"
#include "global.h"

int ctMapShmId = 0;
int globalLockMapShmId = 0;
int ctMapRWLockShmId = 0;
pthread_rwlock_t *ctMapRWLock = NULL;
int globalLockMapRWLockShmId = 0;
pthread_rwlock_t *globalLockMapRWLock = NULL;

void createGlobalLockMap(long long size) {
  int shmhtId;
  long long valSize = sizeof(shmht) + (sizeof(entry) + sizeof(lockValue)) * 10;
  // long long valSize = sizeof(lockValue);
  long long pairSize = sizeof(entry) + valSize;

  key_t key = ftok(SHM_LOCK_MAP_PATH, 0);
  shmhtId = shmget(key, sizeof(shmht) + size * pairSize,
                   0777 | IPC_CREAT);
  if (shmhtId == -1) {
    fprintf(stderr, "shmget for global lock map failed: size %lld\n",
            sizeof(shmht) + size * (pairSize));
    exit(EXIT_FAILURE);
  }

  globalLockMap = (shmht *)shmat(shmhtId, NULL, 0);
  globalLockMap->size = size;
  globalLockMap->valSize = valSize;
  globalLockMap->pairSize = pairSize;
  globalLockMap->shmhtId = shmhtId;
  globalLockMap->isGlobal = 0;
  globalLockMapShmId = shmhtId;

  /* original globalLockMap
  key_t key = ftok(SHM_LOCK_MAP_PATH, 0);
  shmhtId = shmget(key, sizeof(shmht) + GLOBAL_LOCK_MAP_SIZE * pairSize,
  0777|IPC_CREAT); if (shmhtId == -1){ fprintf(stderr, "shmget for global lock
  map failed: size %lld\n", sizeof(shmht) + GLOBAL_LOCK_MAP_SIZE * (pairSize));
      exit(EXIT_FAILURE);
  }

  globalLockMap = (shmht *) shmat(shmhtId, NULL, 0);
  globalLockMap->size = GLOBAL_LOCK_MAP_SIZE;
  globalLockMap->valSize = valSize;
  globalLockMap->pairSize = pairSize;
  globalLockMap->shmhtId = shmhtId;
  globalLockMap->isGlobal = 1;
  globalLockMapShmId = shmhtId;
  */
}

void createMapRWLock(void) {
  key_t key = ftok(SHM_CTMAP_RWLOCK_PATH, 0);
  ctMapRWLockShmId = shmget(key, sizeof(pthread_rwlock_t), 0777 | IPC_CREAT);
  if (ctMapRWLockShmId == -1) {
    fprintf(stderr, "shmget for ctmap rwlock failed: size %ld\n",
            sizeof(pthread_rwlock_t));
    exit(EXIT_FAILURE);
  }
  ctMapRWLock = (pthread_rwlock_t *)shmat(ctMapRWLockShmId, NULL, 0);

  key = ftok(SHM_GLOBLOCKMAP_RWLOCK_PATH, 0);
  globalLockMapRWLockShmId =
      shmget(key, sizeof(pthread_rwlock_t), 0777 | IPC_CREAT);
  if (globalLockMapRWLockShmId == -1) {
    fprintf(stderr, "shmget for global lock map rwlock failed: size %ld\n",
            sizeof(pthread_rwlock_t));
    exit(EXIT_FAILURE);
  }
  globalLockMapRWLock =
      (pthread_rwlock_t *)shmat(globalLockMapRWLockShmId, NULL, 0);
}

void initMapRWLock(void) {
  pthread_rwlockattr_t rwlock_attr;
  memset(&rwlock_attr, 0, sizeof(pthread_rwlockattr_t));
  pthread_rwlockattr_init(&rwlock_attr);
  pthread_rwlockattr_setpshared(&rwlock_attr, PTHREAD_PROCESS_SHARED);
  pthread_rwlock_init(ctMapRWLock, &rwlock_attr);
  pthread_rwlock_init(globalLockMapRWLock, &rwlock_attr);
}

shmht *createCancellableTaskShmht(long long size, long long rscNum) {
  int shmhtId;
  long long rscMapArraySize =
      sizeof(shmht) + (sizeof(entry) + sizeof(rscValue)) * rscNum;
  long long valSize = sizeof(cancellable) + rscMapArraySize;
  long long pairSize = sizeof(entry) + valSize;
  shmht *ctMap;

  key_t key = ftok(SHM_PATH, 0);
  shmhtId = shmget(key, sizeof(shmht) + size * (pairSize), 0777 | IPC_CREAT);
  if (shmhtId == -1) {
    fprintf(stderr, "shmget failed: size %lld\n",
            sizeof(shmht) + size * (pairSize));
    exit(EXIT_FAILURE);
  } else {
    fprintf(stderr, "ctmap size %lld\n", sizeof(shmht) + size * (pairSize));
  }
  void *mem = shmat(shmhtId, NULL, 0);

  ctMap = (shmht *)mem;
  ctMap->size = size;
  ctMap->valSize = valSize;
  ctMap->pairSize = pairSize;
  ctMap->shmhtId = shmhtId;
  ctMap->isGlobal = 1;
  ctMapShmId = shmhtId;

  createMapRWLock();
  createGlobalLockMap(size);

  return ctMap;
}

void initShmht(shmht *map) {
  unsigned int i;
  void *mapArray = (void *)map + sizeof(shmht);
  for (i = 0; i < map->size; ++i) {
    entry *e = (entry *)(mapArray + i * map->pairSize);
    e->isUse = 0;
  }
}

static void initShmhtRscMap(long long rscNum, shmht *shmhtRscMap) {
  void *rscMapArray;
  rscValue *rsc;

  shmhtRscMap->valSize = sizeof(rscValue);
  shmhtRscMap->pairSize = sizeof(entry) + shmhtRscMap->valSize;
  shmhtRscMap->size = rscNum;
  shmhtRscMap->isGlobal = 0;
  initShmht(shmhtRscMap);

  // init rscValue
  rscMapArray = (void *)shmhtRscMap + sizeof(shmht);
  for (unsigned int i = 0; i < shmhtRscMap->size; ++i) {
    rsc = (rscValue *)(rscMapArray + i * shmhtRscMap->pairSize + sizeof(entry));
    rsc->isTaskLevel = 0;
    rsc->oldValue = 0;
    rsc->timeStamp = 0;
    rsc->value = 0;
    rsc->waitValue = 0;
    rsc->oldWaitValue = 0;
    // rsc->rscHQ.head = 0;
    // rsc->rscHQ.tail = 0;
    // rsc->rscHQ.size = 0;
  }
}

void initCancellableTaskShmht(long long rscNum, shmht *ctMap) {
  unsigned int i;
  void *mapArray = (void *)ctMap + sizeof(shmht);
  initShmht(ctMap);
  // by the way init global lock map
  initShmht(globalLockMap);
  initMapRWLock();

  for (i = 0; i < ctMap->size; ++i) {
    shmht *shmhtRscMap;

    // organize the structure: val{cancellable, shmht, rscMapArray}
    cancellable *c =
        (cancellable *)(mapArray + i * ctMap->pairSize + sizeof(entry));
    c->isCancellable = 0;
    c->maxRscNum = 0;
    c->finishedRequests = 0;
    shmhtRscMap = (shmht *)((void *)c + sizeof(cancellable));

    initShmhtRscMap(rscNum, shmhtRscMap);
  }

  mapArray = (void *)globalLockMap + sizeof(shmht);
  for (i = 0; i < globalLockMap->size; ++i) {
    shmht *shmhtLockMap;

    shmhtLockMap = (shmht *)(mapArray + i * globalLockMap->pairSize + sizeof(entry));
    shmhtLockMap->valSize = sizeof(lockValue);
    shmhtLockMap->pairSize = sizeof(entry) + shmhtLockMap->valSize;
    shmhtLockMap->size = 10;
    shmhtLockMap->isGlobal = 0;
    initShmht(shmhtLockMap);
  }
}

void printCallers(void) {
  int layers = 0, i = 0;
  char **symbols = NULL;
  int MAX_FRAMES = 50;

  void *frames[MAX_FRAMES];
  memset(frames, 0, sizeof(frames));
  layers = backtrace(frames, MAX_FRAMES);
  for (i = 0; i < layers; i++) {
    fprintf(stderr, "Layer %d: %p\n", i, frames[i]);
  }
  fprintf(stderr, "------------------\n");

  symbols = backtrace_symbols(frames, layers);
  if (symbols) {
    for (i = 0; i < layers; i++) {
      fprintf(stderr, "SYMBOL layer %d: %s\n", i, symbols[i]);
    }
    free(symbols);
  } else {
    fprintf(stderr, "Failed to parse function names\n");
  }
}

unsigned int shmhtHash(char *s, unsigned int size) {
  if (size == 0) {
    printCallers();
    abort();
  }
  unsigned int h = 0;
  for (; *s; s++) {
    h = *s + h * 31;
  }
  return h % size;
}

int shmhtFind(shmht *map, char *key) {
  unsigned int start, i, j;
  void *mapArray = (void *)map + sizeof(shmht);

  start = shmhtHash(key, map->size);
  i = start;
  j = 0;
  while (j < map->size) {
    entry *e = (entry *)(mapArray + i * map->pairSize);
    if (e->isUse && !strcmp(e->key, key)) {
      return i;
    }

    i = (i + 1) % map->size;
    j++;
  }

  return -1;
}

int _shmhtFindInsertPos(shmht *map, char *key) {
  unsigned int start, i, j;
  void *mapArray = (void *)map + sizeof(shmht);

  start = shmhtHash(key, map->size);
  i = start;
  j = 0;
  while (j < map->size) {
    entry *e = (entry *)(mapArray + i * map->pairSize);
    if (e->isUse == 0) {
      return i;
    }

    i = (i + 1) % map->size;
    j++;
  }

  return -1;
}

void *shmhtGet(shmht *map, char *key) {
  unsigned int start, i, j;
  void *mapArray = (void *)map + sizeof(shmht);
  int pid = currPid;  // debug

  if (map->size == 0) {
    fprintf(stderr, "map size equals to 0: key: %s pid: %d\n", key, pid);
    return NULL;
  }

  start = shmhtHash(key, map->size);
  i = start;
  j = 0;
  while (j < map->size) {
    volatile entry *e = (entry *)(mapArray + i * map->pairSize);
    if (e->isUse && !strcmp(e->key, key)) {
      return (void *)e + sizeof(entry);
    }

    i = (i + 1) % map->size;
    j++;
  }

  return NULL;
}

void shmhtRemove(shmht *map, char *key) {
  int pos;
  entry *e;
  void *val;
  void *mapArray = (void *)map + sizeof(shmht);

  // clear entry
  pos = shmhtFind(map, key);
  if (pos == -1) {
    printCallers();
    abort();
  }
  e = (entry *)(mapArray + pos * map->pairSize);
  e->isUse = 0;
  bzero(e->key, MAX_KEY_SIZE);

  // clear val
  val = (void *)e + sizeof(entry);
  bzero(val, map->valSize);
}

void *shmhtInsert(shmht *map, char *key, long long value) {
  int pos;
  entry *e;
  rscValue *valuePtr;
  void *mapArray = (void *)map + sizeof(shmht);

  pos = _shmhtFindInsertPos(map, key);
  if (pos == -1) {
    printCallers();
    fprintf(stderr, "insert %s lack position\n", key);
    abort();
  }
  e = (entry *)(mapArray + pos * map->pairSize);
  valuePtr = (rscValue *)((void *)e + sizeof(entry));

  e->isUse = 1;
  strcpy(e->key, key);

  valuePtr->value = value;
  return (void *)valuePtr;
}

void shmhtPrint(shmht *map) {
  void *mapArray = (void *)map + sizeof(shmht);

  for (unsigned int j = 0; j < map->size; ++j) {
    entry *e = (entry *)(mapArray + j * map->pairSize);
    rscValue *rsc = (rscValue *)((void *)e + sizeof(entry));
    if (e->isUse) {
      printf("key: %s ", e->key);
      printf("value: %lld\n", rsc->value);
    }
  }
}

void detachShmhtCancellable(void) {
  if (shmdt(globalLockMap) == -1) {
    fprintf(stderr, "global lock map shmdt failed\n");
    exit(EXIT_FAILURE);
  }

  if (shmdt(m) == -1) {
    fprintf(stderr, "task map detach failed");
    exit(EXIT_FAILURE);
  }

  if (shmdt(ctMapRWLock) == -1) {
    fprintf(stderr, "task map rwlock detach failed");
    exit(EXIT_FAILURE);
  }

  if (shmdt(globalLockMapRWLock) == -1) {
    fprintf(stderr, "global lock map rwlock detach failed");
    exit(EXIT_FAILURE);
  }

  if (shmctl(globalLockMapShmId, IPC_RMID, 0) == -1) {
    fprintf(stderr, "global lock map delete failed\n");
    exit(EXIT_FAILURE);
  }

  if (shmctl(ctMapShmId, IPC_RMID, 0) == -1) {
    fprintf(stderr, "task map delete failed\n");
    exit(EXIT_FAILURE);
  }

  if (shmctl(ctMapRWLockShmId, IPC_RMID, 0) == -1) {
    fprintf(stderr, "task map rwlock delete failed\n");
    exit(EXIT_FAILURE);
  }

  if (shmctl(globalLockMapRWLockShmId, IPC_RMID, 0) == -1) {
    fprintf(stderr, "global lock map rwlock delete failed\n");
    exit(EXIT_FAILURE);
  }
}