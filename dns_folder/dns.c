#include<stdio.h>
#include<string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define dns_server_port 53
#define dns_server_ip "114.114.114.114"
//每个数据在结构图中有对应的位数通过位数可以知道字节数
//从而判断它的数据类型
typedef struct dns_head{
    unsigned short id;//0-65535之间的数，作用就是
    //DNS 服务器返回响应时，会把同一个 id 原样带回来
    unsigned short flags;
    unsigned short qdcount;
    unsigned short adcount;
    unsigned short nscount;
    unsigned short arcount;
}head;

typedef struct dns_question{
    unsigned char*name;//域名的名字的长度是不确定的，可长可短，
    //只能在结构体中记录地址
    int length;
    unsigned short qtype;
    unsigned short qclass;
}question;

int header_creater(head*head){
    if(head==NULL){
        return -1;
    }
    memset(head,0,sizeof(head));
    srandom(time(NULL));
    head->id=random();

    head->flags=htons(0x0100);
    head->qdcount=htons(1);
    return 0;
}

int question_create(const char*hostname,question*q){
    if(hostname==NULL||q==NULL){
        printf("question_create()\n");
        return -1;
    }
    q->name=malloc(strlen(hostname)+2);
    memset(q->name,0,strlen(hostname)+2);
    if(q->name==NULL) return -2;
    q->length=strlen(hostname)+2;
    q->qtype=htons(1);
    q->qclass=htons(1);

    const char dev[2]=".";
    char*qname=q->name;
    char*hostname_dup=strdup(hostname);//hostname为const类型
    if(hostname_dup==NULL){
        free(hostname_dup);
        free(q->name);
        q->name=NULL;
        return -3;
    }
    char*temp=strtok(hostname_dup,dev);

    while(temp!=NULL){
        size_t len=strlen(temp);
        *qname=len;
        qname++;
        strncpy(qname,temp,len+1);
        qname+=len;
        temp=strtok(NULL,dev);
    }
    free(hostname_dup);
}

int build_dns(head*h,question*q,char*request,int len){
    if(h==NULL||q==NULL||request==NULL) return -1;
    int offset=0;
    memcpy(request,h,sizeof(head));
    offset+=sizeof(head);
    memcpy(request+offset,q->name,sizeof(question));
    offset+=q->length;
    memcpy(request+offset,&q->qtype,sizeof(q->qtype));
    offset+=sizeof(q->qtype);
    memcpy(request+offset,&q->qclass,sizeof(q->qclass));
    offset+=sizeof(q->qclass);
    return offset;
}

int dns_udp(char*domain){
    if(domain==NULL) return-1;
    int sockfd=socket(AF_INET,SOCK_DGRAM,0);
    if(socket<1)return -2;
    struct sockaddr_in servaddr={0};
    servaddr.sin_addr.s_addr=inet_addr(dns_server_ip);
    servaddr.sin_port=htons(dns_server_port);
    servaddr.sin_family=AF_INET;
    connect(sockfd,(struct servaddr*)&servaddr,sizeof(servaddr));
//由于网络通信比较复杂，第一次发送数据丢失的可能性很大，因此在发送之前先使用connect为sendto开辟一次通信道路
    head h={0};
    header_creater(&h);
    question q={0};
    question_create(&q,domain);
    char request[1024]={0};
    int length=build_dns(&h,&q,request,1024);
    sendto(sockfd,request,length,0,(struct sockaddr*)&servaddr,sizeof( servaddr));
    //将消息从你的计算器向DNS服务器发送消息请求进行域名解析，参数设置中的地址与端口号与提供DNS查询服务的
    //那台服务器的 IP 地址，端口号一致。DNS的端口号与ip地址一般不在你自己操作的计算机上
    char response[1024]={0};
    struct sockaddr_in addr;
    socklen_t addr_ien=sizeof(struct sockaddr_in);
    int n=recvfrom(sockfd,response,sizeof(response),0,(struct sockaddr*)&addr,(socklen_t*)&addr_ien);
    printf("revfrom:%d,%s\n",n,response);
}

int main(int argc,char*argv[]){
    if(argc<2)return -1;
    dns_udp(argv[1]);
    return 0;
}