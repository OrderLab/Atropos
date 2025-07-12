#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#define QUEUESIZE 21

enum rscType {CPUCYCLE, IOWAIT, QUEUEMODE, MEMUSAGE, THREAD, UNKNOWNTYPE};

typedef struct queue{
    unsigned int head;
    unsigned int tail;
    unsigned int size;
    unsigned int maxSize;

    long long *array;
} queue;

/**
 * @brief judge if the queue is empty.
*/
int isEmpty(const queue *q);

/**
 * @brief judge if the queue is full.
*/
int isFull(const queue *q);

/**
 * @brief remove the front element of queue.
*/
long long dequeue(queue *q);

/**
 * @brief enqueue an element to a queue.
*/
void enqueue(queue *q, long long value);

/**
 * @brief get last element
*/
long long getTailElement(queue *q);

/**
 * @brief traverse and print
*/
void printQueue(queue *q);
void fprintQueue(queue *q, FILE *fp);

/**
 * @brief average queue
*/
long long averageQueue(queue *q);
long long monitorAvgNormalTps(queue *q);
long long avgQueueInGap(queue *q, int start, int end);
#endif