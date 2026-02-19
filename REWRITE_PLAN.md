<<<<<<< HEAD
# WebServer 项目重写文档（从第 6 步开始）

## 当前进度总结

✅ 已完成模块：
- 第 1 步：项目骨架与构建系统
- 第 2 步：配置模块（Config）
- 第 3 步：缓冲区模块（Buffer）
- 第 4 步：阻塞队列（BlockDeque）
- 第 5 步：日志模块（Log）

## 【第 6 步】HTTP 请求解析（HttpRequest）

### 目标
实现 HTTP 请求解析器，能够解析请求行、请求头和请求体。

### 6.1 创建 `code/http/httprequest.h`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>
#include "../buffer/buffer.h"

class HttpRequest {
public:
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,        
    };

    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };
    
    HttpRequest() { Init(); }
    ~HttpRequest() = default;

    void Init();
    bool parse(Buffer& buff);
    
    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;

private:
    bool ParseRequestLine_(const std::string& line);
    void ParseHeader_(const std::string& line);
    void ParseData_(const std::string& line);
    void ParsePath_();
    void ParsePost_();
    static int ConverHex(char ch);

    PARSE_STATE state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;
    static const std::unordered_set<std::string> DEFAULT_HTML;
    static int ConverHex(char ch);
};

#endif //HTTPREQUEST_H
```

### 6.2 创建 `code/http/httprequest.cpp`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#include "httprequest.h"
#include <algorithm>

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML {
    "/index", "/register", "/login",
    "/welcome", "/video", "/picture",
};

void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";
    if(buff.ReadableBytes() <= 0) {
        return false;
    }
    while(buff.ReadableBytes() && state_ != FINISH) {
        const char* lineEnd = std::search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line(buff.Peek(), lineEnd);
        switch(state_) {
        case REQUEST_LINE:
            if(!ParseRequestLine_(line)) {
                return false;
            }
            ParsePath_();
            break;    
        case HEADERS:
            ParseHeader_(line);
            if(buff.ReadableBytes() <= 2) {
                state_ = FINISH;
            }
            break;
        case BODY:
            ParseBody_(line);
            break;
        }
        if(lineEnd == buff.BeginWrite()) { break; }
        buff.RetrieveUntil(lineEnd + 2);
    }
    return true;
}

bool HttpRequest::ParseRequestLine_(const std::string& line) {
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch subMatch;
    if(std::regex_match(line, subMatch, patten)) {   
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

void HttpRequest::ParseHeader_(const std::string& line) {
    std::regex patten("^([^:]*): ?(.*)$");
    std::smatch subMatch;
    if(std::regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];
    }
    else {
        state_ = BODY;
    }
}

void HttpRequest::ParseBody_(const std::string& line) {
    body_ = line;
    ParsePost_();
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

void HttpRequest::ParsePath_() {
    if(path_ == "/") {
        path_ = "/index.html"; 
    }
    else {
        for(auto &item: DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

void HttpRequest::ParsePost_() {
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();
        if(DEFAULT_HTML.find(path_) != DEFAULT_HTML.end()) {
            int tag = 1;
            if(user_.find("name") != user_.end() && user_.find("password") != user_.end()) {
                tag = VerifyUser(user_["name"], user_["password"]);
            }
            if(tag == 1) {
                path_ = "/welcome.html";
            }
            else {
                path_ = "/error.html";
            }
        }
    }   
}

int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

void HttpRequest::ParseFromUrlencoded_() {
    if(body_.size() == 0) { return; }

    std::string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for(; i < n; i++) {
        char ch = body_[i];
        switch (ch) {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            break;
        }
    }
    if(j < n) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

std::string HttpRequest::path() const {
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}

std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}
```

### 6.3 更新 Makefile

修改 `SOURCES` 行：
```makefile
SOURCES = $(SRC_DIR)/main.cpp config/config.cpp buffer/buffer.cpp log/log.cpp http/httprequest.cpp
```

### 6.4 编译测试

```bash
make clean
make
```

## 【第 7 步】HTTP 响应构造（HttpResponse）

### 目标
实现 HTTP 响应构造器，能够生成状态行、响应头和响应体。

