#include "ll.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

LL *list_create(void) {
    LL *l = (LL *) malloc(sizeof(LL));
    if (l == NULL) {
        free(l);
        return NULL;
    }
    l->head = NULL;
    return l;
}

bool list_add(LL *l, item *i) { // Checks if key already exists
    // Adds new node if key does not exist
    Node *new_node = (Node *) malloc(sizeof(Node));
    if (new_node == NULL) {
        free(new_node);
        return false;
    }
    new_node->data = *i;
    new_node->next = l->head;
    l->head = new_node;
    return true;
}

item *list_find(LL *l, bool (*cmpfn)(item *, item *), item *i) {
    Node *n = l->head;
    while (n != NULL) {
        if (cmpfn(&n->data, i)) {
            return &n->data;
        }
        n = n->next;
    }
    return NULL;
}

void list_destroy(LL **ll) {
    Node *current = (*ll)->head;
    Node *next;

    // Iterates through whole LL
    while (current != NULL) {
        next = current->next;
        if (current->data.id) {
            rwlock_delete(&(current->data.id));
        }
        free(current);
        current = next;
    }
    free(*ll);
    *ll = NULL;
    return;
}

void list_remove(LL *ll, bool (*cmpfn)(item *, item *), item *iptr) {
    item *found_item = list_find(ll, cmpfn, iptr);
    // if list_find returns null, return
    if (found_item == NULL) {
        return;
    }

    // if item is first node, remove and set second node to head
    if (cmpfn(iptr, &(ll->head->data))) {
        Node *removed_node = ll->head;
        ll->head = ll->head->next;
        free(removed_node);
    } else {
        // Remove node in middle of ll
        Node *current = ll->head->next;
        Node *previous = ll->head;

        while (current != NULL) {
            if (cmpfn(iptr, &(current->data))) {
                Node *removed_node = current;
                current = current->next;
                previous->next = current;
                free(removed_node);
                break;
            }

            previous = current;
            current = current->next;
        }
    }
    return;
}
