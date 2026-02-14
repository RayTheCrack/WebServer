# WebServer 完整重写教程（详细逐步指南）

本文档提供了一个**完整可跟随的教程**，从零开始逐步构建 C++ WebServer 项目。每一步都包含详细的代码框架、实现说明、编译命令和测试步骤，使你能够逐行编写代码。

---

## 总体架构与目标

### 项目核心功能
1. **HTTP/1.1 协议支持** — 解析 GET/POST 请求、返回静态文件
2. **高并发处理** — epoll + 线程池架构
3. **可观测性** — 异步日志系统，支持日志级别和文件滚动
4. **资源管理** — 数据库连接池、定时器管理连接超时
5. **静态文件服务** — 支持 MIME 类型、错误页面、安全路径检查

# 【第 3 步】日志模块

## 目标
实现一个现代 C++ 风格的异步日志系统，满足：

- 支持日志级别（DEBUG/INFO/WARN/ERROR）
- 异步写入（主线程将日志消息入队，后台线程负责写盘）
- 支持日志文件路径配置、按日期轮转（可扩展）
- 使用 `std::string`，单例用静态局部变量返回引用
- 基于提供的 `BlockDeque` 作为消息队列实现阻塞/超时取出

设计思想：配置优先级由 `Config` 提供，日志系统由 `Logger::initLogger(...)` 初始化（可在 `main` 启动时通过 `Config` 进行初始化）。主线程记录日志时尽量只做格式化并入队，避免阻塞业务。

### 关键点

- 使用 `std::unique_ptr<BlockDeque<std::string>>`，在 `initLogger` 时以 `max_queue_size` 构造队列。
- 日志后台线程使用 `pop(..., timeout)` 带超时的方式取消息，保证在退出时能检测到停止条件并刷盘。
- 提供 `format_string` 辅助函数实现 printf 风格格式化，返回 `std::string`。

下面把实现完整地列出（代码即仓库实现）：

#### `code/log/blockqueue.h`

```cpp
// BlockDeque 已修复版本（线程安全阻塞队列）
#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H
/*
    BlockDeque是基于std::deque封装的线程安全阻塞队列，是异步日志的核心依赖。
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
    
    std::condition_variable condConsumer_;

    std::condition_variable condProducer_;
};

// 模板实现
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
        condProducer_.wait(locker);
    }
    deq_.push_back(item);
    condConsumer_.notify_one();
}

template<class T>
void BlockDeque<T>::push_front(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}

template<class T>
bool BlockDeque<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()) {
        condConsumer_.wait(locker);
        if(!is_running_.load()) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()) {
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) == std::cv_status::timeout) {
            return false;
        }
        if(!is_running_.load()) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

#endif /* BLOCKQUEUE_H */
```

#### `code/log/log.h`

```cpp
// Logger 头文件（现代 C++）
#ifndef LOG_H
#define LOG_H

#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <fstream>
#include <atomic>
#include <cstdarg>

#include "blockqueue.h"
#include "../config/config.h"

enum class LogLevel { DEBUG=0, INFO=1, WARN=2, ERROR=3 };

class Logger {
public:
    static Logger& getInstance();

    void initLogger(const std::string& log_file = "./log/webserver.log", LogLevel level = LogLevel::INFO, int max_queue_size = 1024);
    void log(LogLevel level, const std::string& message);
    void shutdown();
    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log_worker_thread();
    std::string get_timestamp();
    std::string get_level_name(LogLevel level);

    std::string log_file_;
    std::string current_date_;
    std::ofstream log_stream_;
    LogLevel current_level_;

    std::unique_ptr<BlockDeque<std::string>> message_queue_;
    std::thread worker_thread_;
    std::atomic<bool> is_running_ {false};
};

std::string format_string(const std::string& fmt, ...);

#define LOG_DEBUG(fmt, ...) \
    do { Logger::getInstance().log(LogLevel::DEBUG, format_string(fmt, ##__VA_ARGS__)); } while(0)
#define LOG_INFO(fmt, ...) \
    do { Logger::getInstance().log(LogLevel::INFO, format_string(fmt, ##__VA_ARGS__)); } while(0)
#define LOG_WARN(fmt, ...) \
    do { Logger::getInstance().log(LogLevel::WARN, format_string(fmt, ##__VA_ARGS__)); } while(0)
#define LOG_ERROR(fmt, ...) \
    do { Logger::getInstance().log(LogLevel::ERROR, format_string(fmt, ##__VA_ARGS__)); } while(0)

#endif // LOG_H
```