### 7.1 创建 `code/http/httpresponse.h`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <unordered_map>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string>
#include "../buffer/buffer.h"

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse() = default;

    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    void MakeResponse(Buffer& buff);
    void UnmapFile();
    char* File();
    size_t FileLen() const;
    void ErrorContent(Buffer& buff, std::string message);
    int Code() const { return code_; }

private:
    void AddStateLine_(Buffer& buff);
    void AddHeader_(Buffer& buff);
    void AddContent_(Buffer& buff);
    void ErrorHtml_();
    std::string GetFileType_();

    int code_;
    bool isKeepAlive_;
    std::string path_;
    std::string srcDir_;
    
    char* mmFile_; 
    struct stat mmFileStat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    static const std::unordered_map<int, std::string> CODE_PATH;
};
#endif //HTTPRESPONSE_H
```

### 7.2 创建 `code/http/httpresponse.cpp`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#include "httpresponse.h"

const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/msword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 500, "Internal Server Error" },
};

const std::unordered_map<int, std::string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
    { 500, "/500.html" },
};

HttpResponse::HttpResponse() {
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
};

void HttpResponse::Init(const std::string& srcDir, std::string& path, bool isKeepAlive, int code){
    assert(srcDir != "");
    if(mmFile_) { UnmapFile(); }
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
}

void HttpResponse::MakeResponse(Buffer& buff) {
    if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        code_ = 404;
    }
    else if(!(mmFileStat_.st_mode & S_IROTH)) {
        code_ = 403;
    }
    else if(code_ == -1) {
        code_ = 200; 
    }
    ErrorHtml_();
    AddStateLine_(buff);
    AddHeader_(buff);
    AddContent_(buff);
}

char* HttpResponse::File() {
    return mmFile_;
}

size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size;
}

void HttpResponse::ErrorHtml_() {
    if(CODE_PATH.count(code_) == 1) {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

void HttpResponse::AddStateLine_(Buffer& buff) {
    std::string status;
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    }
    else {
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.Append("HTTP/1.1 " + std::to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader_(Buffer& buff) {
    buff.Append("Connection: ");
    if(isKeepAlive_) {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    } else{
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType_() + "\r\n");
}

void HttpResponse::AddContent_(Buffer& buff) {
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if(srcFd < 0) {
        ErrorContent(buff, "File NotFound!");
        return;
    }

    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1) {
        ErrorContent(buff, "File NotFound!");
        return;
    }
    mmFile_ = (char*)mmRet;
    close(srcFd);
    buff.Append("Content-length: " + std::to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

void HttpResponse::UnmapFile() {
    if(mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

std::string HttpResponse::GetFileType_() {
    std::string::size_type idx = path_.find_last_of('.');
    if(idx == std::string::npos) {
        return "text/plain";
    }
    std::string suffix = path_.substr(idx);
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}

void HttpResponse::ErrorContent(Buffer& buff, std::string message) {
    std::string body;
    std::string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request";
    }
    body += std::to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>WebServer</em></body></html>";
    buff.Append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}
```

### 7.3 更新 Makefile

修改 `SOURCES` 行：
```makefile
SOURCES = $(SRC_DIR)/main.cpp config/config.cpp buffer/buffer.cpp log/log.cpp http/httprequest.cpp http/httpresponse.cpp
```

### 7.4 编译测试

```bash
make clean
make
```

## 【第 8 步】HTTP 连接封装（HttpConn）

### 目标
封装 HTTP 连接，将请求解析和响应构造整合在一起，处理读写事件。

### 8.1 创建 `code/http/httpconn.h`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#ifndef HTTPCONN_H
#define HTTPCONN_H

#include <sys/types.h>
#include <sys/uio.h>   // readv/writev
#include <arpa/inet.h> // sockaddr_in
#include <stdlib.h>    // atoi()
#include <errno.h>    
#include <atomic>

#include "../log/log.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

class HttpConn {
public:
    HttpConn();
    ~HttpConn();

    void init(int sockFd, const sockaddr_in& addr);

    // 每个连接的读/写接口
    ssize_t read(int* saveErrno);
    ssize_t write(int* saveErrno);

    // 关闭连接
    void Close();

    // 获取文件描述符和地址
    int GetFd() const;
    struct sockaddr_in GetAddr() const;
    const char* GetIP() const;
    int GetPort() const;

    // 处理HTTP请求
    bool process();

