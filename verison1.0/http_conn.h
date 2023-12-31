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
        LINE_OK = 0,        // 读取到一个完整的行
        LINE_BAD,           // 行出错
        LINE_OPEN           // 行数据不完整
    };

public:
    http_conn(){};
    ~http_conn(){};

public:
    void init(int sockfd, const sockaddr_in& addr); // 初始化新接受的连接
    void close_conn();                              // 关闭连接
    void process();                                 // 处理客户端请求
    bool read();                                    // 非阻塞读
    bool write();                                   // 非阻塞写

private:
    void init();                        // 初始化连接
    HTTP_CODE process_read();           // 解析HTTP请求
    bool process_write(HTTP_CODE ret);  // 填充HTTP响应

    // 由process_read调用分析HTTP请求
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_headers(char* text);
    HTTP_CODE parse_content(char* text);
    HTTP_CODE do_request();
    char* get_line() {
        return m_read_buf + m_start_line;
    }
    LINE_STATUS parse_line();

    // 由process_write调用填充HTTP响应
    void unmap();
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_content_type();
    bool add_status_line(int status, const char* title);
    void add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    // 该类所有对象共享类静态成员变量
    static int m_epollfd;               // 所有socket上的事件都被注册到同一个epoll内核事件中，所以设置静态
    static int m_user_count;            // 统计用户的数量

private:
    int m_sockfd;
    sockaddr_in m_address;

    char m_read_buf[READ_BUFFER_SIZE];  // 读缓冲区
    int m_read_idx;                     // 标识读缓冲区中已经读入的客户端数据的最后一个字节的下一个位置
    int m_checked_idx;                  // 当前正在分析的字符在读缓冲区中的位置
    int m_start_line;                   // 当前正在解析的行的起始位置

    CHECK_STATE m_check_state;          // 主状态机当前所处的状态
    METHOD m_method;                    // 请求方法

    char m_real_file[FILENAME_LEN];     // 客户请求的目标文件完整路径，等价于 doc_root + m_url
    char* m_url;                        // 请求目标文件名
    char* m_version;                    // HTTP版本号：1.1
    char* m_host;                       // 主机名
    int m_content_length;               // HTTP请求的消息总长度
    bool m_linger;                      // HTTP请求是否保持连接
    
    char m_write_buf[WRITE_BUFFER_SIZE];// 写缓冲区
    char m_write_idx;                   // 写缓冲区待发送的字节数
    char* m_file_address;               // 客户请求的目标文件被映射到内存中的起始位置
    struct stat m_file_stat;            // 目标文件的信息
    struct iovec m_iv[2];               // iovec表示IO缓冲区向量的结构。成员包含：iov_base, iov_len。采用writev来执行写操作，writev将数据从I/O缓冲区向量写入文件
    int m_iv_count;                     // m_iv_count表示被写内存块的数量。

    int bytes_to_send;                  // 将要发送的数据的字节数
    int bytes_have_send;                // 已经发送的字节数
    
};

#endif