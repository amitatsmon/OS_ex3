#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

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
int MAX, continue_running  = 0;
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
        printf("Error allocating in malloc(): %s", strerror(errno));
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
    int error_code;

    //Creating a list element containing the given value;
    if(NULL == (temp = (intlist_element*)malloc(sizeof(intlist_element))))
    {
        printf("Error allocating memory to add a new elemet to the list: %s\n", strerror(errno));
        exit(-1);
    }
    temp->next = NULL;
    temp->prev = NULL;
    temp->value = value;

    if (0 != (error_code = pthread_mutex_lock(list->lock)))
    {
        printf("Error in pthread_mutex_lock(): %s\n", strerror(error_code));
        exit(-1);
    }

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
    if (0 != (error_code = pthread_mutex_unlock(list->lock)))
    {
        printf("Error in pthread_mutex_unlock(): %s\n", strerror(error_code));
        exit(-1);
    }
}

//NEED THREAD SAFE
//blocking, if the list is empty - wait until an item is available and then pop it.
int intlist_pop_tail(intlist* list) {
    intlist_element* temp;
    int poped_value, error_code;

    if (0 != (error_code = pthread_mutex_lock(list->lock)))
    {
        printf("Error in pthread_mutex_lock(): %s\n", strerror(error_code));
        exit(-1);
    }
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
        list->tail->next = NULL;
    }
    --list->size;
    free(temp);
    if (0 != (error_code = pthread_mutex_unlock(list->lock)))
    {
        printf("Error in pthread_mutex_unlock(): %s\n", strerror(error_code));
        exit(-1);
    }
    return poped_value;
}