#### `code/log/log.cpp`

```cpp
// Logger 实现（异步写入）
#include "log.h"

#include <chrono>
#include <iomanip>
#include <filesystem>
#include <iostream>

using namespace std::chrono_literals;

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::initLogger(const std::string& log_file, LogLevel level, int max_queue_size) {
    if (is_running_.load()) return;
    current_level_ = level;
    log_file_ = log_file;

    try {
        std::filesystem::path p(log_file_);
        if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    } catch(...) {}

    current_date_ = get_timestamp().substr(0, 10);
    if (!log_file_.empty()) log_stream_.open(log_file_, std::ios::app);

    message_queue_.reset(new BlockDeque<std::string>(static_cast<size_t>(max_queue_size)));
    is_running_.store(true);
    worker_thread_ = std::thread(&Logger::log_worker_thread, this);
}

void Logger::setLogLevel(LogLevel level) { current_level_ = level; }
LogLevel Logger::getLogLevel() const { return current_level_; }

void Logger::log(LogLevel level, const std::string& message) {
    if (!is_running_.load()) {
        if (level >= current_level_) {
            std::string line = get_timestamp() + " [" + get_level_name(level) + "] " + message + "\n";
            std::fwrite(line.data(), 1, line.size(), stdout);
        }
        eturrn;
    }
    if (level < current_level_) return;
    std::string line = get_timestamp() + " [" + get_level_name(level) + "] " + message;
    message_queue_->push_back(line);
}

void Logger::shutdown() {
    if (!is_running_.load()) return;
    is_running_.store(false);
    if (message_queue_) message_queue_->stop();
    if (worker_thread_.joinable()) worker_thread_.join();
    if (log_stream_.is_open()) { log_stream_.flush(); log_stream_.close(); }
}

Logger::~Logger() { shutdown(); }

std::string Logger::get_timestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto tt = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm{};
    localtime_r(&tt, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::get_level_name(LogLevel level) {
    switch(level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
    }
    return "INFO";
}

void Logger::log_worker_thread() {
    std::string entry;
    while (is_running_.load() || (message_queue_ && message_queue_->size() > 0)) {
        if (!message_queue_) break;
        bool ok = message_queue_->pop(entry, 1); // 1s timeout
        if (!ok) continue;
        if (log_stream_.is_open()) log_stream_ << entry << std::endl;
        else std::cout << entry << std::endl;
    }
    if (log_stream_.is_open()) log_stream_.flush();
}

// 简单 printf 风格格式化
std::string format_string(const std::string& fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    const int needed = std::vsnprintf(nullptr, 0, fmt.c_str(), ap);
    std::string buf(needed + 1, '\0');
    std::vsnprintf(&buf[0], buf.size(), fmt.c_str(), ap2);
    va_end(ap2);
    va_end(ap);
    buf.resize(needed);
    return buf;
}
```

### 使用与集成

- 在 `main.cpp` 中通过 `Config` 读取 `log_file` 与 `log_level`，然后调用：

```cpp
Logger::getInstance().initLogger(config.log_file, (LogLevel)config.log_level, 2048);
```

- 使用 `LOG_INFO(...)` / `LOG_ERROR(...)` 等宏记录日志。
- 程序退出时可显式调用 `Logger::getInstance().shutdown()`，或者依赖单例析构自动调用。

### 测试建议

- 编译并短时运行：
```
make
timeout 2 ./bin/server || true
```
- 检查配置指定的日志文件是否出现并包含时间戳与日志级别。

以上实现遵循现代 C++ 风格（`std::string`、智能指针、静态局部单例），并基于你提供的 `BlockDeque` 实现异步队列。
修改 Makefile 的 `SOURCES` 行：

