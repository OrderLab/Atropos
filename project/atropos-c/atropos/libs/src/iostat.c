#include"global.h"
#include"iostat.h"
#include<stdlib.h>
#include<stdio.h>
#include<string.h>

void genProcStatsFileName(char *procFileName, int pid)
{
	char pidStr[10];

	snprintf(pidStr, 10, "%i", (int) pid);
	strcpy(procFileName, "/proc/");
	strcat(procFileName, pidStr);
	strcat(procFileName, "/stat");
}

void genThreadStatsFileName(char *thdFileName, unsigned int pid, pthread_t tid)
{
	char pidStr[10];
	char tidStr[10];

	snprintf(pidStr, 10, "%i", (int) pid);
	snprintf(tidStr, 10, "%u", tid);

	strcpy(thdFileName, "/proc/");
	strcat(thdFileName, pidStr);
	strcat(thdFileName, "/task/");
	strcat(thdFileName, tidStr);
	strcat(thdFileName, "/stat");
}

void genProcIoFileName(char *procIoFileName, int pid)
{
	char pidStr[10];

	snprintf(pidStr, 10, "%i", (int) pid);

	strcpy(procIoFileName, "/proc/");
	strcat(procIoFileName, pidStr);
	strcat(procIoFileName, "/io");
}

void genThreadIoFileName(char *thdIoFileName, unsigned int pid, unsigned int tid)
{
	char tidStr[10];
	char pidStr[10];

	snprintf(pidStr, 10, "%i", (int) pid);
	snprintf(tidStr, 10, "%u", tid);

	strcpy(thdIoFileName, "/proc/");
	strcat(thdIoFileName, pidStr);
	strcat(thdIoFileName, "/task/");
	strcat(thdIoFileName, tidStr);
	strcat(thdIoFileName, "/io");
}

unsigned long long readThdCpuTime(unsigned int pid, pthread_t tid)
{
	FILE *fp;
	char thdFileName[50], line[512];
	int i;
	unsigned long long uTime, sTime;

	genThreadStatsFileName(thdFileName, pid, tid);

	if ((fp = fopen(thdFileName, "r")) == NULL)
		return 0;

	// we extract user time, system time and aggregated block io delay from stat file
	while (fgets(line, sizeof(line), fp) != NULL) {
		i = sscanf(line, "%*d %*s %*c %*d %*d \
						  %*d %*d %*d %*u %*lu \
						  %*lu %*lu %*lu %lu %lu", \ 
			   &uTime, &sTime);
	}
	fclose(fp);

	return uTime + sTime;
}

unsigned long long readThreadIOStats(unsigned int pid, pthread_t tid)
{
	FILE *fp;
	char thdFileName[50], line[512];
	int i;
	unsigned long long ioDelay;

	// thread tsta file: /proc/[pid]/task/[tid]/stat
	genThreadStatsFileName(thdFileName, pid, tid);

	if ((fp = fopen(thdFileName, "r")) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp) != NULL) {
		i = sscanf(line, "%*d %*s %*c %*d %*d \
						  %*d %*d %*d %*u %*lu \
						  %*lu %*lu %*lu %*lu %*lu \
						  %*ld %*ld %*ld %*ld %*ld \
						  %*ld %*llu %*lu %*ld %*lu \
						  %*lu %*lu %*lu %*lu %*lu \
						  %*lu %*lu %*lu %*lu %*lu \
						  %*lu %*lu %*d %*d %*u \
						  %*u %llu", \
			   &ioDelay);
	}

	//printf("%llu\n", ioDelay);
	fclose(fp);
	return ioDelay;
}

unsigned long long readThreadIOUsage(unsigned int pid, pthread_t tid)
{
	FILE *fp;
	char thdFileName[50], line[512];
	int i;
	unsigned long long readBytes = 0, writeBytes = 0, result = 0;

	// thread tsta file: /proc/[pid]/task/[tid]/io
	genThreadIoFileName(thdFileName, pid, tid);

	if ((fp = fopen(thdFileName, "r")) == NULL)
		return 0;

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

	result = readBytes + writeBytes;
	fclose(fp);
	return result;
}
