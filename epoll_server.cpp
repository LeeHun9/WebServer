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
//https://developer.aliyun.com/article/398525
int client_fds[OPEN_MAX];

int main() {
    int ser_souck_fd, nfds;
    int i;  
    char input_message[BUFF_SIZE];
    char resv_message[BUFF_SIZE];
 
 
    struct sockaddr_in ser_addr;
    ser_addr.sin_family= AF_INET;    //IPV4
    ser_addr.sin_port = htons(SERV_PORT);
    ser_addr.sin_addr.s_addr = INADDR_ANY;  //指定的是所有地址

    struct epoll_event ev, events[20];

    //fd_set
    int efd = epoll_create(OPEN_MAX);
    if (efd == -1) {
        perror("epoll_create failure");
        return -1;
    }


    //creat socket
    if( (ser_souck_fd = socket(AF_INET,SOCK_STREAM,0)) < 0 )
    {
        perror("creat failure");
        return -1;
    }

    //bind soucket
    if(bind(ser_souck_fd, (const struct sockaddr *)&ser_addr,sizeof(ser_addr)) < 0)
    {
        perror("bind failure");
        return -1;
    }

    //listen
    if(listen(ser_souck_fd, 20) < 0)
    {
        perror("listen failure");
        return -1;
    }

    ev.data.fd = ser_souck_fd;
    ev.events = EPOLLIN|EPOLLET;
    epoll_ctl(efd, EPOLL_CTL_ADD, ser_souck_fd, &ev);

    int max_fd=1;
    struct timeval mytime;
    printf("wait for client connnect!\n");

    while (1) {
        nfds = epoll_wait(efd, events, 20, 0);

        for (i = 0; i < nfds; i++) {

            // ser_souck_fd监听到有新用户连接
            if (events[i].data.fd == ser_souck_fd) {
                struct sockaddr_in client_address;
                socklen_t address_len;
                int client_sock_fd = accept(ser_souck_fd, (struct sockaddr *)&client_address, &address_len);
                if (client_sock_fd < 0) {
                    perror("accept failure");
                    return -1;
                }
                
                // 打印客户端信息
                char* str = inet_ntoa(client_address.sin_addr);
                printf("accept connection from %s\n", str);

                // 注册EPOLL事件
                ev.data.fd = client_sock_fd;
                ev.events = EPOLLIN|EPOLLET;
                epoll_ctl(efd, EPOLL_CTL_ADD, client_sock_fd, &ev);
                
            }
            // 该用户已已连接
            else if (events[i].events & EPOLLIN) {
                int client_fd = events[i].data.fd;
                if (client_fd < 0) 
                    continue;
                int nread;
                if ((nread = read(events[i].data.fd, resv_message, BUFF_SIZE)) < 0) {
                    if (errno == ECONNRESET) {
                        close(client_fd);
                        events[i].data.fd = -1;
                    }
                    else printf("rescessed error!");
                }
                else if (nread > 0) {
                    printf("message form client[%d]:%s\n", i, resv_message);
                    // 读完之后准备写
                    ev.data.fd=client_fd;
                    ev.events=EPOLLOUT|EPOLLET;
                    epoll_ctl(efd, EPOLL_CTL_MOD, client_fd, &ev);
                    
                }
                else{
                    close(client_fd);
                    events[i].data.fd = -1;
                }
            }
            // 有数据发送
            else if (events[i].events & EPOLLOUT) {
                int sockfd = events[i].data.fd;

                // 收到什么就发送什么
                write(sockfd, resv_message, BUFF_SIZE);

                ev.data.fd=sockfd;
                ev.events=EPOLLIN|EPOLLET;
                //写完后，这个sockfd准备读
                epoll_ctl(efd,EPOLL_CTL_MOD,sockfd,&ev);
            }
        }
    }

    
    return 0;
}