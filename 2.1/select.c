#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
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
    fd_set rfds,rset;
    FD_ZERO(&rfds);//初始化清空
    FD_SET(sockfd,&rfds);//将监听节点放入rfds集合中

    int maxfd=sockfd;
while (1) {//不加while那服务器只能与一个客户端连接
    rset=rfds;
    int nready=select(maxfd+1,&rset,NULL,NULL,NULL);
    if (nready==0) {
        continue;//超过超时时间但没有fd可读
    }
    if (nready < 0) {
    printf("select error: %s\n", strerror(errno));
}
    if (FD_ISSET(sockfd,&rset)) {
        int clientfd = accept(sockfd,(struct sockaddr*)&cli_addr,&len);
        FD_SET(clientfd,&rfds);
        if (clientfd>maxfd) maxfd=clientfd;
    }

    int i=0;
    for (i=sockfd+1;i<maxfd;i++) {
        if (FD_ISSET(i,&rset)) {
            char buffer[1024]={0};
            int count=recv(i,buffer,1024,0);
            printf("%s\n",buffer);
            count=send(i,buffer,1024,0);
            if (count==0) {
                close(i);
                FD_CLR(i,&rfds);
                continue;
            }
            printf("%d\n",count);
        }

    }
}
    getchar();
    return 0;
}