```makefile
SOURCES = $(SRC_DIR)/main.cpp config/config.cpp
```

## 2.4 更新 main.cpp

```cpp
#include "config/config.h"
#include <iostream>
#include <unistd.h>

// 简单的日志宏（后续将被完整日志模块替换）
#define LOG_INFO(fmt, ...) \
    fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

int main(int argc, char* argv[]) {
    // 获取配置单例并解析命令行
    Config& config = Config::GetInstance();
    config.parse_command_line(argc, argv);

    // 打印启动信息
    LOG_INFO("=== WebServer Starting ===");
    config.print_config();

    // 占位循环
    LOG_INFO("Placeholder loop. Press Ctrl+C to exit.");
    while (true) {
        sleep(1);
    }

    return 0;
}
```

## 2.5 编译与测试

```bash
make clean
make
./bin/server -p 8080 -t 4 -r resources/
```

**预期输出：**
```
[INFO] === WebServer Starting ===
=== WebServer Configuration ===
Port: 8080
Threads: 4
Resource Root: resources/
...
```

---

---

# 【第 3 步】日志模块

## 目标
实现异步日志系统，支持日志级别、文件输出和日志滚动（按日期）。

## 3.1 创建 log/log.h

```cpp
#ifndef LOG_H
#define LOG_H

#include <string>
#include <queue>
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <fstream>
#include <ctime>

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger {
public:
    static Logger& GetInstance();

    // 初始化日志系统
    void init(const std::string& log_file, LogLevel level = LogLevel::INFO);

    // 记录日志
    void log(LogLevel level, const std::string& message);

    // 关闭日志系统
    void shutdown();

    // 设置日志级别
    void set_level(LogLevel level) { current_level_ = level; }

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log_worker_thread();
    void flush_to_file();
    std::string get_timestamp();
    std::string get_level_name(LogLevel level);
    void check_and_rotate();  // 检查日志文件是否需要轮转

    std::string log_file_;
    std::string current_date_;
    std::ofstream log_stream_;
    LogLevel current_level_;
    
    std::queue<std::string> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread worker_thread_;
    bool is_running_;
};

// 便捷日志宏
#define LOG_DEBUG(fmt, ...) \
    Logger::GetInstance().log(LogLevel::DEBUG, format_string(fmt, ##__VA_ARGS__))
#define LOG_INFO(fmt, ...) \
    Logger::GetInstance().log(LogLevel::INFO, format_string(fmt, ##__VA_ARGS__))
#define LOG_WARN(fmt, ...) \
    Logger::GetInstance().log(LogLevel::WARN, format_string(fmt, ##__VA_ARGS__))
#define LOG_ERROR(fmt, ...) \
    Logger::GetInstance().log(LogLevel::ERROR, format_string(fmt, ##__VA_ARGS__))

// 辅助函数：格式化字符串
std::string format_string(const char* fmt, ...);

#endif  // LOG_H
```

## 3.2 创建 log/log.cpp

