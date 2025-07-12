#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <setjmp.h>
#include <pthread.h>

#include "hashMap.h"
#include "cancellable.h"
#include "syscall.h"

#define easyCreateCancel(isC) createCancellable(isC, 0, 0, 0, NULL)

jmp_buf getPy;
static const int THREAD_NUMBER = 2;
hashMap *map;

unsigned int getThreadToKill(hashMap *map)
{
    for (unsigned int i = 0; i < map->arraySize; ++i){
        if (map->mapArray[i].inUse){
            return map->mapArray[i].key;
        }
    }
}

void killInConnection(void *arg){
    printf("\n");
}

void handle(int signo)
{
    printf("get python signal\n");
    print(map);
    siglongjmp(getPy, 1);
}

void *handleConnection(void *arg)
{
    hashMap *map = (hashMap *)arg;
    cancellable *c = easyCreateCancel(0);
    insert(map, syscall(SYS_gettid), c);
    MysqlSelect(syscall(SYS_gettid), 100, c);
    removeEntry(map, syscall(SYS_gettid), 1);
}

int main(void)
{
    pthread_t threads[THREAD_NUMBER];
    
    map = createHashMap(32);
    printf("test begin\n");

    if (SIG_ERR == signal(SIGUSR1, handle))
        printf("failed to register siguser1 handler\n");

    for (int i = 0; i < THREAD_NUMBER; i++) {
        int ret = pthread_create(&threads[i], NULL, handleConnection, (void *)map);
        if (ret != 0)
            printf("create thread %d fail\n", i);
    }

    if (sigsetjmp(getPy, 1)){
        printf("back to main\n");
        
        // in future, the command should be like "kill query id"
        unsigned int threadIDtoKill = getThreadToKill(map);
        printf("thread to kill: %d\n", threadIDtoKill);
        system("sudo ./cancel.sh");
    }

    while(1);
    
    for (int i = 0; i < THREAD_NUMBER; i++)
    {
        pthread_join(threads[i], NULL);
    }
    
    return 0;
}