#include "hash_table.h"

#include "hash_fn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Create a new hash table
Hashtable *hash_create(void) {
    Hashtable *ht = malloc(sizeof(Hashtable));
    if (ht == NULL) {
        return NULL;
    }
    for (int i = 0; i < TABLE_SIZE; i++) {
        ht->table[i] = list_create();
        if (ht->table[i] == NULL) {
            free(ht);
            return NULL;
        }
    }
    return ht;
}

// Put a key-value pair into the hash table
bool hash_put(Hashtable *ht, const char *key, rwlock_t *val) {
    rwlock_t **existing_val = (rwlock_t **) hash_get(ht, key);
    if (existing_val != NULL) {
        // If the key already exists, update its value
        *existing_val = val;
        return true;
    } else {
        // If the key does not exist, create a new item and add it to the list
        item new_item;
        snprintf(new_item.key, sizeof(new_item.key), "%s", key);
        new_item.id = val;
        size_t index = hash(key) % TABLE_SIZE;
        return list_add(ht->table[index], &new_item);
    }
}

// Searches hash table for key and returns if found, otherwise returns NULL
rwlock_t **hash_get(Hashtable *ht, const char *key) {
    item search_item;
    snprintf(search_item.key, sizeof(search_item.key), "%s", key);

    // Use the hash function to determine the index
    size_t index = hash(key) % TABLE_SIZE;

    item *found_item = list_find(ht->table[index], cmp, &search_item);
    if (found_item != NULL) {
        return &(found_item->id);
    } else {
        return NULL;
    }
}

// Destroy the hash table and free all memory
void hash_destroy(Hashtable **ht) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        list_destroy(&(*ht)->table[i]);
    }
    free(*ht);
    *ht = NULL;
}
