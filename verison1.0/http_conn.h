#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include "locker.h"
#include <sys/uio.h>


class http_conn {
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;

    /*
        HTTP 请求方法，只支持GET
    */
    enum METHOD {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT
    };

    /*
        解析客户端请求时，主状态机的状态
    */
    enum CHECK_STATE {
        CHECK_STATE_REQUESTLINE = 0,    // 正在分析请求行
        CHECK_STATE_HEADER,             // 正在分析头部字段
        CHECK_STATE_CONTENT             // 正在解析请求体
    };

    /*
    */
    enum HTTP_CODE {
        NO_REQUEST,         // 请求不完整，需要继续读取客户数据
        GET_REQUEST,        // 表示获得了一个完成的客户请求
        BAD_REQUEST,        // 表示客户请求语法错误
        NO_RESOURCE,        // 表示服务器没有资源
        FORBIDDEN_REQUEST,  // 表示客户对资源没有足够的访问权限
        FILE_REQUEST,       // 文件请求,获取文件成功
        INTERNAL_ERROR,     // 表示服务器内部错误
        CLOSED_CONNECTION   // 表示客户端已经关闭连接了
    };

    enum LINE_STATUS {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn(){};
    ~http_conn(){};
public:
    void init(int sockfd, const sockaddr_in& addr);
    void close_conn();
    void process();
    bool read();
    bool write();

private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);

    
    
};




#endif