#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H
/*
    BlockDeque是基于std::deque封装的线程安全阻塞队列，是异步日志的核心依赖，作用是：
    主线程（生产者）：写日志时将日志字符串入队；
    刷盘线程（消费者）：从队列取日志字符串写入文件；
    核心特性：队列满时生产者阻塞、队列空时消费者阻塞，支持超时取数据、手动唤醒等。
*/
#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>
#include <atomic>
#include <cassert>

template<class T>
class BlockDeque {

public:
    explicit BlockDeque(size_t Max_Campacity = 1000);

    ~BlockDeque();

    void clear();

    bool empty();

    bool full();

    void stop();

    size_t size();

    size_t capacity();

    T front();

    T back();

    void push_front(const T& item);

    void push_back(const T& item);

    bool pop(T& item);

    bool pop(T& item, int timeout);

    void flush();

private:
    std::deque<T> deq_;

    size_t capacity_;

    std::mutex mtx_;

    std::atomic<bool> is_running_;
    
    std::condition_variable condConsumer_; // 生产者条件变量（队列满时阻塞生产者）

    std::condition_variable condProducer_; // 消费者条件变量（队列空时阻塞消费者）
};

// 模板定义不离头文件，实现放在头文件中

template<class T>
BlockDeque<T>::BlockDeque(size_t Max_Capacity) : capacity_(Max_Capacity) {
    assert(Max_Capacity > 0);
    is_running_.store(true);
}

template<class T>
BlockDeque<T>::~BlockDeque() {
    stop();
}

template<class T>
void BlockDeque<T>::stop() {
    {
        std::unique_lock<std::mutex> locker(mtx_);
        deq_.clear();
        is_running_.store(false);
    }
    condConsumer_.notify_all();
    condProducer_.notify_all();
}

template<class T>
void BlockDeque<T>::flush() {
    condProducer_.notify_one();
}

template<class T>
void BlockDeque<T>::clear() {
    std::unique_lock<std::mutex> locker(mtx_);
    deq_.clear();
}   

template<class T>
bool BlockDeque<T>::empty() {
    std::unique_lock<std::mutex> locker(mtx_);
    return deq_.empty();
}

template<class T>
bool BlockDeque<T>::full() {
    std::unique_lock<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

template<class T>
size_t BlockDeque<T>::size() {
    std::unique_lock<std::mutex> locker(mtx_);
    return deq_.size();
}

template<class T>
size_t BlockDeque<T>::capacity() {
    std::unique_lock<std::mutex> locker(mtx_);
    return capacity_;
}

template<class T>
T BlockDeque<T>::front() {
    std::unique_lock<std::mutex> locker(mtx_);
    return deq_.front();
}

template<class T>
T BlockDeque<T>::back() {
    std::unique_lock<std::mutex> locker(mtx_);
    return deq_.back();
}

template<class T>
void BlockDeque<T>::push_back(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {
        condProducer_.wait(locker); // 队列满时阻塞生产者,直到消费者消费了一个元素并通知生产者
    }
    deq_.push_back(item);
    condConsumer_.notify_one(); // 通知消费者有新元素入队
}

template<class T>
void BlockDeque<T>::push_front(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one(); // 通知消费者有新元素入队
}

// pop函数返回值表示是否成功取到元素（false表示队列已停止且无元素可取）
template<class T>
bool BlockDeque<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()) {
        // 队列空时阻塞消费者,直到生产者生产了一个元素并通知消费者
        condConsumer_.wait(locker);
        if(!is_running_.load()) {
            return false; // 队列已停止且无元素可取
        }
        // 如果被唤醒但队列仍然空，继续等待
    }
    // 否则成功取到元素
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one(); // 通知生产者有空间入队
    return true;
}

// 带超时的pop函数，返回值表示是否成功取到元素（false表示超时或队列已停止且无元素可取）
// timeout单位为秒
// 支持 “超时等待”：队列空时最多等timeout秒，超时则返回false；
// 避免消费者无限阻塞（比如日志系统退出时）。
template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()) {
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) == std::cv_status::timeout) {
            return false; // 超时未取到元素
        }
        if(!is_running_.load()) {
            return false; // 队列已停止且无元素可取
        }
        // 如果被唤醒但队列仍然空，继续等待
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one(); // 通知生产者有空间入队
    return true;
}

#endif /* BLOCKQUEUE_H */
