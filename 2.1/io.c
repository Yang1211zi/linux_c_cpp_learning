#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
void*thread_func(void* arg) {
    int clientfd = *(int*)arg;
    char buffer[1024]={0};
    while (1) {//不加while，对应的客户端只能发送接收一次数据，线程就退出了，没法实现客户端与服务器的多次通信
        int count=recv(clientfd,buffer,1024,0);
        if (count==0) {
            printf("client closed\n");
            break;
        }
        printf("%s\n",buffer);
        count=send(clientfd,buffer,1024,0);
        printf("%d\n",count);
    }
    close(clientfd);
    return NULL;
}

int main() {
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in servaddr;
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(2000);
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    if (-1==bind(sockfd,(struct sockaddr*)&servaddr,sizeof(struct sockaddr))) {
        printf("bind error:%s\n",strerror(errno));
    }
    listen(sockfd,128);
    //backlog为服务器已经调用 listen() 后，内核为“等待被 accept 的连接”准备的排队长度
    //监听fd建立之后，新的客户端建立连接后不是马上通过accept()接收，而是先进入内核队列
    //而backlog就是限制 已经建立连接，但是还没有被accept取走的连接数量


    struct sockaddr_in cli_addr;
    socklen_t len=sizeof(cli_addr);
#if 0
    //只能和一个客户端建立连接
    int clientfd = accept(sockfd,(struct sockaddr*)&cli_addr,&len);
    char buffer[1024]={0};
    int count=recv(clientfd,buffer,1024,0);
    printf("%s\n",buffer);
    count=send(clientfd,buffer,1024,0);
    printf("%d\n",count);
#endif

#if 0
    //只能一次性处理一个客户端的完整收发过程，不能同时处理多个客户端
    while (1) {
        int clientfd = accept(sockfd,(struct sockaddr*)&cli_addr,&len);
        char buffer[1024]={0};
        int count=recv(clientfd,buffer,1024,0);
        printf("%s\n",buffer);
        count=send(clientfd,buffer,1024,0);
        printf("%d\n",count);
    }
#endif
while (1) {//不加while那服务器只能与一个客户端连接
    int clientfd = accept(sockfd,(struct sockaddr*)&cli_addr,&len);
    char buffer[1024]={0};
    pthread_t id;
    pthread_create(&id,NULL,thread_func,&clientfd);
}
    getchar();
    return 0;
}