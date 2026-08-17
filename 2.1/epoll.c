#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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

    int efd=epoll_create(1);
    struct epoll_event ev;
    ev.events=EPOLLIN;
    ev.data.fd=sockfd;
    struct epoll_event event[1024]={0};
    epoll_ctl(efd,EPOLL_CTL_ADD,sockfd,&ev);
while (1) {
    //不加while那服务器只能与一个客户端连接
    int nready=epoll_wait(efd,event,1024,-1);
    int i=0;
    for (i=0;i<nready;i++) {
        if (event[i].data.fd==sockfd) {
            int clientfd = accept(sockfd,(struct sockaddr*)&cli_addr,&len);
            ev.events=EPOLLIN;
            ev.data.fd=clientfd;
            epoll_ctl(efd,EPOLL_CTL_ADD,clientfd,&ev);
        }
        else if (event[i].events & EPOLLIN) {
            char buffer[1024]={0};
            int count=recv(event[i].data.fd,buffer,1024,0);
            if (count==0) {
                close(event[i].data.fd);
                epoll_ctl(efd,EPOLL_CTL_DEL,event[i].data.fd,&ev);
                continue;
            }
            printf("%s\n",buffer);
            count=send(event[i].data.fd,buffer,1024,0);
        }
    }

}
    getchar();
    return 0;
}