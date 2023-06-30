#include "http_conn.h"

const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";


int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);    // F_GETFL 用于获取fd的文件状态标志位
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool one_shot) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;    // 设置EPOLLRDHUP表示内核不排队从socket读取任何数据。
    if (one_shot) {
        event.events |= EPOLLONESHOT;   // 确保该事件只通知一次，防止同一通信被不同线程处理
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    // 设置fd非阻塞
    setnonblocking(fd);
}

void delfd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}

void modfd(int epollfd, int fd, int ev) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// 网站根目录
const char* doc_root = "/home/hongxuanquan/NewcoderServer/resources";

// 初始化静态类成员变量
// 所有的客户数
int http_conn::m_user_count = 0;
// 所有socket上的事件都被注册到同一个epoll内核事件中，所以设置成静态的
int http_conn::m_epollfd = -1;

void http_conn::close_conn() {
    if (m_sockfd != -1) {
        delfd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        --m_user_count;
    }
}

void http_conn::init(int sockfd, const sockaddr_in &addr) {
    m_sockfd = sockfd;
    m_address = addr;

    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    // 添加sockfd到epoll监听
    addfd(m_epollfd, m_sockfd, true);
    ++m_user_count;
    init();
}

void http_conn::init() {
    bytes_to_send = 0;
    bytes_have_send = 0;

    m_check_state = CHECK_STATE_REQUESTLINE;    // 初始状态为检查请求行
    m_linger = false;                           // 默认不保持链接  Connection : keep-alive保持连接

    m_method = GET;                             // 默认请求方式为GET
    m_url = 0;              
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;

    bzero(m_read_buf, READ_BUFFER_SIZE);
    bzero(m_write_buf, READ_BUFFER_SIZE);
    bzero(m_real_file, FILENAME_LEN);
}

