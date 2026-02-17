#include "buffer.h"
#include <unistd.h>
#include <sys/uio.h>
#include <errno.h>

void Buffer::append(const char* data, size_t len) {
    if(len == 0) return; // 无数据可追加
    if(writable_size() < len) expand(len); // 如果可写空间不足，扩展缓冲区
    std::copy(data, data + len, begin_write()); // 将数据复制到可写空间
    write_ptr_ += len; // 更新写指针位置
}

bool Buffer::contains(const std::string& str) const {
    return find_substr(str) != std::string::npos; // 如果find_substr返回npos，表示未找到
}

size_t Buffer::find_substr(const std::string& substr) const {
    const char* pos = std::strstr(peek(), substr.c_str());
    if(pos == nullptr) return std::string::npos; // 未找到
    return pos - peek(); // 返回子串在可读数据中的位置
}

ssize_t Buffer::read_from_socket(int fd) {
    // 使用 readv 避免数据拷贝：先读入当前可写位置，
    // 如果满了再读入临时缓冲
    char temp_buffer[65536]; // 64KB 临时缓冲
    struct iovec iov[2];

    size_t writable = writable_size();  
    //  调用 readv 读取数据
    // 数据会先填充满 iov[0]（Buffer 自身缓冲区）
    // 剩余数据会填充到 iov[1]（临时缓冲）
    iov[0].iov_base = begin_write(); // 可写空间
    iov[0].iov_len = writable_size();

    iov[1].iov_base = temp_buffer; // 临时缓冲
    iov[1].iov_len = sizeof(temp_buffer); // 临时缓冲大小
    // ::表示调用全局命名空间的readv，避免潜在的成员函数冲突
    ssize_t n = ::readv(fd, iov, 2); // 读取数据
    if(n < 0) return -1; // 读取失败
    if(n <= static_cast<ssize_t>(writable)) write_ptr_ += n;
    else {
        write_ptr_ = buffer_.size(); // 可写空间已满
        append(temp_buffer, n - writable); // 将剩余数据追加到缓冲区
    }
    return n;
}

ssize_t Buffer::write_to_socket(int fd) {
    ssize_t n = ::write(fd, peek(), readable_size()); // 发送数据
    if(n > 0) skip(n); // 更新读指针位置，表示已发送的数据
    return n;
}

void Buffer::compact() {
    if(read_ptr_ > 0){
        size_t readable = readable_size();
        // 从peek()位置开始，将可读数据移动到缓冲区开始位置（buffer_.data()）
        std::memmove(buffer_.data(), peek(), readable); // 将未读数据移动到缓冲区开始位置
        read_ptr_ = 0;
        write_ptr_ = readable;
    }
}

void Buffer::expand(size_t len) {
    if(writable_size() >= len) compact(); // 如果通过整理可以腾出足够空间，先整理
    if(writable_size() >= len) return; // 整理后如果仍然不足，才扩展
    int new_cap = buffer_.size() + std::max(len, buffer_.size()); // 扩展至少当前容量的两倍
    buffer_.resize(new_cap);
}