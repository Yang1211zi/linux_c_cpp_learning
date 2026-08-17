#define _GNU_SOURCE
#include <stdio.h>
#include<string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <ucontext.h>

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

typedef void(*coroutine_func)(void*);

typedef struct coroutine{
    int id;
    ucontext_t ctx;
    coroutine_func coro_func;
    void* arg;
    void* stack;
    size_t stack_size;
}coro;

typedef struct scheduler{
    coro* coroutines;
    int count_coroutines;
    ucontext_t main_ctx;
    coro*ready_head;

}scheduler;

typedef ssize_t (*read_t)(int fd, void *buf, size_t count);
read_t read_f = NULL;

typedef ssize_t (*write_t)(int fd, const void *buf, size_t count);
write_t write_f = NULL;

ssize_t read(int fd, void *buf, size_t count){
    struct pollfd pfd[1];
    pfd[0].fd = fd;
    pfd[0].events = POLLIN;
    int ret = poll(pfd, 1, 0);
    if(ret<0){
        swapcontext();
    }
    ret=read_f(fd, buf, count);
    printf("ret:%d,buf:%s\n",ret,(char*)buf);
    return ret;
}

ssize_t write(int fd, const void *buf, size_t count){
    struct pollfd pfd[1];
    pfd[0].fd = fd;
    pfd[0].events = POLLOUT;
    int ret = poll(pfd, 1, 0);
    if(ret<0){
        swapcontext();
    }
    ret=write_f(fd, buf, count);
    printf("ret:%d,buf:%s\n",ret,(char*)buf);
    return write_f(fd, buf, count);
}

void hook_init(){
    if(!read_f){
        read_f=(read_t)dlsym(RTLD_NEXT, "read");//dlsym->根据函数名字，在动态链接库里查找函数地址
    }
    if(!write_f){
        write_f=(write_t)dlsym(RTLD_NEXT, "write");
    }
}

int server_init(){
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in servaddr;
    memset(&servaddr,0,sizeof(struct sockaddr_in));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(2000);
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    if (-1==bind(sockfd,(struct sockaddr*)&servaddr,sizeof(struct sockaddr))) {
        printf("bind error:%s\n",strerror(errno));}
    listen(sockfd, 10);
    return sockfd;
}

int coro_func(){
    return 0;
}


int main(){
    hook_init();
    int sockfds = server_init();
    while(1){
      struct sockaddr_in clientaddr;
	  socklen_t len = sizeof(clientaddr);
	  int clientfd = accept(sockfds, (struct sockaddr*)&clientaddr, &len);
	  printf("accept\n");
    while(1){
        char buffer[1024]={0};
        int count=read(clientfd, buffer, 1024);
        if(count==0){
            close(clientfd);
            break;
        }
        write(clientfd, buffer, count);
        printf("sockfd: %d, clientfd: %d, count: %d, buffer: %s\n", sockfds, clientfd, count, buffer);
    }
}
    return 0;
}