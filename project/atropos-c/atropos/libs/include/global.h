#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <stdio.h>
#include <string.h>
#include <pthread.h>

extern int currPid;

typedef struct lockValue
{
    int id;
    long long timeStamp;
    char relatedRsc[50];
} lockValue;

unsigned long long readThdCpuTime(unsigned int pid, pthread_t tid);
unsigned long long readThreadIOStats(unsigned int pid, pthread_t tid);
unsigned long long readThreadIOUsage(unsigned int pid, pthread_t tid);
void genProcStatsFileName(char *procFileName, int pid);
void genThreadStatsFileName(char *thdFileName, unsigned int pid, pthread_t tid);
void genProcIoFileName(char *procIoFileName, int pid);
void genThreadIoFileName(char *thdIoFileName, unsigned int pid, unsigned int tid);

#endif