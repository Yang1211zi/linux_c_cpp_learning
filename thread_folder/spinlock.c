#define _GNU_SOURCE
#include<stdio.h>
#include<pthread.h>
#include<unistd.h>

#define thread_id_length 10

pthread_spinlock_t spinlock;


void*thread_callback(void*arg) {
    int*pcount=(int*)arg;
    int i=0;
    while(i++<100000) {
       pthread_spin_lock(&spinlock);
        (*pcount)++;
        //printf("%d\n",*pcount);
       pthread_spin_unlock(&spinlock);
         usleep(1);
    }
}

int main() {
    pthread_t thread_id[thread_id_length]={0};
    pthread_spin_init(&spinlock,PTHREAD_PROCESS_SHARED);
    int count=0;
    int i=0;
    for (i=0;i<thread_id_length;i++) {
        pthread_create(&thread_id[i], NULL, thread_callback, &count);
    }
    for (i=0;i<100;i++) {
        printf("%d\n",count);
        sleep(1);
    }
    return 0;
}