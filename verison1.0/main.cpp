#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdio>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 65535
#define MAX_EVENT_NUM 10000

extern void addfd(int epollfd, int fd, bool oneshot);
extern void delfd(int epollfd, int fd);

void addsig(int sig, void (handler)(int)) {
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = handler;
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
}


int main(int argc, char** argv) {
    if (argc <= 1) {
        printf("usage: %s port_num\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    // 当一个进程向一个已经关闭写入端的管道（或套接字）写入数据时，
    // 操作系统会向该进程发送SIGPIPE信号。如果该进程没有处理该信号，
    // 则默认情况下会导致进程被终止。SIG_IGN 忽略对该信号的处理
    addsig(SIGPIPE, SIG_IGN);

    threadpool<http_conn>* pool = NULL;
    try {
        pool = new threadpool<http_conn>;
    } catch (...) {
        return 1;
    }

    http_conn* user = new http_conn[MAX_FD];

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);

    int ret = 0;
    struct sockaddr_in address;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    ret = listen(listenfd, 5);

    epoll_event events[MAX_EVENT_NUM];
    int epollfd = epoll_create(5);

    // 添加监听描述符到epoll
    addfd(epollfd, listenfd, false);
    




}
