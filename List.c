/*********************************************************************************
* Ethan Ma, ecma
* 2022 Winter CSE101 PA3
* List.c
* List ADT
*********************************************************************************/

#include "List.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// ----- Structs -----

typedef struct NodeObj *Node;

typedef struct NodeObj {
    int data;
    Node next;
    Node prev;
} NodeObj;

typedef struct ListObj {
    Node front;
    Node back;
    Node cursor;
    int length;
    int cursor_idx;
} ListObj;

// ----- Constructors - Destructors -----

// Creates and returns a new empty List.
Node newNode(int data) {
    Node N = malloc(sizeof(NodeObj));
    N->data = data;
    N->next = N->prev = NULL;
    return N;
}

// Creates and returns a new empty List.
List newList() {
    List L = malloc(sizeof(ListObj));
    L->front = L->back = L->cursor = NULL;
    L->length = 0;
    L->cursor_idx = -1;
    return L;
}

// Frees all heap memory associated with *n.
void freeNode(Node *n) {
    if (n == NULL || *n == NULL) {
        return;
    }
    free(*n);
    *n = NULL;
}

// Frees all heap memory associated with *pL.
void freeList(List *pL) {
    if (pL == NULL || *pL == NULL) {
        return;
    }
    while (length(*pL) > 0) {
        deleteFront(*pL);
    }
    free(*pL);
    *pL = NULL;
}

// ----- Access Functions -----

// Returns then umber of elements in L.
int length(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling length() on NULL list reference\n");
        exit(1);
    }
    return L->length;
}

// Returns the myindex of the cursor elemtn if defined, -1 otherwise.
int myindex(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling myindex() on NULL list reference\n");
        exit(1);
    }
    return L->cursor_idx;
}

// Returns front element of L. Pre: length()>0
int front(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling front() on NULL list reference\n");
        exit(1);
    }
    if (length(L) <= 0) {
        fprintf(stderr, "List Error: calling front() on empty list\n");
        exit(1);
    }
    return L->front->data;
}

// Returns back element of L. Pre: length()>0
int back(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling back() on NULL list reference\n");
        exit(1);
    }
    if (length(L) <= 0) {
        fprintf(stderr, "List Error: calling front() on empty list\n");
        exit(1);
    }
    return L->back->data;
}

// Returns cursor element of L. Pre: length()>0, myindex()>=0
int get(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling get() on NULL list reference\n");
        exit(1);
    }
    if (myindex(L) < 0) {
        fprintf(stderr, "List Error: calling get() with undefined cursor\n");
        exit(1);
    }
    if (length(L) <= 0) {
        fprintf(stderr, "List Error: calling get() on empty list\n");
        exit(1);
    }
    return L->cursor->data;
}

// Returns true iff Lists A and B are in same state, else false.
bool equals(List A, List B) {
    if (A == NULL && B == NULL) {
        return true;
    }
    if (A == NULL || B == NULL || length(A) != length(B)) {
        return false;
    }

    Node i = A->front;
    Node j = B->front;

    while (i != NULL && j != NULL) {
        if (i->data != j->data)
            return false;
        i = i->next;
        j = j->next;
    }
    return true;
}

// ----- Manipulation Procedures -----

// Resets L to its original empty state.
void clear(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling clear() on NULL list reference\n");
        exit(1);
    }
    Node n = L->front;
    while (n != NULL) {
        Node temp = n;
        n = n->next;
        freeNode(&temp);
    }
    L->front = L->back = L->cursor = NULL;
    L->length = 0;
    L->cursor_idx = -1;
}

// Overwrites the cursor element’s data with x.
// Pre: length()>0, myindex()>=0
void set(List L, int x) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling set() on NULL list reference\n");
        exit(1);
    }
    if (length(L) <= 0) {
        fprintf(stderr, "List Error: calling set() on empty list\n");
        exit(1);
    }
    if (myindex(L) < 0) {
        fprintf(stderr, "List Error: calling set() with undefined cursor\n");
        exit(1);
    }
    L->cursor->data = x;
}

// If L is non-empty, sets cursor under the front element,
// otherwise does nothing.
void moveFront(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling moveFront() on NULL list reference\n");
        exit(1);
    }
    if (length(L) > 0) {
        L->cursor = L->front;
        L->cursor_idx = 0;
    }
}

// If L is non-empty, sets cursor under the back element,
// otherwise does nothing.
void moveBack(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling moveBack() on NULL list reference\n");
        exit(1);
    }
    if (length(L) > 0) {
        L->cursor = L->back;
        L->cursor_idx = length(L) - 1;
    }
}

// If cursor is defined and not at front, move cursor one
// step toward the front of L; if cursor is defined and at
// front, cursor becomes undefined; if cursor is undefined
// do nothing
void movePrev(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling movePrev() on NULL list reference\n");
        exit(1);
    }
    if (L->cursor != NULL && myindex(L) == 0) {
        L->cursor = NULL;
        L->cursor_idx = -1;
    }
    if (L->cursor != NULL && myindex(L) != 0) {
        L->cursor = L->cursor->prev;
        L->cursor_idx--;
    }
}

// If cursor is defined and not at back, move cursor one
// step toward the back of L; if cursor is defined and at
// back, cursor becomes undefined; if cursor is undefined
// do nothing
void moveNext(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling moveNext() on NULL list reference\n");
        exit(1);
    }
    if (L->cursor != NULL && myindex(L) == length(L) - 1) {
        L->cursor = NULL;
        L->cursor_idx = -1;
    }
    if (L->cursor != NULL && myindex(L) != length(L) - 1) {
        L->cursor = L->cursor->next;
        L->cursor_idx++;
    }
}

