#ifndef _ITEM
#define _ITEM

#include <stdbool.h>
#include "rwlock.h"

typedef struct item item;

struct item {
    char key[255];
    rwlock_t *id;
};

bool cmp(item *, item *);

#endif