    // 获取读/写缓冲区
    int ToWriteBytes() {
        return iov_[0].iov_len + iov_[1].iov_len;
    }

    bool IsKeepAlive() const {
        return request_.IsKeepAlive();
    }

    static std::atomic<int> userCount;
    static bool isET;

private: 
    int fd_;
    struct sockaddr_in addr_;
    
    bool isClose_;
    
    int iovCnt_;
    struct iovec iov_[2];
    
    Buffer readBuff_; // 读缓冲区
    Buffer writeBuff_; // 写缓冲区
    
    HttpRequest request_;
    HttpResponse response_;
};

#endif //HTTPCONN_H
```

### 8.2 创建 `code/http/httpconn.cpp`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#include "httpconn.h"

std::atomic<int> HttpConn::userCount(0);
bool HttpConn::isET = false;

HttpConn::HttpConn() { 
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
};

HttpConn::~HttpConn() {
    Close(); 
};

void HttpConn::init(int sockFd, const sockaddr_in& addr) {
    assert(sockFd > 0);
    userCount++;
    fd_ = sockFd;
    addr_ = addr;
    readBuff_.RetrieveAll();
    writeBuff_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

void HttpConn::Close() {
    response_.UnmapFile();
    if(isClose_ == false){
        isClose_ = true; 
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

int HttpConn::GetFd() const {
    return fd_;
};

struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

const char* HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = readBuff_.ReadFd(fd_, saveErrno);
        if (len <= 0) {
            break;
        }
    } while (isET);  // ET模式需要循环读取
    return len;
}

ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCnt_);
        if(len <= 0) {
            *saveErrno = errno;
            break;
        }
        if(iov_[0].iov_len + iov_[1].iov_len  == 0) { break; } /* 传输结束 */
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
            writeBuff_.Retrieve(len);
        }
    } while (isET || ToWriteBytes() > 10240);
    return len;
}

bool HttpConn::process() {
    request_.Init();
    if(readBuff_.ReadableBytes() <= 0) {
        return false;
    }
    else if(request_.parse(readBuff_)) {
        LOG_DEBUG("%s", request_.path().c_str());
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
        request_.Init();
        response_.MakeResponse(writeBuff_);
        // 响应头
        iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
        iov_[0].iov_len = writeBuff_.ReadableBytes();
        iovCnt_ = 1;

        // 文件
        if(response_.FileLen() > 0  && response_.File()) {
            iov_[1].iov_base = response_.File();
            iov_[1].iov_len = response_.FileLen();
            iovCnt_ = 2;
        }
        return true;
    }
    else {
        response_.Init(srcDir, request_.path(), false, 400);
        request_.Init();
        response_.MakeResponse(writeBuff_);
        iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
        iov_[0].iov_len = writeBuff_.ReadableBytes();
        iovCnt_ = 1;
    }
    return false;
}
```

### 8.3 更新 Makefile

修改 `SOURCES` 行：
```makefile
SOURCES = $(SRC_DIR)/main.cpp config/config.cpp buffer/buffer.cpp log/log.cpp http/httprequest.cpp http/httpresponse.cpp http/httpconn.cpp
```

### 8.4 编译测试

```bash
make clean
make
```

## 【第 9 步】epoll 事件循环（Epoller）

### 目标
实现基于 epoll 的事件循环，处理文件描述符的读写事件。

### 9.1 创建 `code/server/epoller.h`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <errno.h>

class Epoller {
public:
    explicit Epoller(int maxEvents = 1024);
    ~Epoller();

    bool AddFd(int fd, uint32_t events);
    bool ModFd(int fd, uint32_t events);
    bool DelFd(int fd);
    int Wait(int timeoutMs = -1);
    int GetEventFd(size_t i) const;
    uint32_t GetEvents(size_t i) const;

private:
    int epollFd_;
    std::vector<struct epoll_event> events_;
};

#endif
```

### 9.2 创建 `code/server/epoller.cpp`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#include "epoller.h"

Epoller::Epoller(int maxEvents): epollFd_(epoll_create1(0)), events_(maxEvents) {
    assert(epollFd_ >= 0 && events_.size() > 0);
}

Epoller::~Epoller() {
    close(epollFd_);
}

bool Epoller::AddFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    struct epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::ModFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    struct epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::DelFd(int fd) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}

int Epoller::Wait(int timeoutMs) {
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
}

int Epoller::GetEventFd(size_t i) const {
    assert(i < events_.size());
    return events_[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const {
    assert(i < events_.size());
    return events_[i].events;
}
```

