#ifndef CONFIG_H
#define CONFIG_H

#pragma once
#include <cstdint>
#include <string>

// 全局配置类 单例模式
class Config {
public:
    // 获取单例实例
    static Config& getInstance();
    // 配置参数
    /*
            int port, int trigMode, int timeoutMS, bool OptLinger, 
        int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);
    */

    uint32_t c_port;
    int c_thread_cnt;
    int c_trigMode; // 1 LT 2 ET
    int c_maxConnection;
    bool c_isOptLinger; // 是否优雅关闭连接

    std::string c_resource_root;

    std::string c_log_file;    
    int c_log_level; // 0 : DEBUG 1 : INFO 2 : WARN 3 : ERROR
    int c_log_queue_size; // 日志队列大小
    bool c_open_log; // 是否开启日志
    int c_max_body_size; // 最大请求体大小 1MB
    int c_timeout; // 默认超时时间 60s

    int c_conn_pool_num; // 数据库连接池数量
    std::string c_db_host;
    int c_db_port;
    std::string c_db_user;
    std::string c_db_password;
    std::string c_db_name;

    // 解析命令行参数
    void parse_args(int argc, char* argv[]);

    // 解析配置文件
    void parse_config_file(const std::string& filepath);

    // 打印当前配置
    void print_config() const;

private:
    Config() = default;
    ~Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
};  


#endif /* CONFIG_H */

