#ifndef _LL

#define _LL

#include "item.h"

#include <stdbool.h>

typedef struct Node Node;

struct Node {
    item data;
    Node *next;
};

typedef struct LL LL;
struct LL {
    Node *head;
};

LL *list_create(void);
bool list_add(LL *, item *);
item *list_find(LL *, bool (*)(item *, item *), item *);

void list_destroy(LL **);
void list_remove(LL *, bool (*)(item *, item *), item *);

#endif
