#include "../include/hashMap.h"
#include "../include/cancellable.h"
#include <stdio.h>

int main(void)
{
    hashMap *map = createHashMap(32);
    
    cancellable *c = easyCreateCancel(0);
    cancellable *c2 = easyCreateCancel(0);
    insert(map, 1, c);
    insert(map, 4245, c2);

    print(map);

    /* test rehash */
    for (int i = 1; i <= 15; ++i){
        cancellable *c3 = easyCreateCancel(0);
        insert(map, i*29, c3);
    }

    print(map);
    destroyHashMap(map);
    return 0;
}