### 9.3 更新 Makefile

修改 `SOURCES` 行：
```makefile
SOURCES = $(SRC_DIR)/main.cpp config/config.cpp buffer/buffer.cpp log/log.cpp http/httprequest.cpp http/httpresponse.cpp http/httpconn.cpp server/epoller.cpp
```

### 9.4 编译测试

```bash
make clean
make
```

## 【第 10 步】定时器模块（HeapTimer）

### 目标
实现基于最小堆的定时器，用于处理非活动连接的超时断开。

### 10.1 创建 `code/timer/heaptimer.h`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#ifndef HEAPTIMER_H
#define HEAPTIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h>
#include <functional>
#include <assert.h>
#include "../log/log.h"

typedef std::function<void()> TimeoutCallBack;
typedef std::pair<size_t, size_t> TimerNode; // (到期时间, socket描述符)

class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }
    
    ~HeapTimer() { clear(); }
    
    void adjust(int id, int newExpires);
    
    void add(int id, int timeOut, const TimeoutCallBack& cb);
    
    void doWork(int id);
    
    void clear();
    
    void tick();
    
    void pop();
    
    int GetNextTick();

private:
    void del_(size_t i);
    
    void siftup_(size_t i);
    
    bool siftdown_(size_t index, size_t n);
    
    void SwapNode_(size_t i, size_t j);

    std::vector<TimerNode> heap_;
    std::unordered_map<int, size_t> ref_; // socket描述符 -> 在堆中的位置
};

#endif //HEAPTIMER_H
```

### 10.2 创建 `code/timer/heaptimer.cpp`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#include "heaptimer.h"

void HeapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    size_t j = (i - 1) / 2;
    while(j >= 0) {
        if(heap_[j] < heap_[i]) { break; }
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

void HeapTimer::siftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while(j < n) {
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;
        if(heap_[i] < heap_[j]) break;
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].second] = i;
    ref_[heap_[j].second] = j;
} 

void HeapTimer::add(int id, int timeOut, const TimeoutCallBack& cb) {
    assert(id >= 0);
    size_t i;
    if(ref_.count(id) == 0) {
        /* 新节点：堆尾插入，上浮调整 */
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({clock() + timeOut, id});
        siftup_(i);
    } 
    else {
        /* 已有节点：调整，并更新回调函数 */
        i = ref_[id];
        heap_[i].first = clock() + timeOut;
        if(!siftdown_(i, heap_.size())) {
            siftup_(i);
        }
    }
}

void HeapTimer::doWork(int id) {
    /* 删除指定id节点，并触发回调函数 */
    if(heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    TimerNode node = heap_[i];
    del_(i);
}

void HeapTimer::del_(size_t index) {
    /* 删除指定位置的节点 */
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    /* 将要删除的节点换到队尾，然后调整堆 */
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if(i < n) {
        SwapNode_(i, n);
        if(!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    /* 队尾元素删除 */
    ref_.erase(heap_.back().second);
    heap_.pop_back();
}

void HeapTimer::adjust(int id, int newExpires) {
    /* 调整指定id的节点 */
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].first = newExpires;
    siftdown_(ref_[id], heap_.size());
}

void HeapTimer::tick() {
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        TimerNode node = heap_.front();
        if(std::difftime(node.first, clock()) > 0) { 
            break; 
        }
        node.second();
        pop();
    }
}

void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}

void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

int HeapTimer::GetNextTick() {
    tick();
    size_t res = -1;
    if(!heap_.empty()) {
        res = std::difftime(heap_.front().first, clock());
        if(res < 0) { res = 0; }
    }
    return res;
}
```

### 10.3 更新 Makefile

修改 `SOURCES` 行：
```makefile
SOURCES = $(SRC_DIR)/main.cpp config/config.cpp buffer/buffer.cpp log/log.cpp http/httprequest.cpp http/httpresponse.cpp http/httpconn.cpp server/epoller.cpp timer/heaptimer.cpp
```

### 10.4 编译测试

