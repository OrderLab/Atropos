#include "hashMap.h"
#include "cancellable.h"

void hashMapReadLock(hashMap *map)
{
    if (!map->isLocal){
        pthread_rwlock_rdlock(map->rwLock);
    }
}

void hashMapWriteLock(hashMap *map)
{
    if (!map->isLocal){
        pthread_rwlock_wrlock(map->rwLock);
    }
}

void hashMapUnlock(hashMap *map)
{
    if (!map->isLocal){
        pthread_rwlock_unlock(map->rwLock);
    }
}

hashMap *createHashMap(int size)
{
    hashMap *newHashMap = (hashMap *) malloc(sizeof(hashMap));
    newHashMap->size = 0;
    newHashMap->arraySize = size;
    newHashMap->mapArray = (hashMapItem *) malloc(size * sizeof(hashMapItem));
    newHashMap->isLocal = 0;

    for (unsigned int i = 0; i < newHashMap->arraySize; ++i){
        newHashMap->mapArray[i].inUse = 0;
    }

    return newHashMap;
}

hashMap *createLocalHashMap(int size)
{
    hashMap *newHashMap = (hashMap *) malloc(sizeof(hashMap));
    newHashMap->size = 0;
    newHashMap->arraySize = size;
    newHashMap->mapArray = (hashMapItem *) malloc(size * sizeof(hashMapItem));
    newHashMap->isLocal = 1;

    for (unsigned int i = 0; i < newHashMap->arraySize; ++i){
        newHashMap->mapArray[i].inUse = 0;
    }

    return newHashMap;
}

unsigned int hashKey(unsigned long long key, unsigned int size)
{
    key += (key << 12);
    key ^= (key >> 22);
    key += (key << 4);
    key ^= (key >> 9);
    key += (key << 10);
    key ^= (key >> 2);
    key += (key << 7);
    key ^= (key >> 12);

    key = (key >> 3) * 2654435761;

    return key % size;
}

void _insert(hashMapItem *mapArray, unsigned int size, unsigned long long key, void *value)
{
    unsigned int start = hashKey(key, size);
    unsigned int i = start;
    unsigned int j = 0;
    while (j < size){

        // find position
        if (mapArray[i].inUse == 0){
            mapArray[i].inUse = 1;
            mapArray[i].key = key;
            mapArray[i].value = value;

            break;
        }
        else{
            i = (i+1)%size;
        }

        j++;
    }
}

void resizeHashMap(hashMap *map)
{
    unsigned int newArraySize = map->arraySize * 2;
    hashMapItem *newMapArray = (hashMapItem *) malloc(newArraySize * sizeof(hashMapItem)); 
    hashMapItem *oldMapArray = map->mapArray;

    for (unsigned int i = 0; i < newArraySize; ++i){
        newMapArray[i].inUse = 0;
    }

    // push item in old map into new one
    for (unsigned int i = 0; i < map->arraySize; ++i){
        if (oldMapArray[i].inUse != 0){
            _insert(newMapArray, newArraySize, oldMapArray[i].key, oldMapArray[i].value);
        }
    }

    map->arraySize = newArraySize;
    map->mapArray = newMapArray;
    free(oldMapArray);
}

void insert(hashMap *map, unsigned long long key, void *value)
{
    hashMapWriteLock(map);
    int pos = _get(map->mapArray, map->arraySize, key);
    if (pos != -1){
        hashMapUnlock(map);
        return ;
    }
    _insert(map->mapArray, map->arraySize, key, value);
    map->size++;

    if (map->size > map->arraySize/2){
        resizeHashMap(map);
    }
    hashMapUnlock(map);
}

int _get(hashMapItem *mapArray, unsigned int size, unsigned long long key)
{
    unsigned int start = hashKey(key, size);
    unsigned int i = start;
    unsigned int j = 0;
    while (j < size){

        // find position
        if (mapArray[i].inUse && mapArray[i].key == key){
            return i;
        }
        else{
            i = (i+1)%size;
        }

        j++;
    }

    return -1;
}

