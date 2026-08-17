//
// epoll 解决“哪些 fd 就绪了”，Reactor 解决“fd 就绪之后，该调用谁、怎么处理、怎么组织代码”
//在业务逻辑比较复杂的情况下，如果只用epoll那么所有代码都会堆在主循环中，很臃肿，可读性很差，维护很费劲
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include"server.h"
/*
#define buffer_size 1024
#define PORT 8080
#define conn_size 1048576
#define port_max 20

int efd=0;

typedef int(*RCALLBACK)(int fd);//使用typedef创建一个新类型，这个类型是返回值为int，参数为一个int的函数指针类型


/*
区分一下指针的指向与函数的调用
conn_list[sockfd].r_action.recv_callback=accept_cb; 指向，给地址
conn_list[connfd].r_action.recv_callback(connfd);  先给地址再给参数调用
*/
int recv_cb(int fd);
int send_cb(int fd);
int accept_cb(int fd);
int efd=0;
/*
typedef struct {
    int fd;
    char rbuffer[buffer_size];
    int rlen;
    char wbuffer[buffer_size];
    int wlen;
    union {
        RCALLBACK accept_callback;
        RCALLBACK recv_callback;
    }r_action;
    RCALLBACK send_callback;
}conn;
*/

conn conn_list[conn_size]={0};

int server_init(int port) {
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in servaddr;
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(port);
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    if (-1==bind(sockfd,(struct sockaddr*)&servaddr,sizeof(struct sockaddr))) {
        printf("bind error:%s\n",strerror(errno));
        close(sockfd);
    }
    listen(sockfd,128);
    return sockfd;
}

int set_event(int fd,int event,int flag) {
    if (flag==1) {
        struct epoll_event ev;
        ev.events=event;
        ev.data.fd=fd;
        epoll_ctl(efd,EPOLL_CTL_ADD,fd,&ev);
    }
    else {
        struct epoll_event ev;
        ev.events=event;
        ev.data.fd=fd;
        epoll_ctl(efd,EPOLL_CTL_MOD,fd,&ev);
    }
    return 0;
}

int recv_cb(int fd) {
    int count=recv(fd,conn_list[fd].rbuffer,buffer_size,0);
    if (count<=0) {
        close(fd); 
        epoll_ctl(efd,EPOLL_CTL_DEL,fd,NULL);
        return 0;
    }
    else if(count==0){//disconnect
        epoll_ctl(efd,EPOLL_CTL_DEL,fd,NULL);
        close(fd);
        return 0;
    }
    else if(count<0){
        printf("recv error:%s\n",strerror(errno));
        close(fd); 
        epoll_ctl(efd,EPOLL_CTL_DEL,fd,NULL);
        return -1;
    }
    conn_list[fd].rlen=count;
    conn_list[fd].wlen=conn_list[fd].rlen;
    /*
    memcpy(conn_list[fd].wbuffer,conn_list[fd].rbuffer,conn_list[fd].wlen);
    */
    set_event(fd,EPOLLOUT,0);
    http_request(&conn_list[fd]);
    return count;
}

int send_cb(int fd) {
    http_response(&conn_list[fd]);
    if(conn_list[fd].sate==1){
    int count=send(fd,conn_list[fd].wbuffer,conn_list[fd].wlen,0);
    set_event(fd,EPOLLOUT,0);
    }
    else if(conn_list[fd].sate==2){
        set_event(fd,EPOLLIN,0);
    }
    else if(conn_list[fd].sate==0){
        set_event(fd,EPOLLOUT,0);
    }
    return 0;
}

int accept_cb(int fd) {
    struct sockaddr_in cli_addr;
    socklen_t len=sizeof(cli_addr);
    int clientfd = accept(fd,(struct sockaddr*)&cli_addr,&len);
    if(clientfd<0)  return -1;
    conn_list[clientfd].fd=clientfd;
    conn_list[clientfd].r_action.recv_callback=recv_cb;
    conn_list[clientfd].send_callback=send_cb;
        //为新的连接做初始化参数
    memset(conn_list[clientfd].rbuffer,0,buffer_size);
    conn_list[clientfd].rlen=0;
    memset(conn_list[clientfd].wbuffer,0,buffer_size);
    conn_list[clientfd].wlen=0;
    set_event(clientfd,EPOLLIN,1);
    return clientfd;
}

int main() {
    efd=epoll_create(1);
    int i=0;
    for(i=0;i<port_max;i++){
    int sockfd=server_init(PORT+i);
    conn_list[sockfd].fd=sockfd;
    conn_list[sockfd].r_action.recv_callback=accept_cb;
    set_event(sockfd,EPOLLIN,1);}

    while(1) {
        struct epoll_event event[1024]={0};

        int nready=epoll_wait(efd,event,1024,-1);
        int i=0;
        for (i=0;i<nready;i++) {
            int connfd=event[i].data.fd;
            if (event[i].events & EPOLLIN) {
                conn_list[connfd].r_action.recv_callback(connfd);
            }
            if (event[i].events & EPOLLOUT) {
                conn_list[connfd].send_callback(connfd);
            }
        }
    }
    return 0;
}

/*
设置非阻塞
accept要对fd进行检查
对于没有返回值的要加返回值
*/