#include <sys/epoll.h>
#include <memory>
#include <unordered_map>
#include <vector>


template<typename T>
class Epoll {
public:
    Epoll();
    ~Epoll();
    void epoll_add(T request, int timeout);
    void epoll_mod(T request, int timeout);
    void epoll_del(T request, int timeout);

private:
    static const int MAXFDS = 100000;
    int epollFD;
    std::vector<epoll_event> evens;
};