void *get(hashMap *map, unsigned long long key)
{
    hashMapReadLock(map);
    int pos = _get(map->mapArray, map->arraySize, key);
    hashMapUnlock(map);
    if (pos == -1){             // later we need to check if mapArray[pos] is still inUse
        return NULL;
    }
    return map->mapArray[pos].value;
}

//TODO: let freecancellable call removeentry
void removeCancellable(hashMap *map, unsigned long long key)
{
    hashMapWriteLock(map);
    unsigned int pos = _get(map->mapArray , map->arraySize, key);

    map->mapArray[pos].inUse = 0;
    freeCancellable((cancellable *) map->mapArray[pos].value);
    map->size--;
    hashMapUnlock(map);
}

void removeEntry(hashMap *map, unsigned long long key, int needClean)
{
    hashMapWriteLock(map);
    int pos = _get(map->mapArray , map->arraySize, key);

    if (pos == -1){
        hashMapUnlock(map);
        return ;
    }

    map->mapArray[pos].inUse = 0;
    if (needClean && map->mapArray[pos].value){
        free(map->mapArray[pos].value);
        map->mapArray[pos].value = NULL;
    }

    map->size--;
    hashMapUnlock(map);
}

void destroyHashMap(hashMap *map)
{
    for (unsigned i = 0; i < map->arraySize; ++i){
        if (map->mapArray[i].inUse && map->mapArray[i].value != NULL){
            free(map->mapArray[i].value);
        }
    }
    free(map->mapArray);
    free(map);
}

void printHashMap(hashMap *map)
{
    for (unsigned i = 0; i < map->arraySize; ++i){
        if (map->mapArray[i].inUse){
            printf("%lu, ", map->mapArray[i].key);
        }
        else{
            printf("null, ");
        }
    }

    printf("\n");
}

static char* str2Key(char *s){
    int l = strlen(s) + 1;
    char *ns= (char*) malloc (l*sizeof(char));
    strcpy(ns, s);
    if(ns == NULL)
        return NULL;
    else
        return ns;
}

hashMapStrKey *createHashMapStrKey(int size)
{
    hashMapStrKey *newHashMap = (hashMapStrKey *) malloc(sizeof(hashMapStrKey));
    newHashMap->size = 0;
    newHashMap->arraySize = size;
    newHashMap->mapArray = (hashMapStrKeyItem *) malloc(size * sizeof(hashMapStrKeyItem));

    for (unsigned int i = 0; i < newHashMap->arraySize; ++i){
        newHashMap->mapArray[i].inUse = 0;
        newHashMap->mapArray[i].oldValue = 0;
        newHashMap->mapArray[i].timeStamp = 0;
        newHashMap->mapArray[i].rscMtc.oldUsageVal = 0;
        newHashMap->mapArray[i].rscMtc.oldWaitVal = 0;
        newHashMap->mapArray[i].rscMtc.usageVal = 0;
        newHashMap->mapArray[i].rscMtc.waitVal = 0;
        newHashMap->mapArray[i].type = UNKNOWNTYPE;
        newHashMap->mapArray[i].lastTaskCpuTime = 0;
        newHashMap->mapArray[i].lastTaskIoBytes = 0;
        newHashMap->mapArray[i].lastTaskIoDelay = 0;
    }

    return newHashMap;
}

unsigned int hashStr(char *s, unsigned int size)
{
    unsigned int h = 0;
    for(; *s; s++){
        h = *s + h*31;
    }
    return h % size;
}

void _insertStrKeyItem(hashMapStrKeyItem *mapArray, unsigned int size, char *key, void *value, long long oldValue, int directCopy, long long timeStamp, enum rscType type)
{
    unsigned int start = hashStr(key, size);
    unsigned int i = start;
    unsigned int j = 0;
    while (j < size){

        // find position
        if (mapArray[i].inUse == 0){
            mapArray[i].inUse = 1;
            mapArray[i].key = directCopy ? key : str2Key(key);
            mapArray[i].value = value;
            mapArray[i].clientVal = value;
            mapArray[i].oldValue = oldValue;
            mapArray[i].timeStamp = timeStamp;
            mapArray[i].rscMtc.usageVal = (long long)value;
            mapArray[i].type = type;

            break;
        }
        else{
            i = (i+1)%size;
        }

        j++;
    }
}