// Insert new element into L. If L is non-empty,
// insertion takes place before front element.
void prepend(List L, int x) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling prepend() on NULL list reference\n");
        exit(1);
    }
    Node n = newNode(x);
    if (length(L) == 0) {
        L->front = n;
        L->back = n;
    } else {
        L->front->prev = n;
        n->next = L->front;
        L->front = n;
        L->cursor_idx++;
    }
    L->length++;
}

// Insert new element into L. If L is non-empty,
// insertion takes place after back element.
void append(List L, int x) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling append() on NULL list reference\n");
        exit(1);
    }
    Node n = newNode(x);
    if (length(L) == 0) {
        L->front = n;
        L->back = n;
    } else {
        L->back->next = n;
        n->prev = L->back;
        L->back = n;
    }
    L->length++;
}

// Insert new element before cursor.
// Pre: length()>0, myindex()>=0
void insertBefore(List L, int x) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling insertBefore() on NULL list reference\n");
        exit(1);
    }
    if (length(L) <= 0) {
        fprintf(stderr, "List Error: calling insertBefore() on empty list\n");
        exit(1);
    }
    if (L->cursor == NULL || myindex(L) == -1) {
        fprintf(stderr, "List Error: calling insertBefore() with undefined cursor\n");
        exit(1);
    }
    if (L->cursor == L->front) {
        prepend(L, x);
    } else {
        Node n = newNode(x);
        n->prev = L->cursor->prev;
        n->next = L->cursor;
        L->cursor->prev->next = n;
        L->cursor->prev = n;
        L->cursor_idx++;
        L->length++;
    }
}

// Insert new element after cursor.
// Pre: length()>0, myindex()>=0
void insertAfter(List L, int x) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling insertAfter() on NULL list reference\n");
        exit(1);
    }
    if (length(L) <= 0) {
        fprintf(stderr, "List Error: calling insertAfter() on empty list\n");
        exit(1);
    }
    if (L->cursor == NULL || myindex(L) == -1) {
        fprintf(stderr, "List Error: calling insertAfter() with undefined cursor\n");
        exit(1);
    }

    // back of list
    if (myindex(L) == length(L) - 1) {
        append(L, x);
    } else {
        Node n = newNode(x);
        n->prev = L->cursor;
        n->next = L->cursor->next;
        L->cursor->next = n;
        L->cursor->next->prev = n;
        L->length++;
    }
}

// Delete front element. Pre: length()>0
void deleteFront(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling deleteFront() on NULL list reference\n");
        exit(1);
    }
    if (length(L) <= 0) {
        fprintf(stderr, "List Error: calling deleteFront() on empty list\n");
        exit(1);
    }

    if (length(L) == 1) {
        clear(L);
    } else {
        Node temp = L->front;
        L->front = L->front->next;
        L->front->prev = NULL;
        L->length--;
        freeNode(&temp);
    }

    // myindex exists/cursor handling
    if (myindex(L) != -1) {
        L->cursor_idx--;
        if (myindex(L) == -1) {
            L->cursor = NULL;
        }
    }
}

// Delete back element. Pre: length()>0
void deleteBack(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling deleteBack() on NULL list reference\n");
        exit(1);
    }
    if (length(L) <= 0) {
        fprintf(stderr, "List Error: calling deleteBack() on empty list\n");
        exit(1);
    }
    if (length(L) == 1) {
        clear(L);
    } else {
        if (myindex(L) == length(L) - 1) {
            L->cursor_idx = -1;
            L->cursor = NULL;
        }
        Node temp = L->back;
        L->back = L->back->prev;
        L->back->next = NULL;
        L->length--;
        freeNode(&temp);
    }
}

// Delete cursor element, making cursor undefined.
// Pre: length()>0, myindex()>=0
void mydelete(List L) {
    if (L == NULL) {
        fprintf(stderr, "List Error: calling delete() on NULL list reference\n");
        exit(1);
    }
    if (length(L) <= 0) {
        fprintf(stderr, "List Error: calling delete() on empty list\n");
        exit(1);
    }
    if (myindex(L) < 0) {
        fprintf(stderr, "List Error: calling delete() with undefined cursor\n");
        exit(1);
    }
    if (length(L) == 1) {
        freeNode(&L->cursor);
        clear(L);
    } else {
        if (myindex(L) == 0) {
            deleteFront(L);
        } else if (myindex(L) == length(L) - 1) {
            deleteBack(L);
        } else {
            Node prev = L->cursor->prev;
            L->cursor->prev->next = L->cursor->next;
            L->cursor->next->prev = prev;
            L->length--;
            freeNode(&L->cursor);
            L->cursor = NULL;
            L->cursor_idx = -1;
        }
    }
}

// ----- Other Operations -----

// Prints to the file pointed to by out, a
// string representation of L consisting
// of a space separated sequence of integers, with front on left.
void printList(FILE *out, List L) {
    if (L == NULL) {
        fprintf(stderr, "List error: calling printList() on NULL list reference\n");
        exit(1);
    }
    Node n = L->front;
    for (n = L->front; n != NULL; n = n->next) {
        fprintf(out, "%d", n->data);
        if (n != L->back) {
            fprintf(out, " ");
        }
    }
}

// Returns a new List representing the same integer
// sequence as L. The cursor in the new list is undefined,
// regardless of the state of the cursor in L. The state of L is unchanged.
List copyList(List L) {
    if (L == NULL) {
        return NULL;
    }
    List nL = newList();
    Node nF = L->front;
    while (nF != NULL) {
        append(nL, nF->data);
        nF = nF->next;
    }
    return nL;
}
