#ifndef SERVER_H
#define SERVER_H
#define buffer_size 1024
#define PORT 8080
#define conn_size 1048576
#define port_max 20

extern int efd;

typedef int(*RCALLBACK)(int fd);//使用typedef创建一个新类型，这个类型是返回值为int，参数为一个int的函数指针类型

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
    int sate;
}conn;

extern int http_request(conn *c);
extern int http_response(conn *c);



#endif // SERVER_H