//NEED THREAD SAFE
//if k is larger than the list size, it removes whatever items are in the list and finishes.
void intlist_remove_last_k(intlist* list, int k) {
    int num_to_remove, i, error_code;
    intlist_element* temp, *to_remove = NULL;

    if(k<0)
    {
        exit(-1);
    }

    if (0 != (error_code = pthread_mutex_lock(list->lock)))
    {
        printf("Error in pthread_mutex_lock(): %s\n", strerror(error_code));
        exit(-1);
    }
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

    while(NULL != to_remove)
    {
        temp = to_remove;
        to_remove = to_remove->next;
        free(temp);
    }
    if (0 != (error_code = pthread_mutex_unlock(list->lock)))
    {
        printf("Error in pthread_mutex_unlock(): %s\n", strerror(error_code));
        exit(-1);
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
    int error_code;
    pthread_mutex_t* list_lock = intlist_get_mutex(global_intlist);
    while(continue_running < 2)
    {
//        if (0 != (error_code = pthread_mutex_lock(list_lock)))
//        {
//            printf("Error in pthread_mutex_lock(): %s\n", strerror(error_code));
//            exit(-1);
//        }

        intlist_push_head(global_intlist, rand());
        if(MAX < intlist_size(global_intlist))
        {
            pthread_cond_signal(too_many_elements_cond);
        }

//        if (0 != (error_code = pthread_mutex_unlock(list_lock)))
//        {
//            printf("Error in pthread_mutex_unlock(): %s\n", strerror(error_code));
//            exit(-1);
//        }
    }
    pthread_exit(NULL);
}

void* reader_function(void* nothing)
{
    pthread_mutex_t* list_lock = intlist_get_mutex(global_intlist);
    int error_code;
    while(0 == continue_running)
    {
//        if (0 != (error_code = pthread_mutex_lock(list_lock)))
//        {
//            printf("Error in pthread_mutex_lock(): %s\n", strerror(error_code));
//            exit(-1);
//        }

        intlist_pop_tail(global_intlist);
        if(MAX < intlist_size(global_intlist))
        {
            pthread_cond_signal(too_many_elements_cond);
        }

//        if (0 != (error_code = pthread_mutex_unlock(list_lock)))
//        {
//            printf("Error in pthread_mutex_unlock(): %s\n", strerror(error_code));
//            exit(-1);
//        }
    }
    pthread_exit(NULL);
}

void* garbage_collector_function(void* nothing)
{
    pthread_mutex_t* list_lock = intlist_get_mutex(global_intlist);
    int num_to_remove, error_code;
    if (0 != (error_code = pthread_mutex_lock(list_lock)))
    {
        printf("Error in pthread_mutex_lock(): %s\n", strerror(error_code));
        exit(-1);
    }
    while(continue_running < 2)
    {

        pthread_cond_wait(too_many_elements_cond, list_lock); //waits until the list has more than MAX items

        while(MAX < intlist_size(global_intlist))
        {
            num_to_remove = (intlist_size(global_intlist)+1)/2;
            intlist_remove_last_k(global_intlist, num_to_remove);

            printf("GC - %d items removed from the list\n", num_to_remove);
        }
    }
    if (0 != (error_code = pthread_mutex_unlock(list_lock)))
    {
        printf("Error in pthread_mutex_unlock(): %s\n", strerror(error_code));
        exit(-1);
    }
    pthread_exit(NULL);
}

void print_list(intlist* list)
{
    int end_size,i;
    intlist_element* temp;
    end_size = intlist_size(list);
    printf("size is: %d\n", end_size);
    printf("head is: %d\n", list->head->value);
    temp = list->head;
    for (i = 0; i < end_size; ++i)
    {
        printf("%d ", temp->value);
        temp = temp->next;
    }
    printf("\n");
}

int main(int argc, char** argv)
{
    uint in_wnum, in_rnum, in_max, in_time;
    int error_code, i, end_size;
    pthread_t *writers, *readers, garbage_collector;

//    if(NULL == (global_intlist = (intlist*)malloc(sizeof(intlist))))
//    {
//        printf("Error in malloc(): %s\n", strerror(errno));
//        return -1;
//    }
//    intlist_init(global_intlist);
//
//    intlist_push_head(global_intlist, 1);
//    intlist_push_head(global_intlist, 2);
//    intlist_push_head(global_intlist, 3);
//    intlist_push_head(global_intlist, 4);
//    intlist_push_head(global_intlist, 5);
//    intlist_pop_tail(global_intlist);
//    intlist_push_head(global_intlist, 6);
//    intlist_push_head(global_intlist, 7);
//    intlist_pop_tail(global_intlist);
//    print_list(global_intlist);
//    intlist_remove_last_k(global_intlist, 9);
//    intlist_push_head(global_intlist, 1);
//
//    print_list(global_intlist);
//    intlist_destroy(global_intlist);
//    free(global_intlist);
//    return 0;


    if (5 != argc)
    {
        printf("Incorrect usage.\nUse %s <WNUM> <RNUM> <MAX> <TIME>\n", argv[0]);
        return -1;
    }

    srand((uint)time(NULL)); //Initialize random seed

    in_wnum = (uint)strtol(argv[1], NULL, BASE);
    in_rnum = (uint)strtol(argv[2], NULL, BASE);
    in_max = (uint)strtol(argv[3], NULL, BASE);
    in_time = (uint)strtol(argv[4], NULL, BASE);

    MAX = in_max; //Initialize global variable MAX


    //Define and initialize a global double-linked list of integers
    if(NULL == (global_intlist = (intlist*)malloc(sizeof(intlist))))
    {
        printf("Error in malloc(): %s\n", strerror(errno));
        return -1;
    }
    intlist_init(global_intlist);

    //Create a condition variable for the garbage collector
    if(NULL == (too_many_elements_cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t))))
    {
        printf("Error in malloc(): %s\n", strerror(errno));
        return -1;
    }
    error_code = pthread_cond_init(too_many_elements_cond, NULL);
    if (error_code)
    {
        printf("Error in pthread_cond_init(): %s\n", strerror(error_code));
        return -1;
    }

    printf("Creating GC\n");
    //Create garbage collector thread
    error_code = pthread_create(&garbage_collector,  NULL, garbage_collector_function, NULL);
    if(error_code)
    {
        printf("Error in pthread_create(): %s\n", strerror(error_code));
        return -1;
    }

    printf("Creating writers\n");
    //Create WNUM threads for the writers
    if(NULL == (writers = (pthread_t*)malloc(sizeof(pthread_t)*in_wnum)))
    {
        printf("Error in malloc(): %s\n", strerror(errno));
        return -1;
    }
    for (i = 0; i < in_wnum; ++i)
    {
        error_code = pthread_create(&writers[i], NULL, writer_function, NULL);
        if(error_code)
        {
            printf("Error in pthread_create(): %s\n", strerror(error_code));
            return -1;
        }
    }

    printf("Creating readers\n");
    //Create RNUM threads for the readers
    if(NULL == (readers = (pthread_t*)malloc(sizeof(pthread_t)*in_rnum)))
    {
        printf("Error in malloc(): %s\n", strerror(errno));
        return -1;
    }
    for (i = 0; i < in_rnum; ++i)
    {
        error_code = pthread_create(&readers[i], NULL, reader_function, NULL);
        if(error_code)
        {
            printf("Error in pthread_create(): %s\n", strerror(error_code));
            return -1;
        }
    }

    printf("Sleeping............\n");
    //Sleep TIME seconds
    sleep(in_time);

    printf("Time to stop!!\n");
    //Stop all running threads (safely, avoid deadlocks)
    continue_running = 1;

    for (i = 0; i < in_rnum; ++i)
    {
        error_code = pthread_join(readers[i], NULL);
        if(error_code)
        {
            printf("Error in pthread_join: %s\n", strerror(error_code));
            return -1;
        }
    }
    continue_running = 2;
    for (i = 0; i < in_wnum; ++i)
    {
        error_code = pthread_join(writers[i], NULL);
        if(error_code)
        {
            printf("Error in pthread_join: %s\n", strerror(error_code));
            return -1;
        }
    }
    pthread_cond_signal(too_many_elements_cond); //To wake up the GC
    error_code = pthread_join(garbage_collector, NULL);
    if(error_code)
    {
        printf("Error in pthread_join: %s\n", strerror(error_code));
        return -1;
    }

    // print the size of the list and all items inside
    end_size = intlist_size(global_intlist);
    printf("size is: %d\n", end_size);
//    for (i = 0; i < end_size; ++i)
//    {
//        printf("%d ", intlist_pop_tail(global_intlist));
//    }
//    printf("\n");

    //Cleanup and exit
    free(readers);
    free(writers);
    intlist_destroy(global_intlist);
    free(global_intlist);
    pthread_cond_destroy(too_many_elements_cond);
    free(too_many_elements_cond);
    return 0;
}

//There is a deadlock if the readers want to read and there is nothing to read in the end