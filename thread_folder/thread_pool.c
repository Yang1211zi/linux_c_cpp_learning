#define _GNU_SOURCE
#include<stdio.h>
#include<string.h>
#include <stdlib.h>
#include <pthread.h>

#define List_insert(item, list)do{\
   item->next=list;\
   item->prev=NULL;\
   if(list!=NULL){\
   list->prev=item;\
}\
list=item;\
}while(0);

#define List_delete(item,list)do{\
    if(item->next != NULL){\
    item->next->prev=item->prev;}\
    if(item->prev!=NULL){\
    item->prev->next=item->next;}\
    else if(item->prev==NULL){\
    list=item->next;}\
}while(0);


typedef struct nTask {
    void (*task_func)(struct nTask*arg);//(*task_func)为函数指针，指向线程池里众多的执行函数之一
    void* task_arg;//需要传入执行函数中的参数。与线程有关的参数多为void/void*类型的，是因为执行函数
    //较多，每个函数的需要的参数值类型不尽相同，void可以实现任何类型的强转，方便在函数中根据实际情况转换

    struct nTask* next;
    struct nTask* prev;
}task;

typedef struct nWorker {
    pthread_t thread_id;
    int stop;
    struct nWorker* next;
    struct nWorker* prev;
    struct nManager* manager;//员工也需要知道经理的联系方式
}worker;

typedef struct nManager {
    task* task_list;
    worker* worker_list;//保存worker中的头指针

    pthread_mutex_t mutex;
    pthread_cond_t cond;//条件变量，当没有任务时，执行者需要一直等待

}threadpool;

static void* nthreadpool_callback(void* arg) {
    worker*w=(worker*)arg;
    while (1) {
        pthread_mutex_lock(&w->manager->mutex);
        while (w->manager->task_list == NULL) {//cond
            if (w->stop == 1) break;
            pthread_cond_wait(&w->manager->cond, &w->manager->mutex);
        }
        if (w->stop == 1) {
            pthread_mutex_unlock(&w->manager->mutex);
            break;
        }
        task*t=w->manager->task_list;
        List_delete(t,w->manager->task_list);
        pthread_mutex_unlock(&w->manager->mutex);
        t->task_func(t);
    }
    return NULL;
}

int nthreadpool_creat(threadpool*pool,int workernum) {
    if (pool == NULL) {
        printf("pool is NULL\n");
        return -1;
    }
    if (workernum<1) {
        printf("workernum is less than 1\n");
        workernum = 1;
    }
    memset(pool,0,sizeof(threadpool));
    pthread_cond_t cond_blank=PTHREAD_COND_INITIALIZER;
    memcpy(&pool->cond,&cond_blank,sizeof(pthread_cond_t));
    pthread_mutex_init(&pool->mutex,NULL);
    int i=0;
    for (i=0;i<workernum;i++) {
        worker*workers=malloc(sizeof(worker));
        if (workers==NULL) {
            printf("workers is NULL\n");
            return -1;
        }
        memset(workers,0,sizeof(workers));
        workers->manager=pool;
        List_insert(workers,pool->worker_list);
        int ret=pthread_create(&workers->thread_id,NULL,nthreadpool_callback,workers);
        //线程创建函数成功返回0，不成功返回非0
        if (ret) {
            printf("pthread_creat failed\n");
            return -1;
        }
    }
    return 0;
}

int nthreadpool_delete(threadpool*pool,int rworkernum) {
    if (pool == NULL) {
        printf("pool is NULL\n");
        return -1;
    }
    worker*w=NULL;
    for (w=pool->worker_list;w!=NULL;w=w->next) {
        w->stop=1;
    }
    pthread_mutex_lock(&pool->mutex);
    pthread_cond_broadcast(&pool->cond);//广播是为了唤醒回调函数中pthread_cond_wait里等待的
    //worker,因为在wait中的线程不会检查自身的标志位，也就是说即使自身的stop已经为1了，它也不知道
    pthread_mutex_unlock(&pool->mutex);
/*这里加的锁与回调函数中是一致的，目的是防止丢失唤醒。如果一个线程没有收到任务，正准备进入等待，此时销毁执行
所有worker的stop至1并进行广播，但刚才那个线程还没有进入等待，因此它stop至1，在广播结束后它又进入了等待
导致它虽然stop为1，但收不到广播不知道自己的stop为1自然不会被销毁，而刚才销毁的广播已经过去，它再也收不到
广播，也再也无法被销毁了。加了锁之后线程收到广播和进入while (w->manager->task_list == NULL)判断
就不可能同时发生了，刚才的情况可以被避免*/
w=pool->worker_list;
    while (w!=NULL) {
        pthread_join(w->thread_id,NULL);
        worker*tmp=w;
        free(w);
        w=tmp->next;}
        pool->worker_list=NULL;
    return 0;
}

int nthreadpool_push(threadpool*pool,task* task) {
    //将需要执行的任务放到任务队列中并通知worker来取
    if (pool == NULL) {
        printf("pool is NULL\n");
        return -1;
    }
    pthread_mutex_lock(&pool->mutex);
    List_insert(task,pool->task_list)
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

#if 1
#define thread_num 20
#define task_num 100

void task_callback(task*arg){
    task*t=arg;
    printf("task %d is running\n",*(int*)t->task_arg);
    free(t->task_arg);
    free(t);
}

int main() {
    threadpool pool;
    nthreadpool_creat(&pool,thread_num);
    int i=0;
    for(i=0;i<task_num;i++){
        task*ta=malloc(sizeof(task));
        if(ta==NULL){
            perror("malloc");
            return -1;
        }
        memset(ta,0,sizeof(task));
        ta->task_func=task_callback;
        ta->task_arg=malloc(sizeof(int));
        *(int*)(ta->task_arg)=i;
        nthreadpool_push(&pool,ta);
    }
    getchar();
    return 0;
}
#endif