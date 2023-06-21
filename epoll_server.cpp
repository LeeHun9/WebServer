#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>

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
    }

    

}