// 一次性循环读取客户数据，直到无数据或者客户关闭连接
bool http_conn::read() {
    if (m_read_idx >= READ_BUFFER_SIZE) {   // 超出读缓冲区大小
        return false;
    }
    int bytes_read = 0;
    while (1) {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        // error occurred
        if (bytes_read == -1) {
            // 非阻塞模式下，没有数据需要读取了
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return false;
        } else if (bytes_read == 0) {   // 客户关闭连接
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

// 解析一行，判断依据是"\r\n"。之后m_checked_idx会指向下一行的开头
http_conn::LINE_STATUS http_conn::parse_line() {
    char tmp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx) {
        // 逐位读取
        tmp = m_read_buf[m_checked_idx];
        if (tmp == '\r') {
            if (m_checked_idx + 1 == m_read_idx) {
                return LINE_OPEN;
            } else if (m_read_buf[m_checked_idx + 1] == '\n') {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (tmp == '\n') {
            if ((m_checked_idx > 1) && (m_read_buf[m_checked_idx-1] == '\r')) {
                m_read_buf[m_checked_idx-1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

// 解析HTTP请求行，请求方式、URL、HTTP版本信息
http_conn::HTTP_CODE http_conn::parse_request_line(char* text) {
    printf("Parsing HTTP Request line.\n");
    // 获取url，通过第一个制表符'\t'位置获取 "GET /index.html HTTP/1.1"
    m_url = strpbrk(text, " \t");
    if (!m_url) {
        return BAD_REQUEST;
    }
    // "GET\0/index.html HTTP/1.1"
    *m_url++ = '\0';
    // method = "GET"
    char* method = text;
    // 只支持GET请求方式
    if (strcasecmp(method, "GET") == 0) {   // strcasecmp 不区分大小写
        printf("HTTP Request Method is GET\n");
        m_method = GET;
    } else {
        return BAD_REQUEST;
    }
    // m_url = "/index.html HTTP/1.1"
    m_version = strpbrk(m_url, " \t");
    if (!m_version) {
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    if (strcasecmp(m_version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }
    if (strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if (!m_url || m_url[0] != '/') {
        return BAD_REQUEST;
    }
    m_check_state = CHECK_STATE_HEADER;     // 解析请求行完毕，检查状态变为检查头部
    return NO_REQUEST;
}

// 解析请求的一个头部信息 只支持解析 Connection、Content-Length、Host
http_conn::HTTP_CODE http_conn::parse_headers(char* text) {
    printf("Parsing HTTP Request header: %s\n", text);
    if (text[0] == '\0') {
        // 空行
        // 若http请求有消息体，则读取m_content_length字节的消息体
        if (m_content_length != 0) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        // 处理 Connection 字段
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;
        }
    } else if (strncasecmp( text, "Content-Length:", 15 ) == 0) {
        // Content-Length
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    } else if (strncasecmp( text, "Host:", 5 ) == 0 ) {
        // 处理Host头部字段
        text += 5;
        text += strspn( text, " \t" );
        m_host = text;
        printf("Remote Host: %s\n", m_host);
    } else {
        printf("oop! unknow header: %s\n", text);
    }
    return NO_REQUEST; 
}

// 没有真正解析content，只是判断是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char* text) {
    if (m_read_idx >= m_content_length + m_checked_idx) {
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// 对完整的正确的HTTP请求进行处理，分析文件属性等
// 然后使用mmap将文件映射到内存地址m_file_address处，告诉调用者获取文件成功
http_conn::HTTP_CODE http_conn::do_request() {
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    // 拼接root+url
    strncpy(m_real_file + len, m_url, FILENAME_LEN-len-1);
    // 获取请求文件属性
    if (stat(m_real_file, &m_file_stat) < 0) {
        return NO_RESOURCE;
    }

    // 判断访问权限
    if (!(m_file_stat.st_mode & S_IROTH)) {
        return FORBIDDEN_REQUEST;
    }

    // 判断是否是目录
    if (S_ISDIR(m_file_stat.st_mode)) {
        return BAD_REQUEST;
    }

    // 只读打开文件
    int fd = open(m_real_file, O_RDONLY);

    // 创建文件虚拟内存映射，映射地址设为随机（NULL）
    m_file_address = (char*)mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

// 主状态机，解析客户请求
http_conn::HTTP_CODE http_conn::process_read() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;
    while (((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) ||
     ((line_status = parse_line()) == LINE_OK)) {
        // 读取一行数据
        text = get_line();
        m_start_line = m_checked_idx;
        printf("got 1 http line: %s\n", text);

        switch(m_check_state) {
            case CHECK_STATE_REQUESTLINE: {
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER: {
                ret = parse_headers(text);
                if (ret == BAD_REQUEST) {        // bug 点：NO_REQUEST，设置成 BAD_REQUEST 可解决
                    return BAD_REQUEST;
                } else if (ret == GET_REQUEST) {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT: {
                ret = parse_content(text);
                if (ret == GET_REQUEST) {
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            default: {
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

// 对do_request中映射的内存区域执行munmap操作 
void http_conn::unmap() {
    if (m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

// 向m_sockfd内写入HTTP响应
bool http_conn::write() {
    int tmp = 0;
    if (bytes_to_send == 0) {
        // 表示这一次响应结束，epoll设置socket监听数据到来
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while (1) {
        tmp = writev(m_sockfd, m_iv, m_iv_count);
        if (tmp < -1) {
            // 系统暂时无法写入数据
            if (errno == EAGAIN) {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += tmp;
        bytes_to_send -= tmp;

        if (bytes_have_send >= m_iv[0].iov_len) {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + bytes_have_send - m_write_idx;
            m_iv[1].iov_len = bytes_to_send;
        } else {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - tmp;
        }

        if (bytes_to_send <= 0) {
            // 没有可发送数据
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);

            if (m_linger) {
                init();
                return true;
            } else {
                return false;      // 返回false直接关闭连接
            }
        }
    }
}

// 与写入响应有关的函数
bool http_conn::add_response(const char* format, ...) {
    if (m_write_idx >= WRITE_BUFFER_SIZE) {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= WRITE_BUFFER_SIZE - m_write_idx -1) {
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);       // 清理参数列表
    return true;
}

bool http_conn::add_status_line(int status, const char* title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

void http_conn::add_headers(int content_len) {
    add_content_length(content_len);
    add_content_type();
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_length(int content_len) {
    return add_response("Content-Length: %d\r\n", content_len);
}

bool http_conn::add_linger() {
    return add_response("Connection: %s\r\n", m_linger == true? "keep-alive":"close");
}

bool http_conn::add_blank_line(){
    return add_response("%s","\r\n");
}

bool http_conn::add_content(const char* content) {
    return add_response("%s", content);
}

bool http_conn::add_content_type() {
    return add_response("Content-Type:%s\r\n", "text/html");
}

// 根据http请求结果，返回客户端内容
bool http_conn::process_write(HTTP_CODE ret) {
    switch (ret) {
        case INTERNAL_ERROR: {
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if (!add_content(error_500_form)) return false;
            break;
        }
        case BAD_REQUEST: {
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if ( ! add_content( error_400_form ) ) return false;
            break;
        }
        case NO_RESOURCE: {
            add_status_line( 404, error_404_title );
            add_headers( strlen( error_404_form ) );
            if ( ! add_content( error_404_form ) ) return false;
            break;
        }
        case FORBIDDEN_REQUEST: {
            add_status_line( 403, error_403_title );
            add_headers(strlen( error_403_form));
            if ( ! add_content( error_403_form ) ) return false;
            break;
        }
        case FILE_REQUEST: {
            add_status_line(200, ok_200_title);
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;

            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        default:
            return false;
    }

    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;

}


// 线程池工作线程调用，处理HTTP请求的入口函数
void http_conn::process() {
    // 解析HTTP请求
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);    // 套接字准备好读取
        return;
    }

    // 生成HTTP响应
    bool write_ret = process_write(read_ret);
    if (!write_ret) {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);       // 套接字准备好写入
}

