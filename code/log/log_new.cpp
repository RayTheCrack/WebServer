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
    last_flush_time_ = std::chrono::steady_clock::now(); // 初始化上次刷新时间
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

void Logger::log(LogLevel level, const Buffer& buffer) {
    if(level < current_level_) return; // 级别过低，不记录
    // 如果日志系统未初始化或已关闭，直接输出到标准输出（避免丢失重要日志）
    if (!is_running_.load()) {
        if (level >= current_level_) {
            std::string timestamp = get_timestamp();
            std::string level_name = get_level_name(level);
            std::string line = timestamp + " [" + level_name + "] ";
            std::fwrite(line.data(), 1, line.size(), stdout);
            std::fwrite(buffer.peek(), 1, buffer.readable_size(), stdout);
            std::fwrite("\n", 1, 1, stdout);
        }
        return;
    }
    // 将Buffer内容转换为字符串放入队列
    std::string timestamp = get_timestamp();
    std::string level_name = get_level_name(level);
    std::string line = timestamp + " [" + level_name + "] " + std::string(buffer.peek(), buffer.readable_size());
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
        if(!ok) {
            // 即使没有新日志，也要检查是否需要刷新缓冲区
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_flush_time_).count();
            if(elapsed >= LEAST_FLUSH_SEC_GAP && write_buffer_.readable_size() > 0) {
                if(log_stream_.is_open()) {
                    log_stream_.write(write_buffer_.peek(), write_buffer_.readable_size());
                    log_stream_.flush();
                } else {
                    std::fwrite(write_buffer_.peek(), 1, write_buffer_.readable_size(), stdout);
                }
                write_buffer_.reset(); // 重置缓冲区
                last_flush_time_ = std::chrono::steady_clock::now(); // 更新上次刷新时间
            }
            continue; // 超时或队列已停止，继续检查is_running_标志
        }
        // 将日志条目追加到写入缓冲区
        write_buffer_.append(entry.c_str(), entry.size());
        write_buffer_.append("\n", 1);

        // 检查是否需要强制刷新（超时或缓冲区满）
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_flush_time_).count();
        bool need_flush = (write_buffer_.readable_size() >= 4096) or (elapsed >= LEAST_FLUSH_SEC_GAP);

        if(need_flush) {
            if(log_stream_.is_open())
            {
                log_stream_.write(write_buffer_.peek(), write_buffer_.readable_size());
                log_stream_.flush();
            }
            else std::fwrite(write_buffer_.peek(), 1, write_buffer_.readable_size(), stdout);
            write_buffer_.reset(); // 重置缓冲区
            last_flush_time_ = std::chrono::steady_clock::now(); // 更新上次刷新时间
        }
    }
    // 确保所有日志被写入文件
    if(write_buffer_.readable_size() > 0)
    {
        if(log_stream_.is_open())
        {
            log_stream_.write(write_buffer_.peek(), write_buffer_.readable_size());
            log_stream_.flush();
        }
        else std::fwrite(write_buffer_.peek(), 1, write_buffer_.readable_size(), stdout);
        write_buffer_.reset();
    }
}