```bash
make clean
make
```

## 【第 11 步】线程池（ThreadPool）

### 目标
实现线程池，用于处理并发请求。

### 11.1 创建 `code/pool/threadpool.h`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
#include <assert.h>

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8);
    ~ThreadPool();

    template<class T>
    void AddTask(T&& task);

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    std::mutex queueMutex_;
    std::condition_variable condition_;
    bool stop_;
};

// 构造函数
ThreadPool::ThreadPool(size_t threadCount) : stop_(false) {
    for(size_t i = 0; i < threadCount; ++i) {
        workers_.emplace_back([this] {
            for(;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex_);
                    this->condition_.wait(lock, [this] {
                        return this->stop_ || !this->tasks_.empty();
                    });
                    if(this->stop_ && this->tasks_.empty())
                        return;
                    task = std::move(this->tasks_.front());
                    this->tasks_.pop();
                }
                task();
            }
        });
    }
}

// 析构函数
ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for(std::thread &worker: workers_)
        worker.join();
}

// 添加任务
template<class T>
void ThreadPool::AddTask(T&& task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        if(stop_)
            throw std::runtime_error("enqueue on stopped ThreadPool");
        tasks_.emplace(std::forward<T>(task));
    }
    condition_.notify_one();
}

#endif
```

### 11.2 更新 Makefile

修改 `SOURCES` 行：
```makefile
SOURCES = $(SRC_DIR)/main.cpp config/config.cpp buffer/buffer.cpp log/log.cpp http/httprequest.cpp http/httpresponse.cpp http/httpconn.cpp server/epoller.cpp timer/heaptimer.cpp pool/threadpool.cpp
```

### 11.3 编译测试

```bash
make clean
make
```

## 【第 12 步】数据库连接池（SqlConnPool）

### 目标
实现数据库连接池，提高数据库访问效率。

### 12.1 创建 `code/pool/sqlconnpool.h`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <cassert>
#include "../log/log.h"

class SqlConnPool {
public:
    static SqlConnPool* Instance();

    void Init(const char* host, int port,
              const char* user, const char* pwd,
              const char* dbName, int connSize);
    
    MYSQL* GetConn();
    void FreeConn(MYSQL* conn);
    int GetFreeConnCount();

    void ClosePool();

private:
    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN_;
    std::queue<MYSQL*> connQue_;
    std::mutex mtx_;
    sem_t semId_;
};

class SqlConnRAII {
public:
    SqlConnRAII(MYSQL** sql, SqlConnPool* connpool);
    ~SqlConnRAII();

private:
    MYSQL* sql_;
    SqlConnPool* pool_;
};

#endif
```

### 12.2 创建 `code/pool/sqlconnpool.cpp`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#include "sqlconnpool.h"

SqlConnPool* SqlConnPool::Instance() {
    static SqlConnPool pool;
    return &pool;
}

void SqlConnPool::Init(const char* host, int port,
              const char* user, const char* pwd,
              const char* dbName, int connSize) {
    assert(connSize > 0);
    MAX_CONN_ = connSize;
    for (int i = 0; i < connSize; i++) {
        MYSQL *sql = nullptr;
        sql = mysql_init(sql);
        if (!sql) {
            LOG_ERROR("MySQL init error!");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host,
                                 user, pwd,
                                 dbName, port, nullptr, 0);
        if (!sql) {
            LOG_ERROR("MySQL Connect error!");
        }
        connQue_.push(sql);
    }
    sem_init(&semId_, 0, MAX_CONN_);
}

MYSQL* SqlConnPool::GetConn() {
    MYSQL *sql = nullptr;
    if(connQue_.empty()){
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId_);
    {
        std::lock_guard<std::mutex> locker(mtx_);
        sql = connQue_.front();
        connQue_.pop();
    }
    return sql;
}

void SqlConnPool::FreeConn(MYSQL* sql) {
    assert(sql);
    std::lock_guard<std::mutex> locker(mtx_);
    connQue_.push(sql);
    sem_post(&semId_);
}

void SqlConnPool::ClosePool() {
    std::lock_guard<std::mutex> locker(mtx_);
    while(!connQue_.empty()) {
        auto item = connQue_.front();
        connQue_.pop();
        mysql_close(item);
    }
    mysql_library_end();
}

