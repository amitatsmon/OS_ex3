#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#define BASE 10

typedef struct intlist_element_t {
    struct intlist_element_t* next;
    struct intlist_element_t* prev;
    int value;
} intlist_element;
typedef struct intlist_t {
    int size;
    intlist_element* head;
    intlist_element* tail;
    pthread_mutex_t* lock;
    pthread_cond_t* empty_list_cond;
}intlist;

//Globals
intlist* global_intlist;
int MAX;
pthread_cond_t* too_many_elements_cond;

//no need for thread safe
void intlist_init(intlist* list) {
    int error_code;
    pthread_mutexattr_t attr;

    list->size = 0;
    list->head = NULL;
    list->tail = NULL;

    //Initialize lock
    if(NULL == (list->lock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t))))
    {
        printf("Error allocating in intlist_init(): %s", strerror(errno));
        exit(-1);
    }
    error_code = pthread_mutexattr_init(&attr);
    if (error_code)
    {
        printf("Error in pthread_mutexattr_init(): %s\n", strerror(error_code));
        exit(-1);
    }
    error_code = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (error_code)
    {
        printf("Error in pthread_mutexattr_settype(): %s\n", strerror(error_code));
        exit(-1);
    }
    error_code = pthread_mutex_init(list->lock, &attr);
    if (error_code)
    {
        printf("Error in pthread_mutex_init(): %s\n", strerror(error_code));
        exit(-1);
    }

    //Initialize empty_list_cond
    if(NULL == (list->empty_list_cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t))))
    {
        printf("Error allocating in intlist_init(): %s", strerror(errno));
        exit(-1);
    }
    error_code = pthread_cond_init(list->empty_list_cond, NULL);
    if (error_code)
    {
        printf("Error in pthread_cond_init(): %s\n", strerror(error_code));
        exit(-1);
    }
}

//no need for thread safe
void intlist_destroy(intlist* list) {
    intlist_element *temp1, *temp2;

    pthread_mutex_destroy(list->lock);
    free(list->lock);
    pthread_cond_destroy(list->empty_list_cond);
    free(list->empty_list_cond);
    list->size = 0;

    temp1 = list->head;
    while(NULL != temp1)
    {
        temp2 = temp1;
        temp1 = temp2->next;
        free(temp2);
    }
}

//NEED THREAD SAFE
void intlist_push_head(intlist* list, int value) {
    intlist_element* temp;

    //Creating a list element containing the given value;
    if(NULL == (temp = (intlist_element*)malloc(sizeof(intlist_element))))
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
        list->head->prev = temp;
        temp->next = list->head;
        list->head = temp;
    }
    list->size = list->size+1;
    pthread_cond_signal(list->empty_list_cond);
    pthread_mutex_unlock(list->lock);
}

//NEED THREAD SAFE
//blocking, if the list is empty - wait until an item is available and then pop it.
int intlist_pop_tail(intlist* list) {
    intlist_element* temp;
    int poped_value;

    pthread_mutex_lock(list->lock);
    while (0 == list->size)
    {
        pthread_cond_wait(list->empty_list_cond, list->lock); //block if the list is empty
    }
    temp = list->tail;
    poped_value = temp->value;
    if(NULL == list->tail->prev)
    {
        list->tail = NULL;
        list->head = NULL;
    }
    else
    {
        list->tail = list->tail->prev;
    }
    --list->size;
    free(temp);
    pthread_mutex_unlock(list->lock);
    return poped_value;
}

//NEED THREAD SAFE
//if k is larger than the list size, it removes whatever items are in the list and finishes.
void intlist_remove_last_k(intlist* list, int k) {
    int num_to_remove, i;
    intlist_element* temp, *to_remove = NULL;

    pthread_mutex_lock(list->lock);
    num_to_remove = (list->size < k)? list->size : k; //num_to_remove is the minimum of the size of the list and k;
    if (0 < num_to_remove)
    {
        if (num_to_remove < list->size)
        {
            temp = list->tail;
            for(i=0; i<num_to_remove; ++i)
            {
                temp = temp->prev;
            }
            temp->next->prev = NULL;
            to_remove = temp->next;
            temp->next = NULL;
            list->tail = temp;
        }
        else //Means list-size == num_to_remove
        {
            to_remove = list->head;
            list->head = NULL;
            list->tail = NULL;
        }
        list->size -= num_to_remove;
    }
    pthread_mutex_unlock(list->lock);

    while(NULL != to_remove)
    {
        temp = to_remove;
        to_remove = to_remove->next;
        free(temp);
    }
}

//NEED THREAD SAFE
int intlist_size(intlist* list) {
    return list->size;
}

//NEED THREAD SAFE
pthread_mutex_t* intlist_get_mutex(intlist* list) {
    return list->lock;
}


//Thread functions
void* writer_function(void* nothing)
{
    while(1)
    {
        intlist_push_head(global_intlist, rand());
        if(MAX < intlist_size(global_intlist))
        {
            pthread_cond_signal(&too_many_elements_cond);
        }
    }
}

void* reader_function(void* nothing)
{
    while(1)
    {
        intlist_pop_tail(global_intlist);
        if(MAX < intlist_size(global_intlist))
        {
            pthread_cond_signal(too_many_elements_cond);
        }
    }
}

void* garbage_collector_function(void* nothing)
{
    pthread_mutex_t* list_lock = intlist_get_mutex(global_intlist);
    while(1)
    {
        pthread_cond_wait(too_many_elements_cond, list_lock); //waits until the list has more than MAX items

    }
}

int main(int argc, char** argv)
{
    int in_wnum, in_rnum, in_max, in_time, error_code;

    if (5 != argc)
    {
        printf("Incorrect usage.\nUse %s <WNUM> <RNUM> <MAX> <TIME>\n", argv[0]);
        return -1;
    }

    srand(time(NULL)); //Initialize random seed

    in_wnum = (int)strtol(argv[1], NULL, BASE);
    in_rnum = (int)strtol(argv[1], NULL, BASE);
    in_max = (int)strtol(argv[1], NULL, BASE);
    in_time = (int)strtol(argv[1], NULL, BASE);

    MAX = in_max; //Initialize global variable MAX

    //Define and initialize a global double-linked list of integers.
    if(NULL == (global_intlist = (intlist*)malloc(sizeof(intlist))))
    {
        printf("Error in main(): %s\n", strerror(errno));
        return -1;
    }
    intlist_init(global_intlist);

    //Create a condition variable for the garbage collector.
    if(NULL == (too_many_elements_cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t))))
    {
        printf("Error in main(): %s\n", strerror(errno));
        return -1;
    }
    error_code = pthread_cond_init(too_many_elements_cond, NULL);
    if (error_code)
    {
        printf("Error in main(): %s\n", strerror(error_code));
        exit(-1);
    }

    //Create garbage collector thread

    //Create WNUM threads for the writers

    //Create RNUM threads for the readers

    //Sleep TIME seconds

    //Stop all running threads (safly, avoid deadlocks)

    // print the size of the list and all items inside

    //Cleanup and exit
    intlist_destroy(global_intlist);
    free(global_intlist);
}