#ifndef BUFFER_H
#define BUFFER_H

#include <string>
#include <vector>
#include <iostream>
#include <cstring>
#include <cassert>
/*
    Buffer 类用于管理缓冲区，实现自动扩容，避免频繁分配内存，并实现空间复用
    并且提供常用的缓冲区操作，如读取、写入、查找等
    适用于需要高效处理大量数据的场景，如网络编程、文件读写等
*/
class Buffer { 

public:
    static const size_t INITIAL_CAPACITY = 1024; // 初始缓冲区大小

    Buffer(size_t cap = INITIAL_CAPACITY) : buffer_(cap), read_ptr_(0), write_ptr_(0) {}

    // 获取可读数据的长度
    size_t readable_size() const {
        return write_ptr_ - read_ptr_;
    }

    // 获取可写空间的长度
    size_t writable_size() const {
        return buffer_.size() - write_ptr_;
    }

    // 获取已使用的缓冲大小
    size_t used_size() const {
        return write_ptr_;
    }

    // 获取缓冲区总容量
    size_t capacity() const {
        return buffer_.size();
    }

    // 获取可读数据的指针
    const char* peek() const {
        return buffer_.data() + read_ptr_; // 返回指向可读数据的指针
    }

    // 获取可写数据的指针
    char* begin_write() {
        return buffer_.data() + write_ptr_; // 返回指向可写空间的指针
    }
    // const 版本，用于只读操作，相当于readable_end()
    const char* begin_write_const() const {
        return buffer_.data() + write_ptr_;
    }

    // 追加数据到缓冲区
    void append(const char* data, size_t len);

    void append(const std::string& str) {
        append(str.data(), str.size());
    }

    // 读取数据
    std::string retrieve(size_t len) {
        assert(len <= readable_size()); // 确保读取长度不超过可读数据长度
        std::string result(peek(), len);
        read_ptr_ += len; // 更新读指针位置
        return result;
    }   

    // 读取数据直到某个指针位置
    void retrieve_until(const char* end) {
        assert(end >= peek() and end <= begin_write_const());
        retrieve(end - peek());
    }
    
    // 跳过数据
    void skip(size_t len) {
        assert(len <= readable_size()); // 确保跳过长度不超过可读数据长度
        read_ptr_ += len; // 更新读指针位置
    }

    // 检查是否包含某个字符串
    bool contains(const std::string& str) const;

    // 查找字符串位置
    size_t find_substr(const std::string& substr) const;

    // 从 socket 读取数据到缓冲区
    ssize_t read_from_socket(int fd);

    // 从 socket 写入数据到缓冲区
    ssize_t write_to_socket(int fd);

    // 重置缓冲区
    void reset() { read_ptr_ = 0; write_ptr_ = 0; }

    // 清空缓冲区
    void clear() {
        read_ptr_ = 0;
        write_ptr_ = 0;
        buffer_.clear();
        buffer_.resize(INITIAL_CAPACITY); // 重置为初始容量
    }

    // 整理缓冲区，移动未读数据到缓冲区开始位置
    void compact();

    // 扩展缓冲区容量
    void expand(size_t len);

private:
    std::vector<char> buffer_; // 内部缓冲区
    size_t read_ptr_; // 下一个可读位置
    size_t write_ptr_; // 下一个可写位置
};

#endif /* BUFFER_H */


