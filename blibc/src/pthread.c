#include <pthread.h>
#include <bc_extsc.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>


struct thread_info {
    tid_t tid;
    void* ret;
    volatile int done;
    int joined;

    pthread_mutex_t mut;
    pthread_cond_t cond;
};

struct thread_info** thread_table;
static int thread_table_sz;
static pthread_mutex_t thread_table_mut;


// called at startup by crt0.c
void __pthread_init(void) {
    pthread_mutex_init(&thread_table_mut, NULL);
}


static
void free_thread_info(struct thread_info* ti) {
    (void) ti;
    // nothing to free yet
}


struct thread_info* acquire_thread_info(tid_t tid) {
    int err = pthread_mutex_lock(&thread_table_mut);

    assert(!err);

    // sequential search
    // @todo replace with binary search
    for(int i = 0; i < thread_table_sz; i++) {
        struct thread_info* ti = thread_table[i];

        err = pthread_mutex_lock(&ti->mut);
        assert(!err);

        if(ti->tid == tid) {
            // found
            pthread_mutex_unlock(&thread_table_mut);

            return ti;
        }

        pthread_mutex_unlock(&ti->mut);
    }

    pthread_mutex_unlock(&thread_table_mut);
    return NULL;
}


static void release_thread_info(struct thread_info* ti) {
    pthread_mutex_unlock(&ti->mut);
}


static void insert_thread_info(struct thread_info* ti) {
    int err = pthread_mutex_lock(&thread_table_mut);
    assert(!err);

    thread_table = realloc(thread_table, (thread_table_sz + 1) * sizeof(ti));

    thread_table[thread_table_sz++] = ti;


    pthread_mutex_unlock(&thread_table_mut);
}


static void remove_thread_info(tid_t tid) {
    int err = pthread_mutex_lock(&thread_table_mut);

    assert(!err);

    // sequential search
    // @todo replace with binary search
    for(int i = 0; i < thread_table_sz; i++) {
        struct thread_info* ti = thread_table[i];

        err = pthread_mutex_lock(&ti->mut);
        assert(!err);

        if(ti->tid == tid) {
            // found
            
            free_thread_info(ti);
            if(i != thread_table_sz - 1) {
                thread_table[i] = thread_table[thread_table_sz - 1];
            }
            thread_table_sz--;
            break;
        }

        pthread_mutex_unlock(&ti->mut);
    }

    pthread_mutex_unlock(&thread_table_mut);
}



struct wrapper_arg {
    void* pthread_arg;
    void* (*fun)(void*);
};

void pthread_exit(void* retval) {
    /////////////////////////////
    ////  save return value  ////
    /////////////////////////////

    tid_t tid = _get_tid();

    struct thread_info* info = acquire_thread_info(tid);

    if(!info) {
        printf("thread detached\n");
        // thread detached
    }
    else {
        //printf("pthread_cond_broadcast\n");
        pthread_cond_broadcast(&info->cond);
        info->ret = retval;
        info->done = 1;
        release_thread_info(info);
    }

    // actually exit the thread
    _thread_exit();
}



static void pthread_wrapper(void* arg) {
    struct wrapper_arg warg = *(struct wrapper_arg*)arg;
    free(arg);

    void* ret = warg.fun(warg.pthread_arg);

    pthread_exit(ret);
}


int pthread_create(pthread_t* __restrict newthread,
			   const pthread_attr_t* __restrict attr,
			   void *(*start_routine)(void *),
			   void *__restrict arg)
{   
    (void) attr;

    struct wrapper_arg* warg = malloc(sizeof(struct wrapper_arg));

    *warg = (struct wrapper_arg) {
        .fun = start_routine,
        .pthread_arg = arg,
    };

    int tid = _thread_create(pthread_wrapper, warg);


    if(tid == 0) {
        return EAGAIN;
    }

    struct thread_info* ti = malloc(sizeof(struct thread_info));
    *ti = (struct thread_info) {
        .tid = tid,
        .done = 0,
    };

    pthread_mutex_init(&ti->mut, NULL);
    pthread_cond_init(&ti->cond, NULL);

    insert_thread_info(ti);

    if(newthread) {
        *newthread = (pthread_t) {
            .tid = tid,
        };
    }

    return 0;
}


int pthread_join(pthread_t th, void** thread_return) {
    struct thread_info* ti = acquire_thread_info(th.tid);

    if(!ti) {
        return -1;
    }

    if(ti->joined) {
        pthread_mutex_unlock(&ti->mut);
        return EINVAL;
    }

    ti->joined = 1;

    while(! ti->done) 
        pthread_cond_wait(&ti->cond, &ti->mut);


    if(thread_return)
        *thread_return = ti->ret;

    pthread_mutex_unlock(&ti->mut);
    return 0;
}


pthread_t pthread_self(void) {
    return (pthread_t) {.tid = _get_tid()};
}