int SqlConnPool::GetFreeConnCount() {
    std::lock_guard<std::mutex> locker(mtx_);
    return connQue_.size();
}

SqlConnPool::~SqlConnPool() {
    ClosePool();
}

SqlConnRAII::SqlConnRAII(MYSQL** sql, SqlConnPool* connpool) {
    assert(connpool);
    *sql = connpool->GetConn();
    sql_ = *sql;
    pool_ = connpool;
}

SqlConnRAII::~SqlConnRAII() {
    if(sql_) { pool_->FreeConn(sql_); }
}
```

### 12.3 更新 Makefile

修改 `SOURCES` 行：
```makefile
SOURCES = $(SRC_DIR)/main.cpp config/config.cpp buffer/buffer.cpp log/log.cpp http/httprequest.cpp http/httpresponse.cpp http/httpconn.cpp server/epoller.cpp timer/heaptimer.cpp pool/threadpool.cpp pool/sqlconnpool.cpp
```

### 12.4 编译测试

```bash
make clean
make
```

## 【第 13 步】WebServer 主逻辑（WebServer）

### 目标
实现 WebServer 主逻辑，整合所有模块，处理客户端请求。

### 13.1 创建 `code/server/webserver.h`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>     // close()
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../epoller.h"
#include "../log/log.h"
#include "../heaptimer.h"
#include "../threadpool.h"
#include "../sqlconnpool.h"
#include "../http/httpconn.h"

class WebServer {
public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger,
        int sqlPort, const char* sqlUser, const  char* sqlPwd,
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);

    ~WebServer();
    
    void Start();

private:
    bool InitSocket_();
    void InitEventMode_(int trigMode);
    void AddClient_(int fd, sockaddr_in addr);
    
    void DealListen_();
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);
    
    void SendError_(int fd, const char* info);
    void ExtentTime_(HttpConn* client);
    void CloseConn_(HttpConn* client);
    
    void OnRead_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnProcess_(HttpConn* client);

    static const int MAX_FD = 65536;

    int port_;
    bool openLinger_;
    int timeoutMS_;  /* 毫秒 */
    bool isClose_;
    int listenFd_;
    char* srcDir_;
    
    uint32_t listenEvent_;
    uint32_t connEvent_;
    
    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<ThreadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConn> users_;
};

#endif
```

### 13.2 创建 `code/server/webserver.cpp`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#include "webserver.h"