```cpp
#include "log.h"
#include <iostream>
#include <cstdarg>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>

std::string format_string(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    int size = vsnprintf(nullptr, 0, fmt, args) + 1;
    va_end(args);

    va_start(args, fmt);
    std::string result(size, '\0');
    vsnprintf(&result[0], size, fmt, args);
    va_end(args);

    return result;
}

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : current_level_(LogLevel::INFO), is_running_(false) {}

Logger::~Logger() {
    shutdown();
}

void Logger::init(const std::string& log_file, LogLevel level) {
    log_file_ = log_file;
    current_level_ = level;

    // 创建日志目录（如果不存在）
    size_t last_slash = log_file.find_last_of('/');
    if (last_slash != std::string::npos) {
        std::string dir = log_file.substr(0, last_slash);
        mkdir(dir.c_str(), 0755);
    }

    // 打开日志文件
    log_stream_.open(log_file, std::ios::app);
    if (!log_stream_.is_open()) {
        std::cerr << "[ERROR] Failed to open log file: " << log_file << std::endl;
        return;
    }

    is_running_ = true;
    worker_thread_ = std::thread(&Logger::log_worker_thread, this);
}

void Logger::shutdown() {
    if (!is_running_) return;

    is_running_ = false;
    queue_cv_.notify_one();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    flush_to_file();
    if (log_stream_.is_open()) {
        log_stream_.close();
    }
}

std::string Logger::get_timestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string Logger::get_level_name(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void Logger::check_and_rotate() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    std::ostringstream date_oss;
    date_oss << std::put_time(&tm, "%Y-%m-%d");
    std::string today = date_oss.str();

    if (current_date_.empty()) {
        current_date_ = today;
    } else if (current_date_ != today) {
        // 需要轮转日志文件
        log_stream_.close();

        std::string new_log_file = log_file_ + "." + current_date_;
        rename(log_file_.c_str(), new_log_file.c_str());

        log_stream_.open(log_file_, std::ios::app);
        current_date_ = today;
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < current_level_ || !is_running_) return;

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        std::string log_message = "[" + get_timestamp() + "] [" + 
                                 get_level_name(level) + "] " + message;
        message_queue_.push(log_message);
    }
    queue_cv_.notify_one();
}

void Logger::flush_to_file() {
    std::string msg;
    while (!message_queue_.empty()) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (message_queue_.empty()) break;
            msg = message_queue_.front();
            message_queue_.pop();
        }

        check_and_rotate();
        log_stream_ << msg << std::endl;
    }
    log_stream_.flush();
}

void Logger::log_worker_thread() {
    while (is_running_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this] { return !message_queue_.empty() || !is_running_; });

        lock.unlock();
        flush_to_file();
    }

    flush_to_file();  // 最后一次刷新
}
```

## 3.3 修改 Makefile

更新 `SOURCES`：

```makefile
SOURCES = $(SRC_DIR)/main.cpp config/config.cpp log/log.cpp
```

## 3.4 更新 main.cpp

```cpp
#include "config/config.h"
#include "log/log.h"
#include <unistd.h>

int main(int argc, char* argv[]) {
    // 解析配置
    Config& config = Config::GetInstance();
    config.parse_command_line(argc, argv);

    // 初始化日志系统
    Logger& logger = Logger::GetInstance();
    logger.init(config.log_file, (LogLevel)config.log_level);

    // 记录启动信息
    LOG_INFO("=== WebServer Starting ===");
    config.print_config();

    // 占位循环
    LOG_INFO("Placeholder loop. Press Ctrl+C to exit.");
    while (true) {
        sleep(1);
    }

    logger.shutdown();
    return 0;
}
```

## 3.5 编译与测试

```bash
make clean
make
./bin/server -p 8080 -t 4 -l 0  # 日志级别 0=DEBUG
```

验证日志文件是否生成：

```bash
cat log/webserver.log
```

---

---

# 【第 4 步】缓冲区模块

## 目标
实现动态可扩展的字节缓冲区，用于读取网络数据和缓存响应数据，支持零拷贝操作。

## 4.1 创建 buffer/buffer.h

