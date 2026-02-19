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
