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
#define max_port 100

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

int is_listenfd(int fd,int*fds){
    int i=0;
    for(i=0;i<max_port;i++){
    if(fd==(fds[i])) 
    return -1;}
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc<2) return -1;
    int port = atoi(argv[1]);//起始端口
    int i=0;
    int sockfds[max_port]={0};
    int epfd=epoll_create(1);
    for(i=0;i<max_port;i++){
    int sockfd=socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port+i);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in))!=0) {
        perror("bind error");
        return -1;
    }
    if (listen(sockfd, 5)!=0) {
        perror("listen error");
        return -1;
    }
    struct epoll_event ev;//告诉epoll要监控哪个fd
    ev.events=EPOLLIN;//当 数据可读(EPOLLIN) 时通知我
    ev.data.fd=sockfd;//当 sockfd 数据可读(EPOLLIN) 时通知我
    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);
    sockfds[i]=sockfd;

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
    
    struct epoll_event events[epoll_size];//保存 epoll_wait() 返回的就绪事件结果
    
    

    while (1) {
        //结尾参数-> -1:监听列表里没有就不添加 0：一直检查有没有 2：2ticks检查一次
        int nready=epoll_wait(epfd, events, epoll_size, 5);
        if (nready==-1) continue;
        int i=0;
        for (i=0;i<nready;i++) {
            int sockfd=is_listenfd(events[i].data.fd,sockfds);
            if (sockfd!=0) {
                struct sockaddr_in cli_addr;
                memset(&cli_addr, 0, sizeof(struct sockaddr_in));
                socklen_t cli_addr_len = sizeof(cli_addr);
                int clientfd=accept(sockfd, (struct sockaddr*)&cli_addr, &cli_addr_len);
                //这里对ev的配置实际上是为clientfd返回数据的方式做准备，服务的是recv()这个函数
                struct epoll_event ev;
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
                    struct epoll_event ev;
                    ev.events=EPOLLIN;
                    ev.data.fd=clientfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, &ev);
                    close(clientfd);
                    
                    
                }
                else if (len==0) {
                    struct epoll_event ev;
                    ev.events=EPOLLIN;
                    ev.data.fd=clientfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, &ev);//将端口从epoll监听列表删除
                    close(clientfd);//关闭端口通道
                    
                    
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