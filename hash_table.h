#ifndef _HASH

#define _HASH

#include "ll.h"

#define TABLE_SIZE 128

typedef struct Hashtable Hashtable;

struct Hashtable {
    LL *table[TABLE_SIZE];
};

Hashtable *hash_create(void);

bool hash_put(Hashtable *, const char *key, rwlock_t *val);

rwlock_t **hash_get(Hashtable *, const char *key);

void hash_destroy(Hashtable **);

#endif