WebServer::WebServer(
    int port, int trigMode, int timeoutMS, bool OptLinger,
    int sqlPort, const char* sqlUser, const  char* sqlPwd,
    const char* dbName, int connPoolNum, int threadNum,
    bool openLog, int logLevel, int logQueSize):
    port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
    timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller()) {
    
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);
    
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;
    
    // 初始化日志系统
    if(openLog) {
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                           (listenEvent_ & EPOLLET ? "ET": "LT"),
                           (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
    
    // 初始化数据库连接池
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
    
    InitEventMode_(trigMode);
    if(!InitSocket_()) { isClose_ = true; }
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

void WebServer::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

void WebServer::Start() {
    int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    while(!isClose_) {
        if(timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick();
        }
        int eventCnt = epoller_->Wait(timeMS);
        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if(fd == listenFd_) {
                DealListen_();
            } else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);
            } else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            } else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::SendError_(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::CloseConn_(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if(timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
}

void WebServer::DealListen_() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if(fd <= 0) { return;}
        else if(HttpConn::userCount >= MAX_FD) {
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);
    } while(listenEvent_ & EPOLLET);
}

void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

void WebServer::OnRead_(HttpConn* client) {
    assert(client);
    int readErrno = 0;
    ssize_t n = client->read(&readErrno);
    if(n <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }
    OnProcess_(client);
}

void WebServer::OnProcess_(HttpConn* client) {
    if(client->process()) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int writeErrno = 0;
    ssize_t n = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {
            OnProcess_(client);
            return;
        }
    }
    else if(n < 0) {
        if(writeErrno == EAGAIN) {
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { 
        timer_->adjust(client->GetFd(), timeoutMS_); 
    }
}

bool WebServer::InitSocket_() {
    int ret;
    struct sockaddr_in addr;
    if(port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    struct linger optLinger = { 0 };
    if(openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = listen(listenFd_, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

int SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}
```

### 13.3 更新 Makefile

修改 `SOURCES` 行：
```makefile
SOURCES = $(SRC_DIR)/main.cpp config/config.cpp buffer/buffer.cpp log/log.cpp http/httprequest.cpp http/httpresponse.cpp http/httpconn.cpp server/epoller.cpp timer/heaptimer.cpp pool/threadpool.cpp pool/sqlconnpool.cpp server/webserver.cpp
```

### 13.4 更新 `code/main.cpp`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#include "server/webserver.h"

int main(int argc, char *argv[]) {
    int port = 8080;
    int trigMode = 0;
    int timeoutMS = 60000;
    bool OptLinger = false;
    int sqlPort = 3306;
    const char* sqlUser = "root";
    const char* sqlPwd = "123456";
    const char* dbName = "webserver";
    int connPoolNum = 6;
    int threadNum = 8;
    bool openLog = true;
    int logLevel = 0;
    int logQueSize = 1024;

    // 解析命令行参数
    int opt;
    const char *str = "t:l:e:o:";
    while ((opt = getopt(argc, argv, str)) != -1) {
        switch (opt) {
            case 't': threadNum = atoi(optarg); break;
            case 'l': logQueSize = atoi(optarg); break;
            case 'e': trigMode = atoi(optarg); break;
            case 'o': openLog = atoi(optarg); break;
            default: break;
        }
    }

    // 创建WebServer实例并启动
    WebServer server(
        port, trigMode, timeoutMS, OptLinger,
        sqlPort, sqlUser, sqlPwd, dbName,
        connPoolNum, threadNum, openLog, logLevel, logQueSize
    );
    
    server.Start();
    
    return 0;
}
```

### 13.5 编译与测试

```bash
make clean
make
./bin/server
```

## 【第 14 步】静态资源与测试

### 目标
添加静态资源，并进行功能测试。

### 14.1 创建静态资源目录

```bash
mkdir -p resources/html
mkdir -p resources/image
mkdir -p resources/video
mkdir -p resources/js
mkdir -p resources/css
```

### 14.2 添加简单的 HTML 文件

创建 `resources/html/index.html`:

```html
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>WebServer Test</title>
</head>
<body>
    <h1>Welcome to WebServer!</h1>
    <p>This is a test page for the WebServer.</p>
</body>
</html>
```

### 14.3 测试 WebServer

```bash
# 启动服务器
./bin/server

# 在另一个终端测试
curl http://localhost:8080/index.html
```

## 【第 15 步】性能测试

### 目标
使用 webbench 进行性能测试。

### 15.1 编译 webbench

```bash
cd webbench-1.5
make
```

### 15.2 运行性能测试

```bash
./webbench -c 100 -t 30 http://localhost:8080/index.html
```

## 【第 16 步】优化与文档

### 目标
优化代码性能，完善项目文档。

### 16.1 性能优化建议

1. 使用内存池减少内存分配开销
2. 优化日志系统，减少锁竞争
3. 使用零拷贝技术优化数据传输
4. 优化数据库查询，添加索引

### 16.2 完善文档

1. 添加详细的代码注释
2. 编写用户手册
3. 添加开发文档
4. 编写测试用例

## 总结

通过以上步骤，我们已经完成了一个基于 C++ 的 WebServer 项目，包括：

1. 项目骨架与构建系统
2. 配置模块（Config）
3. 缓冲区模块（Buffer）
4. 阻塞队列（BlockDeque）
5. 日志模块（Log）
6. HTTP 请求解析（HttpRequest）
7. HTTP 响应构造（HttpResponse）
8. HTTP 连接封装（HttpConn）
9. epoll 事件循环（Epoller）
10. 定时器模块（HeapTimer）
11. 线程池（ThreadPool）
12. 数据库连接池（SqlConnPool）
13. WebServer 主逻辑（WebServer）
14. 静态资源与测试
15. 性能测试
16. 优化与文档

这个项目实现了一个高性能的 Web 服务器，支持 HTTP 协议，具有异步日志、线程池、数据库连接池等功能，可以处理并发请求，并具有良好的扩展性和可维护性。
=======

>>>>>>> 431c097fcc60f48ce18d91356752377d48c137a6
