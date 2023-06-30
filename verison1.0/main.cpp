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

#define MAX_FD 65535            // 最大连接数
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
        pool = new threadpool<http_conn>;       // 创建线程池对象
    } catch (...) {
        return 1;
    }

    http_conn* users = new http_conn[MAX_FD];       

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);

    int ret = 0;
    struct sockaddr_in address;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);


    // 设置端口复用，多个客户端复用同一个监听端口
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    ret = listen(listenfd, 5);

    // 创建epoll对象、事件数组
    epoll_event events[MAX_EVENT_NUM];
    int epollfd = epoll_create(5);

    // 添加监听描述符到epoll
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;     // 初始化类静态变量 m_epollfd

    while (1) {
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUM, -1);

        if (num < 0 && errno != EINTR) {
            printf("epoll_wait failed\n");
            break;
        }


        // 遍历
        for (int i = 0; i < num; ++i) {
            int sockfd = events[i].data.fd;

            // 有客户端连接
            if (sockfd == listenfd) {

                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);

                if (connfd < 0) {
                    printf("errno: %d\n", errno);
                    continue;
                }

                // 超出最大用户
                if (http_conn::m_user_count >= MAX_FD) {
                    close(connfd);
                    continue;
                }

                // 初始化连接
                users[connfd].init(connfd, client_address); 
            } 
            // 若该socket出现若干问题，内核无法继续从socket中读数据到缓冲区：
            // EPOLLRDHUP 表示对等方连接处于半关闭；EPOLLHUP表示对等放已经关闭连接；EPOLLERR表示socket中有错误。
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                users[sockfd].close_conn();
            }
            // EPOLLIN 表示内核已经将一些数据从缓冲区中读取到socket中，用户可以从socket中读取数据
            else if (events[i].events & EPOLLIN) {
                // 若从该socket中读取到数据
                if (users[sockfd].read()) {
                    // 将这个任务交给线程池的某个线程
                    pool->append(users + sockfd);
                } else {
                    users[sockfd].close_conn();
                }
            }
            // EPOLLOUT 表示内核已经把需要写入fd的数据准备好了
            else if (events[i].events & EPOLLOUT) {
                // 向socket中写入响应
                if (!users[sockfd].write()) {
                    users[sockfd].close_conn();
                }
            }
        }
    }

    // 收尾处理
    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;

    return 0;
}