```cpp
#ifndef BUFFER_H
#define BUFFER_H

#include <string>
#include <vector>
#include <cassert>
#include <cstring>

class Buffer {
public:
    static const size_t INITIAL_CAPACITY = 1024;

    // 构造函数
    Buffer(size_t initial_capacity = INITIAL_CAPACITY);

    // 获取可读数据大小
    size_t readable_size() const { return write_idx_ - read_idx_; }

    // 获取可写空间大小
    size_t writable_size() const { return buffer_.size() - write_idx_; }

    // 获取已使用缓冲大小
    size_t used_size() const { return write_idx_; }

    // 获取总容量
    size_t capacity() const { return buffer_.size(); }

    // 获取可读数据指针
    const char* peek() const { return buffer_.data() + read_idx_; }

    // 获取可写数据指针
    char* begin_write() { return buffer_.data() + write_idx_; }

    // 追加数据
    void append(const char* data, size_t len);
    void append(const std::string& str) { append(str.data(), str.size()); }

    // 读取数据
    std::string get_read_str(size_t len) {
        assert(len <= readable_size());
        std::string result(peek(), len);
        read_idx_ += len;
        return result;
    }

    // 跳过数据
    void skip(size_t len) {
        assert(len <= readable_size());
        read_idx_ += len;
    }

    // 检查是否包含某个字符串
    bool contains_substr(const std::string& substr) const;

    // 查找字符串位置
    size_t find_substr(const std::string& substr) const;

    // 从 socket 读取数据
    ssize_t read_from_socket(int fd);

    // 从 socket 写入数据
    ssize_t send_to_socket(int fd);

    // 重置缓冲区
    void reset() { read_idx_ = write_idx_ = 0; }

    // 清空缓冲区
    void clear() {
        read_idx_ = write_idx_ = 0;
        buffer_.clear();
        buffer_.resize(INITIAL_CAPACITY);
    }

    // 整理缓冲区（移除已读数据）
    void compact();

    // 扩容缓冲区
    void expand(size_t len);

private:
    std::vector<char> buffer_;
    size_t read_idx_;   // 读指针
    size_t write_idx_;  // 写指针
};

#endif  // BUFFER_H
```

## 4.2 创建 buffer/buffer.cpp

```cpp
#include "buffer.h"
#include <unistd.h>
#include <sys/uio.h>
#include <errno.h>

Buffer::Buffer(size_t initial_capacity) 
    : read_idx_(0), write_idx_(0) {
    buffer_.resize(initial_capacity);
}

void Buffer::append(const char* data, size_t len) {
    if (len == 0) return;

    if (writable_size() < len) {
        expand(len);
    }

    std::memcpy(begin_write(), data, len);
    write_idx_ += len;
}

bool Buffer::contains_substr(const std::string& substr) const {
    return find_substr(substr) != std::string::npos;
}

size_t Buffer::find_substr(const std::string& substr) const {
    const char* pos = std::strstr(peek(), substr.c_str());
    if (pos == nullptr) return std::string::npos;
    return pos - peek();
}

ssize_t Buffer::read_from_socket(int fd) {
    // 使用 readv 避免数据拷贝：先读入当前可写位置，
    // 如果满了再读入临时缓冲
    char temp_buf[65536];
    struct iovec iov[2];
    size_t writable = writable_size();

    iov[0].iov_base = begin_write();
    iov[0].iov_len = writable;
    iov[1].iov_base = temp_buf;
    iov[1].iov_len = sizeof(temp_buf);

    ssize_t n = ::readv(fd, iov, 2);
    if (n < 0) {
        return -1;
    }

    if (n <= (ssize_t)writable) {
        write_idx_ += n;
    } else {
        write_idx_ += writable;
        append(temp_buf, n - writable);
    }

    return n;
}

ssize_t Buffer::send_to_socket(int fd) {
    ssize_t n = ::write(fd, peek(), readable_size());
    if (n > 0) {
        skip(n);
    }
    return n;
}

void Buffer::compact() {
    if (read_idx_ > 0) {
        size_t readable = readable_size();
        if (readable > 0) {
            std::memmove(buffer_.data(), peek(), readable);
        }
        read_idx_ = 0;
        write_idx_ = readable;
    }
}

void Buffer::expand(size_t len) {
    if (writable_size() >= len) {
        compact();  // 先尝试整理
    }

    if (writable_size() < len) {
        // 需要扩容
        size_t new_capacity = buffer_.size() + std::max(len, buffer_.size());
        buffer_.resize(new_capacity);
    }
}
```

## 4.3 修改 Makefile

```makefile
SOURCES = $(SRC_DIR)/main.cpp config/config.cpp log/log.cpp buffer/buffer.cpp
```

---

# 【第 5 步】HTTP 请求解析

## 目标
实现 HTTP 请求的状态机解析，支持 GET/POST 方法、头部解析、URL 参数等。

## 5.1 创建 http/httprequest.h

