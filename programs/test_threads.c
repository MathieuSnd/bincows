#include <pthread.h>


#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>


int X = 0;

pthread_mutex_t mutex;
pthread_cond_t cond;



void* thread_entry(void* arg) {
    const char* print = arg;

    pthread_mutex_lock(&mutex);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);



    for(int i = 0; i < 1000; i++) {
        X++;
    }
    printf("%s\n", print);


    return print;
}


#define N_THREADS 500

int main() {


    pthread_t threads[N_THREADS];
    char* strings[N_THREADS];

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init (&cond,  NULL);

 
    for(int i = 0; i < N_THREADS; i++) {
        strings[i] = malloc(128); 
        sprintf(strings[i], "thread %u!", i);
        
        pthread_create(&threads[i], NULL, thread_entry, strings[i]);
    }

    usleep(1000*1000);

    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);


    for(int i = 0; i < N_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }


    printf("X = %u\n", X);

}
