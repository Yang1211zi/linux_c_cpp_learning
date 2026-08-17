#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

#define buffer_Size 1024
#define epoll_size 1024
//一个进程最多同时管理1024个内核资源句柄（文件描述符）包括普通文件，socket网络连接，pipe管道，终端，设备文件等。
//其中一个TCP连接占一个fd
void*cli_ron(void* arg) {
    int clientfd=*(int*)arg;
    free(arg);
    while (1) {
        char buffer[buffer_Size]={0};
        int len=recv(clientfd, buffer, buffer_Size, 0);
        if (len<0) {
        close(clientfd);
            break;
        }
        else if (len==0) {
            close(clientfd);
            break;
        }
        else {
            printf("data:%s,len:%d\n",buffer,len);
        }

    }


}

int main(int argc, char* argv[]) {
    if (argc<2) return -1;
    int port = atoi(argv[1]);//char* -> int
    int sockfd=socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;//服务器不知道客户端会通过哪个IP找它，所以监听所有本机IP
    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in))!=0) {
        perror("bind error");
        return -1;
    }//服务器侧都需要bind，它需要一个固定的地址让别人找到；客户端侧则只需要临时找一个出口访问别人，不用bind
    if (listen(sockfd, 5)!=0) {
        perror("listen error");
        return -1;
    }
    #if 0
    while (1) {
        struct sockaddr_in cli_addr;
        memset(&cli_addr, 0, sizeof(struct sockaddr_in));
        socklen_t cli_addr_len = sizeof(&cli_addr);
        int*cli_apt=malloc(sizeof(int));
         *cli_apt=accept(sockfd, (struct sockaddr*)&cli_addr, &cli_addr_len);
        //listen() 之后，服务器进入等待状态；accept() 负责从等待队列中取出一个客户端连接，
        //并创建一个新的通信 socket。
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, cli_ron, cli_apt);
    }
    #endif
    int epfd=epoll_create(1);
    struct epoll_event events[epoll_size];//保存 epoll_wait() 返回的就绪事件结果
    struct epoll_event ev;//向 epoll 注册/修改事件的配置结构体。相当于每个具体io口的配置工具，用ev将这个端口的信息配置好之后通过epoll_ctl()传入&ev实现配置
    ev.events=EPOLLIN;//当 数据可读(EPOLLIN) 时通知我
    ev.data.fd=sockfd;//当 sockfd 数据可读(EPOLLIN) 时通知我
    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

    while (1) {
        //结尾参数-> -1:监听列表里没有就不添加 0：一直检查有没有 2：2ticks检查一次
        int nready=epoll_wait(epfd, events, epoll_size, 5);
        if (nready==-1) continue;
        int i=0;
        for (i=0;i<nready;i++) {
            if (events[i].data.fd==sockfd) {
                struct sockaddr_in cli_addr;
                memset(&cli_addr, 0, sizeof(struct sockaddr_in));
                socklen_t cli_addr_len = sizeof(&cli_addr);
                int clientfd=accept(sockfd, (struct sockaddr*)&cli_addr, &cli_addr_len);
                //这里对ev的配置实际上是为clientfd返回数据的方式做准备，服务的是recv()这个函数
                ev.events=EPOLLIN | EPOLLET;
                //水平触发(默认状态):只要有数据就触发
                //边沿触发(EPOLLET):从有数据->没数据 or 没数据->有数据，有这种状态的转变才触发
                //假如说缓冲区小一次性没读完，如果采用边沿触发，可能读完一次之后，状态未发生跳变，就不会再读剩下的数据了
                ev.data.fd=clientfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);
            }
            else {
                int clientfd=events[i].data.fd;
                char buffer[buffer_Size]={0};
                int len=recv(clientfd, buffer, buffer_Size, 0);
                if (len<0) {
                    close(clientfd);
                    ev.events=EPOLLIN;
                    ev.data.fd=clientfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, &ev);
                }
                else if (len==0) {
                    close(clientfd);//关闭端口通道
                    ev.events=EPOLLIN;
                    ev.data.fd=clientfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, &ev);//将端口从epoll监听列表删除
                }
                else {
                    printf("data:%s,len:%d\n",buffer,len);
                }

            }
            }
        }
        return 0;
    }

    


//一请求一线程的方式访问服务器，在当前百万，亿级别的客户端需要访问的情况下已经被弃用了
//线程的创建成本太高，一个线程8M，那么1G也才只能存128个线程