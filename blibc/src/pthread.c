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
    volatile int joined;
    int joinable;

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
    pthread_mutex_destroy(&ti->mut);
    pthread_cond_destroy(&ti->cond);
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

        release_thread_info(ti);
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
        // thread detached
    }
    else {
        if(!info->joinable)  {
            release_thread_info(info);
            remove_thread_info(tid);
        }
        else {
            info->ret = retval;
            info->done = 1;
            release_thread_info(info);
            pthread_cond_broadcast(&info->cond);
        }
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


int pthread_create(pthread_t* restrict newthread,
			   const pthread_attr_t* restrict attr,
			   void *(*start_routine)(void *),
			   void *restrict arg)
{   
    int joinable = 1;
    
    if(attr)
        joinable = attr->joinable;
    

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
        .joined = joinable ? 0 : 1,
        .joinable = joinable ? 1 : 0
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
        release_thread_info(ti);
        return EINVAL;
    }

    ti->joined = 1;

    while(! ti->done) {
        pthread_cond_wait(&ti->cond, &ti->mut);
    }


    if(thread_return)
        *thread_return = ti->ret;

    tid_t tid = ti->tid;

    release_thread_info(ti);

    remove_thread_info(tid);
    return 0;
}


int pthread_detach(pthread_t th) {
    struct thread_info* ti = acquire_thread_info(th.tid);

    if(!ti) {
        return ESRCH;
    }

    ti->joinable = 0;

    volatile int joined = ti->joined;
    release_thread_info(ti);

    return joined ? EINVAL : 0;
}



pthread_t pthread_self(void) {
    return (pthread_t) {.tid = _get_tid()};
}


int pthread_attr_getdetachstate(const pthread_attr_t* attr, int *detachstate)
{
    *detachstate = attr->joinable ? 
                        PTHREAD_CREATE_JOINABLE 
                      : PTHREAD_CREATE_DETACHED;
    return 0;
}


int pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate) 
{
    switch(detachstate) {
        case PTHREAD_CREATE_JOINABLE:
            attr->joinable = 1;
            break;
        case PTHREAD_CREATE_DETACHED:
            attr->joinable = 0;
            break;
        default: return EINVAL;
    }
    return 0;
}

int pthread_attr_getguardsize(const pthread_attr_t* attr, size_t* guardsize)
{
    (void) attr;
    *guardsize = 0x1000;
    return 0;
}

int pthread_attr_setguardsize(pthread_attr_t* attr, size_t guardsize) {
    (void) attr;
    return guardsize == 0x1000 ? 0 : -1;
}



/* Return in *PARAM the scheduling parameters of *ATTR.  */
int pthread_attr_getschedparam(const pthread_attr_t* restrict attr,
				       struct sched_param *restrict param
) {
    (void) attr;
    param->sched_priority = 0;
    return 0;
} 

/* Set scheduling parameters(priority, etc) in *ATTR according to PARAM.  */
int pthread_attr_setschedparam(pthread_attr_t* restrict attr,
				       const struct sched_param *restrict param)
{
    (void) attr;
    if(param->sched_priority == 0)
        return 0;
    return -1;
}                       


int pthread_attr_getschedpolicy(const pthread_attr_t* restrict
					attr, int *restrict policy)
{
    (void) attr;
    *policy = SCHED_RR;
    return 0;
}


/* Set scheduling policy in *ATTR according to POLICY.  */
int pthread_attr_setschedpolicy(pthread_attr_t* attr, int policy)
{
    (void) attr;
    if(policy == SCHED_RR)
        return 0;
    return  -1;
}


/* Return in *INHERIT the scheduling inheritance mode of *ATTR.  */
int pthread_attr_getinheritsched(const pthread_attr_t* restrict
					 attr, int *restrict inherit)
{
    (void) attr;
    *inherit = PTHREAD_INHERIT_SCHED;
    return 0;
}

/* Set scheduling inheritance mode in *ATTR according to INHERIT.  */
int pthread_attr_setinheritsched(pthread_attr_t* attr,
					 int inherit) 
{
    (void) attr;
    (void) inherit;
    return 0;
}


/* Return in *SCOPE the scheduling contention scope of *ATTR.  */
int pthread_attr_getscope(const pthread_attr_t* restrict attr,
				  int *restrict scope)
{
    (void) attr;
    *scope = PTHREAD_SCOPE_SYSTEM;
    return 0;
}


int pthread_attr_setscope(pthread_attr_t* attr, int scope)
{
    (void) attr;
    if(scope == PTHREAD_SCOPE_SYSTEM)
        return 0;
    return -1;
}


int pthread_attr_getstacksize(const pthread_attr_t* restrict
				      attr, size_t* restrict stacksize)
{
    (void) attr;
    *stacksize = PTHREAD_STACK_MIN;
    return 0;
}


int pthread_attr_setstacksize(pthread_attr_t* attr,
				      size_t stacksize)
{
    (void) attr;
    if(stacksize == PTHREAD_STACK_MIN)
        return 0;
    return -1;
}
