#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include <cstdio>
#include <exception>
#include "locker.h"


template<typename T>
class threadpool {
public:
    threadpool(int thread_num = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T* request);
    
private:
    static void* worker(void* arg);
    void run();

private:
    // 线程数量
    int m_thread_num;

    // 线程池数组，大小为m_thread_num
    pthread_t* m_threads;

    // 请求队列中最多允许的请求数量
    int m_max_requests;

    // 请求队列
    std::list< T* > m_workqueue;

    // 请求队列互斥锁
    locker m_queuelocker;

    sem m_queuestat;

    bool m_stop;
};

template <typename T>
threadpool<T>::threadpool(int thread_num, int max_requests) : 
        m_thread_num(thread_num), m_max_requests(max_requests), 
        m_threads(NULL), m_stop(false) {
    if (thread_num <= 0 || max_requests <= 0) {
        throw std::exception();
    }

    // new请求线程空间
    m_threads = new pthread_t[m_thread_num];
    if (!m_threads) {
        throw std::exception();
    }

    // 创建m_thread_num个线程，并设置为脱离线程
    for (int i = 0; i < m_thread_num; i++) {
        if (pthread_create(m_threads+i, NULL, worker, this) != 0) {
            delete []m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]) != 0) {
            delete []m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool() {
    delete [] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T* request) {
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_front(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void* threadpool<T>::worker(void* arg) {
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run() {
    if (!m_stop) {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request) continue;
        request->process();
    }
    
}












#endif