```cpp
#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <map>
#include <memory>
#include "../buffer/buffer.h"

enum class HttpMethod {
    GET,
    POST,
    HEAD,
    PUT,
    DELETE,
    UNKNOWN
};

enum class HttpVersion {
    HTTP_1_0,
    HTTP_1_1,
    UNKNOWN
};

enum class ParseState {
    REQUEST_LINE,
    HEADERS,
    BODY,
    FINISH
};

class HttpRequest {
public:
    HttpRequest() : method_(HttpMethod::UNKNOWN), version_(HttpVersion::HTTP_1_1), 
                    state_(ParseState::REQUEST_LINE), body_len_(0) {}

    // 解析请求
    bool parse(Buffer& buf);

    // 获取请求方法
    HttpMethod method() const { return method_; }
    std::string method_str() const;

    // 获取请求路径
    const std::string& path() const { return path_; }

    // 获取请求版本
    HttpVersion version() const { return version_; }

    // 获取请求头
    const std::map<std::string, std::string>& headers() const { return headers_; }

    // 获取特定请求头
    std::string get_header(const std::string& key) const;

    // 获取请求体
    const std::string& body() const { return body_; }

    // 获取请求状态
    ParseState state() const { return state_; }

    // 检查是否保持连接
    bool keep_alive() const;

    // 重置请求
    void reset();

private:
    // 解析请求行
    bool parse_request_line(const std::string& line);

    // 解析头部
    bool parse_headers(Buffer& buf);

    // 解析请求体
    bool parse_body(Buffer& buf);

    HttpMethod method_;
    HttpVersion version_;
    std::string path_;
    std::string query_;
    std::map<std::string, std::string> headers_;
    std::string body_;
    ParseState state_;
    size_t body_len_;
};

#endif  // HTTP_REQUEST_H
```

## 5.2 创建 http/httprequest.cpp

```cpp
#include "httprequest.h"
#include <sstream>
#include <algorithm>
#include <cctype>

std::string HttpRequest::method_str() const {
    switch (method_) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        default: return "UNKNOWN";
    }
}

std::string HttpRequest::get_header(const std::string& key) const {
    auto it = headers_.find(key);
    if (it != headers_.end()) {
        return it->second;
    }
    return "";
}

bool HttpRequest::keep_alive() const {
    std::string connection = get_header("connection");
    // 转小写
    std::transform(connection.begin(), connection.end(), 
                   connection.begin(), ::tolower);
    
    if (connection == "keep-alive") return true;
    if (version_ == HttpVersion::HTTP_1_1) return true;
    return false;
}

void HttpRequest::reset() {
    method_ = HttpMethod::UNKNOWN;
    version_ = HttpVersion::HTTP_1_1;
    path_.clear();
    query_.clear();
    headers_.clear();
    body_.clear();
    state_ = ParseState::REQUEST_LINE;
    body_len_ = 0;
}

bool HttpRequest::parse(Buffer& buf) {
    bool ok = true;

    while (state_ != ParseState::FINISH) {
        switch (state_) {
            case ParseState::REQUEST_LINE: {
                size_t pos = buf.find_substr("\r\n");
                if (pos == std::string::npos) return ok;

                std::string line = buf.get_read_str(pos);
                buf.skip(2);  // 跳过 \r\n

                if (!parse_request_line(line)) {
                    return false;
                }
                state_ = ParseState::HEADERS;
                break;
            }
            case ParseState::HEADERS: {
                if (!parse_headers(buf)) {
                    return ok;
                }
                state_ = ParseState::BODY;
                break;
            }
            case ParseState::BODY: {
                if (!parse_body(buf)) {
                    return ok;
                }
                state_ = ParseState::FINISH;
                break;
            }
            case ParseState::FINISH:
            default:
                break;
        }
    }

    return ok;
}

bool HttpRequest::parse_request_line(const std::string& line) {
    std::istringstream iss(line);
    std::string method_str, path, version_str;

    if (!(iss >> method_str >> path >> version_str)) {
        return false;
    }

    // 解析方法
    if (method_str == "GET") method_ = HttpMethod::GET;
    else if (method_str == "POST") method_ = HttpMethod::POST;
    else if (method_str == "HEAD") method_ = HttpMethod::HEAD;
    else if (method_str == "PUT") method_ = HttpMethod::PUT;
    else if (method_str == "DELETE") method_ = HttpMethod::DELETE;
    else return false;

    // 解析版本
    if (version_str == "HTTP/1.0") version_ = HttpVersion::HTTP_1_0;
    else if (version_str == "HTTP/1.1") version_ = HttpVersion::HTTP_1_1;
    else return false;

    // 解析路径和查询参数
    size_t query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        path_ = path.substr(0, query_pos);
        query_ = path.substr(query_pos + 1);
    } else {
        path_ = path;
    }

    return true;
}

bool HttpRequest::parse_headers(Buffer& buf) {
    size_t pos = buf.find_substr("\r\n");
    while (pos != std::string::npos) {
        std::string line = buf.get_read_str(pos);
        buf.skip(2);  // 跳过 \r\n

        if (line.empty()) {
            // 空行表示头部结束
            return true;
        }

        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            return false;
        }

        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);

        // 移除前后空格
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // 转换为小写便于比较
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        headers_[key] = value;

        if (key == "content-length") {
            try {
                body_len_ = std::stoul(value);
            } catch (...) {
                return false;
            }
        }

        pos = buf.find_substr("\r\n");
    }

    return false;  // 未找到空行
}

bool HttpRequest::parse_body(Buffer& buf) {
    if (method_ == HttpMethod::GET || method_ == HttpMethod::HEAD) {
        return true;  // GET/HEAD 没有请求体
    }

    if (body_len_ == 0) {
        return true;
    }

    if (buf.readable_size() < body_len_) {
        return false;  // 数据未到达
    }

    body_ = buf.get_read_str(body_len_);
    return true;
}
```

