#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>


#define MAXLINE 80
#define SERV_PORT 8000

/*
int main() {
    int i, maxi, maxfd;
	int listenfd, connfd, sockfd;

    int nready, client[FD_SETSIZE];
    ssize_t n;

    fd_set rset, allset;

    char buf[MAXLINE];
    char str[INET_ADDRSTRLEN];

    socklen_t clientaddr_len;
    struct sockaddr_in clientaddr, serveraddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&serveraddr, sizeof(serveraddr));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERV_PORT);

    bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));

    listen(listenfd, 20); // 最大128个

    maxfd = listenfd;

    maxi = -1;      // client列表中最大下标

    for (i = 0; i < FD_SETSIZE; i++) {
        client[i] = -1;
    }

    FD_ZERO(&allset);

    FD_SET(listenfd, &allset);

    while(1) {
        
        rset = allset;          // 每次循环重新设置select监控信号集

        // select注册监听
        nready = select(maxfd+1, &rset, NULL, NULL, NULL);

        if (nready < 0) {
            perror("select errors");
            exit(-1);
        }

        if (FD_ISSET(listenfd, &rset)) {
            clientaddr_len = sizeof(clientaddr);
            connfd = accept(listenfd, (struct sockaddr*)&clientaddr, &clientaddr_len); // accept返回通信套接字，当前非阻塞，因为select已经发生读写事件

            printf("received from %s at PORT %d\n",
                   inet_ntop(AF_INET, &clientaddr.sin_addr, str, sizeof(str)),
                   ntohs(clientaddr.sin_port));

            for (i = 0; i < FD_SETSIZE; i++) {
                if (client[i] < 0) {
                    client[i] = connfd;
                    break;
                }
            }

            if (i == FD_SETSIZE) {
                fputs("too many clients\n", stderr);
                exit(1);
            }

            FD_SET(connfd, &allset);

            if (connfd > maxfd) {
                maxfd = connfd;
            }

            if (i > maxi) {
                maxi = i;
            }

            if (--nready == 0) continue;

        }


        // 处理已经accept的客户端连接
        for (i = 0; i <= maxi; i++) {
            if ((sockfd = client[i]) < 0) continue;

            if (FD_ISSET(sockfd, &rset)) {
                if ((n = read(sockfd, buf, MAXLINE)) == 0) {
                    close(sockfd);
                    FD_CLR(sockfd, &rset);
                    client[i] = -1;
                }
                else {
                    int j;
                    for (j = 0; j < n; j++) {
                        buf[j] = toupper(buf[j]);
                    }
                    write(sockfd, buf, n);
                }
                if (--nready == 0) {
                    break;
                }
            }
            
        }
    }
    close(listenfd);
    return 0;
}
*/
#define BUFF_SIZE 1024
#define backlog 7
#define ser_port 11277
#define CLI_NUM 3

int client_fds[CLI_NUM];
 
int main(int agrc,char **argv)
{
    int ser_souck_fd;
    int i;  
    char input_message[BUFF_SIZE];
    char resv_message[BUFF_SIZE];
 
 
    struct sockaddr_in ser_addr;
    ser_addr.sin_family= AF_INET;    //IPV4
    ser_addr.sin_port = htons(ser_port);
    ser_addr.sin_addr.s_addr = INADDR_ANY;  //指定的是所有地址
 
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
    if(listen(ser_souck_fd, backlog) < 0)
    {
        perror("listen failure");
        return -1;
    }
 
 
    //fd_set
    fd_set ser_fdset;
    int max_fd=1;
    struct timeval mytime;
    printf("wait for client connnect!\n");
 
    while(1)
    {
        mytime.tv_sec=27;
        mytime.tv_usec=0;
 
        FD_ZERO(&ser_fdset);
 
        //add standard input
        FD_SET(0,&ser_fdset);
        if(max_fd < 0)
        {
            max_fd=0;
        }
 
        //add serverce
        FD_SET(ser_souck_fd,&ser_fdset);
        if(max_fd < ser_souck_fd)
        {
            max_fd = ser_souck_fd;
        }
 
        //add client
        for(i=0;i<CLI_NUM;i++)  //用数组定义多个客户端fd
        {
            if(client_fds[i]!=0)
            {
                FD_SET(client_fds[i],&ser_fdset);
                if(max_fd < client_fds[i])
                {
                    max_fd = client_fds[i];
                }
            }
        }
 
        //select多路复用
        int ret = select(max_fd + 1, &ser_fdset, NULL, NULL, &mytime);
 
        if(ret < 0)   
        {   
            perror("select failure\n");   
            continue;   
        }   
 
        else if(ret == 0)
        {
            printf("time out!");
            continue;
        }
 
        else
        {
            if(FD_ISSET(0,&ser_fdset)) //标准输入是否存在于ser_fdset集合中（也就是说，检测到输入时，做如下事情）
            {
                printf("send message to");
                bzero(input_message,BUFF_SIZE);
                fgets(input_message,BUFF_SIZE,stdin);
 
                for(i=0;i<CLI_NUM;i++)
                {
                    if(client_fds[i] != 0)
                    {
                        printf("client_fds[%d]=%d\n", i, client_fds[i]);
                        send(client_fds[i], input_message, BUFF_SIZE, 0);
                    }
                }
 
            }
 
            if(FD_ISSET(ser_souck_fd, &ser_fdset))
            {
                struct sockaddr_in client_address;
                socklen_t address_len;
                int client_sock_fd = accept(ser_souck_fd,(struct sockaddr *)&client_address, &address_len);
                if(client_sock_fd > 0)
                {
                    int flags=-1;
                    //一个客户端到来分配一个fd，CLI_NUM=3，则最多只能有三个客户端，超过4以后跳出for循环，flags重新被赋值为-1
                    for(i=0;i<CLI_NUM;i++)
                    {
                        if(client_fds[i] == 0)
                        {
                            flags=i;
                            client_fds[i] = client_sock_fd;
                            break;
                        }
                    }
 
 
                    if (flags >= 0)
                    {
                        printf("new user client[%d] add sucessfully!\n",flags);
 
                    }
 
                    else //flags=-1
                    {  
                        char full_message[]="the client is full!can't join!\n";
                        bzero(input_message,BUFF_SIZE);
                        strncpy(input_message, full_message,100);
                        send(client_sock_fd, input_message, BUFF_SIZE, 0);
 
                    }
                }   
            }
 
        }
 
        //deal with the message
 
        for(i=0; i<CLI_NUM; i++)
        {
            if(client_fds[i] != 0)
            {
                if(FD_ISSET(client_fds[i],&ser_fdset))
                {
                    bzero(resv_message,BUFF_SIZE);
                    int byte_num=read(client_fds[i],resv_message,BUFF_SIZE);
                    if(byte_num > 0)
                    {
                        printf("message form client[%d]:%s\n", i, resv_message);
                    }
                    else if(byte_num < 0)
                    {
                        printf("rescessed error!");
                    }
 
                    //某个客户端退出
                    else  //cancel fdset and set fd=0
                    {
                        printf("clien[%d] exit!\n",i);
                        FD_CLR(client_fds[i], &ser_fdset);
                        client_fds[i] = 0;
                       // printf("clien[%d] exit!\n",i);
                        continue;  //这里如果用break的话一个客户端退出会造成服务器也退出。 
                    }
                }
            }
        }   
    }
    return 0;
}