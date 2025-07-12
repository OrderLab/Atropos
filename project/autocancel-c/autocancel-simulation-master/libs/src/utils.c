#include "utils.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>

void initializeQueue(queue *q, unsigned int maxSize)
{
    q->head = 0;
    q->tail = 0;
    q->size = 0;
    q->maxSize = maxSize;

    q->array = (long long *) malloc(sizeof(long long) * maxSize);
}

int isEmpty(const queue *q)
{
    if (q->head == q->tail){
        return 1;
    }
    else{
        return 0;
    }
}

int isFull(const queue *q)
{
    if ((q->tail + 1) % q->maxSize == q->head){
        return 1;
    }
    else{
        return 0;
    }
}

long long dequeue(queue *q)
{
    if (isEmpty(q)){
        perror("pop an empty queue!!");
        exit(-1);
    }
    long long retVal = q->array[q->head];
    q->head = (q->head + 1) % q->maxSize;
    q->size -= 1;
    return retVal;
}

void enqueue(queue *q, long long value)
{
    if (isFull(q)){
        dequeue(q);
        enqueue(q, value);
        return ;
    }

    q->array[q->tail%q->maxSize] = value;
    q->tail = (q->tail+1)%q->maxSize;
    q->size += 1;
}

long long getTailElement(queue *q)
{
    return q->array[(q->tail + q->maxSize - 1) % q->maxSize];
}

void printQueue(queue *q)
{
    for (unsigned int i = q->head; i != q->tail; i = (i+1)%q->maxSize){
        printf("%lld ", q->array[i]);
    }
    printf("\n");
}

void fprintQueue(queue *q, FILE *fp)
{
    for (unsigned int i = q->head; i != q->tail; i = (i+1)%q->maxSize){
        fprintf(fp, "%lld ", q->array[i]);
    }
    fprintf(fp, "\n");
}

long long averageQueue(queue *q)
{
    if (isEmpty(q)){
        return 0;
    }

    long long sum = 0;
    unsigned int c = 0;
    for (unsigned int i = q->head; i != q->tail; i = (i+1)%q->maxSize){
        sum += q->array[i];
        c++;
    }

    return sum / c;
}

long long monitorAvgNormalTps(queue *q)
{
    long long avgNormalTps = 0;
    long long normal[3] = {0, 0, 0};
    for (int i = 0; i < q->maxSize; i+= 10){
        if (i + 10 >= q->size){
            break;
        }

        long long requestPersec = 0;
        for (int j = 0; j < 10; ++j){
            int index = (q->head + i + j) % q->maxSize;
            requestPersec += q->array[index];
        }
        requestPersec = requestPersec / 10;

        // find smallest number in normal
        long long min = INT_MAX;
        int minIndex = 0;
        for (int k = 0; k < 3; ++k){
            if (normal[k] < min){
                min = normal[k];
                minIndex = k;
            }
        }

        if (requestPersec > normal[minIndex]){
            normal[minIndex] = requestPersec;
        }
    }

    for (int i = 0; i < 3; ++i){
        avgNormalTps += normal[i];
    }

    avgNormalTps = avgNormalTps/3;
    return avgNormalTps;
}

long long avgQueueInGap(queue *q, int start, int end)
{
    if (isEmpty(q)){
        return 0;
    }

    long long sum = 0;
    unsigned int c = 0;
    unsigned int extraTail = (q->head + end) % q->maxSize;
    unsigned int newHead = (q->head + start) % q->maxSize;

    for (unsigned int i = newHead; i != q->tail && i != extraTail; i = (i+1) % q->maxSize){
        sum += q->array[i];
        c++;
    }

    return sum / c;
}