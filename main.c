
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

typedef struct intlist_element_t {
    struct intlist_element_t* next;
    struct intlist_element_t* prev;
    int value;
} intlist_element;

typedef struct intlist_t {
    intlist_element* head;
    intlist_element* tail;
    pthread_mutex_t* lock;
    int size;
}intlist;

//no need for thread safe
void intlist_init(intlist* list)
{
    int error_code;
    pthread_mutexattr_t attr;

    list->size = 0;
    error_code = pthread_mutexattr_init(&attr);
    if (error_code)
    {
        printf("Error in pthread_mutex_init(): %s\n", strerror(error_code));
        exit(-1);
    }
    error_code = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (error_code)
    {
        printf("Error in pthread_mutex_init(): %s\n", strerror(error_code));
        exit(-1);
    }
    error_code = pthread_mutex_init(list->lock, &attr);
    if (error_code)
    {
        printf("Error in pthread_mutex_init(): %s\n", strerror(error_code));
        exit(-1);
    }
    list->head = NULL;
    list->tail = NULL;
}

//no need for thread safe
void intlist_destroy(intlist* list)
{
    intlist_element *temp1, *temp2;

    pthread_mutex_destroy(list->lock);
    temp1 = list->head;
    while(NULL != temp1)
    {
        temp2 = temp1;
        temp1 = temp2->next;
        free(temp2);
    }
}

//NEED THREAD SAFE
void intlist_push_head(intlist* list, int value)
{
    intlist_element* temp;

    //Creating a list element containing the given value;
    if(NULL == (temp = (intlist_element*)malloc(sizeof(temp))))
    {
        printf("Error allocating memory to add a new elemet to the list: %s", strerror(errno));
        exit(-1);
    }
    temp->next = NULL;
    temp->prev = NULL;
    temp->value = value;

    pthread_mutex_lock(list->lock);
    if(NULL == list->head)
    {
        list->head = temp;
        list->tail = temp;
    }
    else
    {
        list->tail->next = temp;
        temp->prev = list->tail;
        list->tail = temp;
    }
    ++list->size;
    pthread_mutex_unlock(list->lock);
}

//NEED THREAD SAFE
//blocking, if the list is empty - wait until an item is available and then pop it.
int intlist_pop_tail(intlist* list);

//NEED THREAD SAFE
//if k is larger than the list size, it removes whatever items are in the list and finishes.
void intlist_remove_last_k(intlist* list, int k);

//NEED THREAD SAFE
int intlist_size(intlist* list);

//NEED THREAD SAFE
pthread_mutex_t* intlist_get_mutex(intlist* list);

int main()
{
    printf("Hello, World!\n");
    return 0;
}