## 5.3 修改 Makefile

```makefile
SOURCES = $(SRC_DIR)/main.cpp config/config.cpp log/log.cpp buffer/buffer.cpp http/httprequest.cpp
```

---

# 【第 6 步】HTTP 响应构造

## 目标
实现 HTTP 响应的构造，支持状态码、头部、文件映射等。

## 6.1 创建 http/httpresponse.h

```cpp
#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>
#include <map>
#include <memory>
#include "../buffer/buffer.h"

class HttpResponse {
public:
    enum class StatusCode {
        OK = 200,
        BAD_REQUEST = 400,
        FORBIDDEN = 403,
        NOT_FOUND = 404,
        INTERNAL_ERROR = 500
    };

    HttpResponse(StatusCode code = StatusCode::OK, bool keep_alive = true);

    // 设置状态码
    void set_status(StatusCode code) { status_code_ = code; }

    // 添加响应头
    void add_header(const std::string& key, const std::string& value);

    // 设置响应体（字符串）
    void set_body(const std::string& body);

    // 设置响应体（文件）
    void set_file_body(const std::string& filepath);

    // 将响应序列化到缓冲区
    void to_buffer(Buffer& buf);

    // 获取响应状态
    StatusCode status() const { return status_code_; }

    // 获取文件路径（如果有的话）
    const std::string& file_path() const { return file_path_; }

    // 检查是否有文件体
    bool has_file_body() const { return !file_path_.empty(); }

private:
    std::string get_status_message(StatusCode code);
    std::string get_mime_type(const std::string& filepath);
    std::string get_file_ext(const std::string& filepath);

    StatusCode status_code_;
    std::map<std::string, std::string> headers_;
    std::string body_;
    std::string file_path_;
    bool keep_alive_;
};

#endif  // HTTP_RESPONSE_H
```

## 6.2 创建 http/httpresponse.cpp

