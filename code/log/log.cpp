#include "log.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <filesystem>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::initLogger(const std::string& log_file, LogLevel level, int max_queue_size) {
    if (is_running_.load()) {
        return; // 已经初始化
    }
    current_level_ = level;
    log_file_ = log_file;

    std::filesystem::path log_path(log_file_);
    // 确保日志目录存在，如果不存在则创建
    if(log_path.has_parent_path()) {
        std::filesystem::create_directories(log_path.parent_path());
    }

    current_date_ = get_timestamp().substr(0, 10); // 取日期部分 YYYY-MM-DD
    if(!log_file.empty()) log_stream_.open(log_file_, std::ios::app | std::ios::out);
 
    message_queue_ = std::make_unique<BlockDeque<std::string>>(max_queue_size);
    is_running_.store(true);
    worker_thread_ = std::thread(&Logger::log_worker_thread, this);
}

Logger::~Logger() {
    shutdown();
}

void Logger::shutdown() {
    if(!is_running_.load()) return; // 已经关闭
    is_running_.store(false);
    if(message_queue_) message_queue_->stop(); // 停止队列，唤醒所有等待的线程
    if(worker_thread_.joinable()) worker_thread_.join(); // 等待刷盘线程退出
    if(log_stream_.is_open()) log_stream_.close(); // 关闭日志文件流
}

void Logger::setLogLevel(LogLevel level) {
    current_level_ = level;
}

LogLevel Logger::getLogLevel() const {
    return current_level_;
}

void Logger::log(LogLevel level, const std::string& msg) {
    if(level < current_level_) return; // 级别过低，不记录
    // 如果日志系统未初始化或已关闭，直接输出到标准输出（避免丢失重要日志）
    if (!is_running_.load()) {
        if (level >= current_level_) {
            std::string line = get_timestamp() + " [" + get_level_name(level) + "] " + msg + "\n";
            std::fwrite(line.data(), 1, line.size(), stdout);
        }
        return;
    }
    std::string line = get_timestamp() + " [" + get_level_name(level) + "] " + msg;
    message_queue_->push_back(std::move(line)); // 将日志消息放入队列，异步写入
}

std::string Logger::get_level_name(LogLevel level) {
    switch(level) 
    {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    // 将时间点转换为time_t以便格式化
    time_t time_t_now = std::chrono::system_clock::to_time_t(now);
    // 毫秒时间只能通过duration_cast计算
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm_now;
    // localtime : 将time_t转换为本地时间的tm结构
    localtime_r(&time_t_now, &tm_now); // 线程安全的localtime
    std::ostringstream oss;
    // 格式化输出时间戳 YYYY-MM-DD HH:MM:SS.mmm
    oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3) << millis.count();
    return oss.str();
}

void Logger::log_worker_thread() {
    std::string entry;
    while(is_running_.load() or (message_queue_ and message_queue_->size())) {
        if(!message_queue_) break; // 队列未初始化，退出线程
        bool ok = message_queue_->pop(entry, 1); // 带超时的pop，避免无限阻塞
        if(!ok) continue; // 超时或队列已停止，继续检查is_running_标志
        if(log_stream_.is_open())
            log_stream_ << entry << std::endl; // 写入日志文件
        else 
            std::fwrite((entry + "\n").data(), 1, entry.size() + 1, stdout); // 如果文件流不可用，输出到标准输出
    }
    if(log_stream_.is_open()) log_stream_.flush(); // 确保所有日志被写入文件
}

// // 格式化字符串函数，类似于 printf 风格
// std::string format_string(const std::string& fmt, ...) {
//     // 步骤1：初始化可变参数列表
//     va_list ap;
//     va_start(ap, fmt);  // ap绑定到最后一个固定参数fmt，指向第一个可变参数
//     // 步骤2：拷贝参数列表（关键！因为vsnprintf会消费va_list）
//     va_list ap2;
//     va_copy(ap2, ap);   // 拷贝ap到ap2，后续分两次使用
//     // 步骤3：计算格式化字符串所需的字节数（不包含末尾的'\0'）
//     const int needed = std::vsnprintf(nullptr, 0, fmt.c_str(), ap);
//     // 说明：
//     // - nullptr：不写入任何字符，仅计算所需长度
//     // - 0：写入长度为0
//     // - 返回值needed：格式化后的字符串总字节数（不含'\0'）
//     // 步骤4：创建足够大的缓冲区（+1是为了容纳末尾的'\0'）
//     std::string buf(needed + 1, '\0');  // 初始化为needed+1个'\0'
//     // 步骤5：真正执行格式化，写入缓冲区
//     std::vsnprintf(&buf[0], buf.size(), fmt.c_str(), ap2);
//     // 说明：
//     // - &buf[0]：获取string底层字符数组的首地址
//     // - buf.size()：缓冲区大小（needed+1），避免缓冲区溢出
//     // - ap2：拷贝后的参数列表（ap已被第一步vsnprintf消费，无法复用）
//     // 步骤6：清理可变参数列表（必须配对va_start/va_copy）
//     va_end(ap2);
//     va_end(ap);
//     // 步骤7：去掉末尾的'\0'，返回纯格式化字符串
//     buf.resize(needed);
//     return buf;
// }