void _insertStrKeyDouble(hashMapStrKeyItem *mapArray, unsigned int size, char *key, double value)
{
    unsigned int start = hashStr(key, size);
    unsigned int i = start;
    unsigned int j = 0;
    while (j < size){

        // find position
        if (mapArray[i].inUse == 0){
            mapArray[i].inUse = 1;
            mapArray[i].key = str2Key(key);
            mapArray[i].dValue = value;
            break;
        }
        else{
            i = (i+1)%size;
        }

        j++;
    }
}

static void _copyItemToNewMapArray(hashMapStrKeyItem *newMapArray, unsigned int size, hashMapStrKeyItem *oldStrkeyItem)
{
    unsigned int start = hashStr(oldStrkeyItem->key, size);
    unsigned int i = start;
    unsigned int j = 0;
    while (j < size){

        // find position
        if (newMapArray[i].inUse == 0){
            newMapArray[i] = *oldStrkeyItem;
            newMapArray[i].key = oldStrkeyItem->key;
            break;
        }
        else{
            i = (i+1)%size;
        }

        j++;
    }
}

void resizeHashMapStrKey(hashMapStrKey *map)
{
    unsigned int newArraySize = map->arraySize * 2;
    hashMapStrKeyItem *newMapArray = (hashMapStrKeyItem *) malloc(newArraySize * sizeof(hashMapStrKeyItem)); 
    hashMapStrKeyItem *oldMapArray = map->mapArray;

    // push item in old map into new one
    for (unsigned int i = 0; i < map->arraySize; ++i){
        if (oldMapArray[i].inUse != 0){
            //_insertStrKeyItem(newMapArray, newArraySize, oldMapArray[i].key, oldMapArray[i].value, oldMapArray[i].oldValue, 1, oldMapArray[i].timeStamp);
            _copyItemToNewMapArray(newMapArray, newArraySize, &oldMapArray[i]);
        }
    }

    map->arraySize = newArraySize;
    map->mapArray = newMapArray;
    free(oldMapArray);
}

void insertStrKeyItem(hashMapStrKey *map, char *key, void *value, long long timeStamp, enum rscType type)
{
    _insertStrKeyItem(map->mapArray, map->arraySize, key, value, 0, 0, timeStamp, type);
    map->size++;

    if (map->size > map->arraySize/2){
        resizeHashMapStrKey(map);
    }
}

void insertStrKeyDouble(hashMapStrKey *map, char *key, double value)
{
    _insertStrKeyDouble(map->mapArray, map->arraySize, key, value);
    map->size++;

    if (map->size > map->arraySize/2){
        resizeHashMapStrKey(map);
    }
}

int _getStrKeyItem(hashMapStrKeyItem *mapArray, unsigned int size, char *key)
{
    unsigned int start = hashStr(key, size);
    unsigned int i = start;
    unsigned int j = 0;
    while (j < size){

        // find position
        if (mapArray[i].inUse && !strcmp(mapArray[i].key, key)){
            return i;
        }
        else{
            i = (i+1)%size;
        }

        j++;
    }

    return -1;
}

void *getStrKeyItem(hashMapStrKey *map, char *key)
{
    int pos = _getStrKeyItem(map->mapArray, map->arraySize, key);
    if (pos == -1){             // later we need to check if mapArray[pos] is still inUse
        return NULL;
    }
    return map->mapArray[pos].value;
}

double getStrKeyDouble(hashMapStrKey *map, char *key)
{
    int pos = _getStrKeyItem(map->mapArray, map->arraySize, key);
    if (pos == -1){             // later we need to check if mapArray[pos] is still inUse
        return 0;
    }

    return map->mapArray[pos].dValue;
}

void destroyHashMapStrKey(hashMapStrKey *map)
{
    for (unsigned i = 0; i < map->arraySize; ++i){
        if (map->mapArray[i].inUse && map->mapArray[i].key != NULL){
            free(map->mapArray[i].key);
        }
    }
    free(map->mapArray);
    free(map);
}