```cpp
#include "httpresponse.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>

HttpResponse::HttpResponse(StatusCode code, bool keep_alive)
    : status_code_(code), keep_alive_(keep_alive) {}

void HttpResponse::add_header(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

void HttpResponse::set_body(const std::string& body) {
    body_ = body;
    add_header("Content-Length", std::to_string(body.size()));
    add_header("Content-Type", "text/html; charset=utf-8");
}

void HttpResponse::set_file_body(const std::string& filepath) {
    file_path_ = filepath;
    
    // 检查文件是否存在
    struct stat file_stat;
    if (stat(filepath.c_str(), &file_stat) < 0) {
        status_code_ = StatusCode::NOT_FOUND;
        set_body("<html><body><h1>404 Not Found</h1></body></html>");
        return;
    }

    size_t file_size = file_stat.st_size;
    add_header("Content-Length", std::to_string(file_size));
    add_header("Content-Type", get_mime_type(filepath));
}

void HttpResponse::to_buffer(Buffer& buf) {
    // 添加状态行
    std::string status_line = "HTTP/1.1 " + std::to_string((int)status_code_) + 
                             " " + get_status_message(status_code_) + "\r\n";
    buf.append(status_line.c_str(), status_line.size());

    // 添加必要的头部
    add_header("Server", "WebServer/1.0");
    if (keep_alive_) {
        add_header("Connection", "keep-alive");
    } else {
        add_header("Connection", "close");
    }

    // 添加所有头部
    for (const auto& [key, value] : headers_) {
        std::string header_line = key + ": " + value + "\r\n";
        buf.append(header_line.c_str(), header_line.size());
    }

    // 空行表示头部结束
    buf.append("\r\n", 2);

    // 添加响应体（如果是字符串）
    if (!body_.empty()) {
        buf.append(body_.c_str(), body_.size());
    }
}

std::string HttpResponse::get_status_message(StatusCode code) {
    switch (code) {
        case StatusCode::OK: return "OK";
        case StatusCode::BAD_REQUEST: return "Bad Request";
        case StatusCode::FORBIDDEN: return "Forbidden";
        case StatusCode::NOT_FOUND: return "Not Found";
        case StatusCode::INTERNAL_ERROR: return "Internal Server Error";
        default: return "Unknown";
    }
}

std::string HttpResponse::get_mime_type(const std::string& filepath) {
    std::string ext = get_file_ext(filepath);
    
    static const std::map<std::string, std::string> mime_map = {
        {".html", "text/html; charset=utf-8"},
        {".htm", "text/html; charset=utf-8"},
        {".txt", "text/plain; charset=utf-8"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".mp4", "video/mp4"},
        {".webm", "video/webm"},
        {".pdf", "application/pdf"},
        {".zip", "application/zip"},
    };

    auto it = mime_map.find(ext);
    if (it != mime_map.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

std::string HttpResponse::get_file_ext(const std::string& filepath) {
    size_t pos = filepath.find_last_of('.');
    if (pos == std::string::npos) {
        return "";
    }
    std::string ext = filepath.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}
```

## 6.3 修改 Makefile

```makefile
SOURCES = $(SRC_DIR)/main.cpp config/config.cpp log/log.cpp buffer/buffer.cpp \
          http/httprequest.cpp http/httpresponse.cpp
```

---

# 后续步骤预告（第 7-20 步）

以下步骤将逐步提供详细代码（请告知我继续）：

- **第 7 步**: HTTP 连接封装（`http/httpconn.*`）
- **第 8 步**: epoll 事件循环封装（`server/epoller.*`）
- **第 9 步**: 线程池实现（`pool/threadpool.*`）
- **第 10 步**: 定时器模块（`timer/heaptimer.*`）
- **第 11 步**: 数据库连接池（`pool/sqlconnpool.*`）
- **第 12 步**: WebServer 主逻辑（`server/webserver.*`）
- **第 13 步**: 静态资源与路由处理
- **第 14 步**: 错误页面与 MIME 类型
- **第 15 步**: 集成测试
- **第 16 步**: 性能优化
- **第 17 步**: 安全加固
- **第 18 步**: 文档编写

---

## 现在可以做的事

按照上面第 1-6 步，逐步编写代码。每一步都包含：
- 完整的头文件（`.h`）
- 完整的实现文件（`.cpp`）
- Makefile 修改说明
- 编译与测试步骤

**建议**：
1. 先完成第 1 步，确保能编译
2. 然后逐个添加第 2-6 步的文件
3. 每一步都编译验证
4. 当你完成某一步后，告诉我，我将继续提供下一步详细代码
