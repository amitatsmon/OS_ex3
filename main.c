#include <stdio.h>
#include <pthread.h>

typedef struct intlist_element_t
{
    struct intlist_element_t next;
    struct intlist_element_t prev;
    int value;
} intlist_element;

typedef struct intlist_t {
    intlist_element head;
    intlist_element tail;
    int size;
    pthread_mutex_t lock;
}intlist;

//no need for thred safe
void intlist_init(intlist* list);

//no need for thred safe
void intlist_destroy(intlist* list);

//NEED THREAD SAFE
pthread_mutex_t* intlist_get_mutex(intlist* list);

//NEED THREAD SAFE
void intlist_push_head(intlist* list, int value);

//NEED THREAD SAFE
//blocking, if the list is empty - wait until an item is available and then pop it.
int intlist_pop_tail(intlist* list);

//NEED THREAD SAFE
//if k is larger than the list size, it removes whatever items are in the list and finishes.
void intlist_remove_last_k(intlist* list, int k);

//NEED THREAD SAFE
int intlist_size(intlist* list);

int main()
{
    printf("Hello, World!\n");
    return 0;
}