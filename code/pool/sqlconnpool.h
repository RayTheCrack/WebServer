#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <mutex>
#include <string>
#include <queue>
#include <condition_variable>
#include <thread>
#include <atomic>

#include "../log/log.h"
/*
    为什么需要SQL连接池？
    1. 数据库连接是一种稀缺资源，频繁的创建和销毁连接会带来额外的开销
    2. 连接池可以复用已经创建的连接，减少创建和销毁连接的开销
    3. 连接池可以限制连接的数量，防止过多的连接占用系统资源
    4. 连接池可以提供连接的负载均衡，提高系统的并发性能
*/

class SqlConnPool {

public:
    static SqlConnPool& getInstance(); // 获取单例

    MYSQL* getConnection(); // 获取连接

    void freeConnection(MYSQL* conn); // 释放连接

    int GetFreeConnCnt() const; // 获取空闲连接数

    void Init(const char* host, int port,
            const char* user,const char* pwd, 
            const char* dbName, int connSize); // 初始化连接池

    void ClosePool(); // 关闭连接池

private:
    SqlConnPool();
    ~SqlConnPool();

    SqlConnPool(const SqlConnPool&) = delete;
    SqlConnPool& operator=(const SqlConnPool&) = delete;

    int MAX_CONN_;
    std::atomic<int> useCount_;
    std::atomic<int> freeCount_;

    std::queue<MYSQL*> connQue_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;// 信号量：控制可获取的空闲连接数，实现「无连接时线程阻塞等待」
};

#endif /* SQLCONNPOOL_H */
