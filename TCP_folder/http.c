#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/select.h>
#define buffer_size 4096
#define http_version "HTTP/1.1"
#define connection_type "Connection: close\r\n"
char* host_to_ip(const char*hostname) {
    if (hostname == NULL) {
        printf("hostname is NULL\n");
    }
    struct hostent* host = gethostbyname(hostname);
    if (host) {
        return inet_ntoa(*(struct in_addr*)host->h_addr_list[0]);
    }
}

int socket_create(char*ip) {
    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in sin={0};
    sin.sin_port=htons(80);
    sin.sin_family=AF_INET;
    sin.sin_addr.s_addr=inet_addr(ip);
    if (0!=connect(sockfd,(struct sockaddr*)&sin,sizeof(sin))) return -1;
    //在自己定义ipv4地址信息时，用struct sockaddr_in，但由于每个ip类型的结构体都不一样，比如ipv6变为
    //sockaddr_in6。对于一个统一的API函数，如connect(),sendto()等，不可能针对每一个类型都定义一个对应
    //的函数，因此对于统一的API函数，地址信息的结构体统一强转为struct sockaddr类型
    fcntl(sockfd,F_SETFL,O_NONBLOCK);//将socket端口设置成非阻塞
    return sockfd;
}

char* http_send_request(const char*hostname,const char*resource) {
    char*ip=host_to_ip(hostname);
    int socket= socket_create(ip);
    char buffer[buffer_size];
    sprintf(buffer,
        "GET %s %s\r\n"
        "Host: %s\r\n"
        "%s"
        "\r\n",
        resource,http_version,hostname,
        connection_type
        );
    send(socket,buffer,strlen(buffer),0);
    //下面准备进行接收，但由于端口被设置成非阻塞，如果端口中没有数据，那么接收函数收到的只有空值
    //因此设计一个select来判断端口中有没有数据来判断接收时机
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(socket,&readfds);
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    char*res=malloc(sizeof(int));
    memset(res,0,sizeof(int));
    while (1) {
        int selection=select(socket+1,&readfds,NULL,NULL,&timeout);
        if (!selection||!FD_ISSET(socket,&readfds)) {
            break;
        }
        else {
            memset(buffer,0,sizeof(buffer));
            int len=recv(socket,buffer,buffer_size,0);
            if (!len) {
                break;
            }
            res=realloc(res,(strlen(res)+len+1)*sizeof(char));
            strncat(res,buffer,len);
        }
    }
    return res;
}

int main(int argc, char* argv[]) {
    if (argc<3) return -1;
    char*temp=http_send_request(argv[1],argv[2]);
    printf("%s\n",temp);
    free(temp);
    return 0;
}