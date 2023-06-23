#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

#define BUFF_SIZE 1024
#define MAXLINE 80
#define SERV_PORT 8000
#define OPEN_MAX 1024
#define CLI_NUM 3

int client_fds[OPEN_MAX];

int main() {

    // create socket
    int lfd = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in saddr;
    saddr.sin_port = htons(8000);
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_family = AF_INET;

    // bind socket
    bind(lfd, (struct sockaddr *)&saddr, sizeof(saddr));

    // listen
    listen(lfd, 8);

    // 创建epoll实例
    int epfd = epoll_create(100);

    // 添加lfd到epoll实例
    struct epoll_event epev;
    epev.data.fd = lfd;
    epev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &epev);

    struct epoll_event epevs[1024];

    while (1) {

        // 查询epoll
        int ret = epoll_wait(epfd, epevs, 1024, -1);
        if (ret == -1) {
            perror("epoll_wait");
            exit(-1);
        }

        printf("ret = %d\n", ret);


        // 遍历epoll返回的描述符
        for (int i = 0; i < ret; i++) {
            int curfd = epevs[i].data.fd;

            // epoll监听的lfd有数据到达，有客户端连接
            if (curfd == lfd) {
                struct sockaddr_in cliaddr;
                socklen_t len = sizeof(cliaddr);
                int cfd = accept(lfd, (struct sockaddr*)&cliaddr, &len);

                // 将客户端描述符也加入epoll
                epev.events = EPOLLIN;
                epev.data.fd = cfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &epev);
            }
            else {
                if (epevs[i].events & EPOLLOUT) {
                    continue;
                }
                char buf[1024] = {0};
                int len = read(curfd, buf, sizeof(buf));
                if (len == -1) {
                    perror("read");
                    exit(-1);
                } else if (len == 0) {
                    printf("client close...\n");
                    epoll_ctl(epfd, EPOLL_CTL_DEL, curfd, NULL);
                    close(curfd);
                } else if (len > 0) {
                    printf("read buf = %s\n", buf);
                    write(curfd, buf, strlen(buf) + 1);
                }
            }
        }

    }

    close(lfd);
    close(epfd);
    return 0;
}