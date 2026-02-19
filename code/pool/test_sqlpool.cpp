
#include "sqlconnpool.h"
#include "sqlconnRAII.h"
#include "../log/log.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

// 测试函数：使用连接池执行查询
void test_query(int thread_id) {
    LOG_INFO("Thread {}: Starting query test", thread_id);

    MYSQL* sql = nullptr;
    SqlConnRAII raii(&sql, &SqlConnPool::getInstance());

    if (!sql) {
        LOG_ERROR("Thread {}: Failed to get MySQL connection", thread_id);
        return;
    }

    // 执行测试查询
    const char* query = "SELECT VERSION()";
    if (mysql_query(sql, query) != 0) {
        LOG_ERROR("Thread {}: Query failed: {}", thread_id, mysql_error(sql));
        return;
    }

    MYSQL_RES* result = mysql_store_result(sql);
    if (result) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            LOG_INFO("Thread {}: MySQL Version: {}", thread_id, row[0]);
        }
        mysql_free_result(result);
    }

    LOG_INFO("Thread {}: Query completed", thread_id);
}

// 测试函数：测试连接池并发性能
void test_concurrent_queries(int num_threads, int queries_per_thread) {
    LOG_INFO("Starting concurrent test with {} threads, {} queries per thread", 
             num_threads, queries_per_thread);

    std::vector<std::thread> threads;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i, queries_per_thread]() {
            for (int j = 0; j < queries_per_thread; ++j) {
                test_query(i);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    LOG_INFO("Concurrent test completed in {} ms", duration.count());
    LOG_INFO("Total queries: {}, Average time per query: {:.2f} ms", 
             num_threads * queries_per_thread,
             static_cast<double>(duration.count()) / (num_threads * queries_per_thread));
}

// 测试函数：测试连接池状态
void test_pool_status() {
    LOG_INFO("=== Connection Pool Status ===");
    LOG_INFO("Free connections: {}", SqlConnPool::getInstance().GetFreeConnCnt());
}

int main() {
    // 初始化日志系统
    Logger::getInstance().initLogger("log/test_sqlpool.log", LogLevel::DEBUG, 1024, 3);

    LOG_INFO("=== Starting SQL Connection Pool Test ===");

    // 初始化连接池
    const char* host = "127.0.0.1";
    const char* user = "root";
    const char* pwd = "password";
    const char* dbName = "webserver";
    int port = 3306;
    int connSize = 10;

    LOG_INFO("Initializing connection pool...");
    LOG_INFO("Host: {}, Port: {}, User: {}, Database: {}, Pool Size: {}",
             host, port, user, dbName, connSize);

    SqlConnPool::getInstance().Init(host, port, user, pwd, dbName, connSize);

    // 测试1：单线程查询
    LOG_INFO("\n=== Test 1: Single Thread Query ===");
    test_query(0);
    test_pool_status();

    // 测试2：并发查询
    LOG_INFO("\n=== Test 2: Concurrent Queries ===");
    test_concurrent_queries(5, 3);
    test_pool_status();

    // 测试3：高并发压力测试
    LOG_INFO("\n=== Test 3: High Concurrency Stress Test ===");
    test_concurrent_queries(20, 5);
    test_pool_status();

    // 关闭连接池
    LOG_INFO("\n=== Closing Connection Pool ===");
    SqlConnPool::getInstance().ClosePool();

    // 关闭日志系统
    Logger::getInstance().shutdown();

    LOG_INFO("=== Test Completed ===");
    std::cout << "Test completed. Check test_sqlpool.log for details." << std::endl;

    return 0;
}
