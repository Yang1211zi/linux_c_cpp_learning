#include<stdio.h>
#include"server.h"
#include <fcntl.h>      // open()、O_RDONLY
#include <sys/stat.h>   // struct stat、fstat()
#include <sys/types.h>  // 系统数据类型
#include <unistd.h>     // close()、read()等
#include <sys/sendfile.h>
#include <string.h>  // strerror()
#include <errno.h> 
int http_request(conn *c){
    char *request=c->rbuffer;
    printf("request:%s\n",request);
    c->sate=0;
    return 0;
}

int http_response(conn *c){
    #if 0 
    c->wlen=sprintf(c->wbuffer,"HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!");
    #endif

    #if 0
    int filefd=open("index.html",O_RDONLY);
    struct stat st;
    fstat(filefd,&st);
    c->wlen=sprintf(c->wbuffer,"HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n",st.st_size);
    close(filefd);
    int count=0;
    count=read(filefd,c->wbuffer+c->wlen,buffer_size-c->wlen);
    c->wlen+=count;

    #endif

    #if 1
    int filefd=open("index.html",O_RDONLY);
    struct stat st;
    fstat(filefd,&st);
    if(c->sate==0){
        c->wlen=sprintf(c->wbuffer,"HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n",st.st_size);
        c->sate=1;
    }
    else if(c->sate==1){
        int ret=0;
        ret=sendfile(c->fd,filefd,NULL,st.st_size);
        if(ret<0){
            printf("sendfile error:%s\n",strerror(errno));
        }
        memset(c->wbuffer,0,buffer_size);
        c->wlen=0;
        c->sate=2;
    }
    if(c->sate==2){
        memset(c->wbuffer,0,buffer_size);
        c->wlen=0;
    }
    close(filefd);
    #endif
    return 0;
}

