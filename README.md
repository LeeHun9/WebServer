# WebServer

本项目主要用于记录C++11实现WebServer的过程。

阻塞和非阻塞、同步和异步

一个典型的网络IO接口调用，分为两个阶段，分别是“数据就绪” 和 “数据读写”，数据就绪阶段分为阻塞和非阻塞，表现得结果就是，阻塞当前线程或是直接返回。

网络IO阶段1

操作系统的TCP接收缓冲区

数据就绪：
- 阻塞：调用IO方法的线程进入阻塞状态
- 非阻塞：不会改变线程状态，程序会接着执行，通过返回值来判断

ssize_t recv(int sockfd, void *buf, size_t len, int flags);

网络IO阶段2

数据读写：
- 同步

```c
char buf[1024] = {0};
int size = recv(sockfd, buf, 1024, 0);
if (size > 0)
...
```

- 异步

需要将sockfd和buf和通知方式（signal）全部交给操作系统，操作系统将缓冲区数据传到buf中，然后通知用户程序来读buf。


处理IO时候，阻塞和非阻塞都是同步IO，用了特殊API才是异步IO
- aio_read()
- aio_write()