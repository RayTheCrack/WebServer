#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <unordered_map>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string>
#include <assert.h>

#include "../config/config.h"
#include "../buffer/buffer.h"
#include "../log/log.h"

/**
 * @brief HTTP响应类，用于处理HTTP响应的构建和管理
 */
class HttpResponse {
public:
    
    HttpResponse(); 
    ~HttpResponse();

    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    void MakeResponse(Buffer& buff);
    void UnmapFile(); // 解除文件的内存映射（释放 mmap 资源）
    char* GetFile();
    size_t FileLen() const;
    void ErrorContent(Buffer& buff, std::string message);
    int Code() const {return code_;};


private:
    // 状态行
    void AddStateLine_(Buffer& buff);
    // 响应头
    void AddHeaders_(Buffer& buff);
    // 响应体
    void AddBody_(Buffer& buff);

    void ErrorHtml_();
    std::string GetFileType_();

    int code_; // HTTP 响应状态码（如 200、404、500）
    bool isKeepAlive_;
    std::string path_; // 请求的文件路径
    std::string srcDir_; // 网站根目录
    // mmap() 函数的作用是把磁盘文件直接映射到进程的虚拟内存空间，从而实现文件的高效读写
    char* mmFile_; // 内存映射的文件指针
    struct stat mmFileStat_; // 文件状态结构体（存储文件大小、类型等信息，通过 stat 函数获取）
    // 静态常量：文件后缀与 Content-Type 的映射（如 .html → text/html）
    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    // 静态常量：HTTP 状态码与状态描述的映射（如 200 → OK）
    static const std::unordered_map<int, std::string> CODE_STATUS;
    // g静态常量：状态码与错误页面路径的映射（如 404 → /404.html）
    static const std::unordered_map<int, std::string> CODE_PATH;
    
};

#endif /* HTTPRESPONSE_H */
