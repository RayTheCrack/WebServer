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
    uint32_t port = 8080;
    int thread_cnt = 5;
    std::string resource_root = "resources/";
    std::string log_file = "log/webserver.log";    
    int log_level = 1; // 0 : DEBUG 1 : INFO 2 : WARN 3 : ERROR
    int MAX_BODY_SIZE = 1024 * 1024; // 最大请求体大小 1MB
    int TIMEOUT = 60; // 默认超时时间 60s
    std::string db_host = "localhost";
    int db_port = 3306;
    std::string db_user = "root";
    std::string db_password = "password";
    std::string db_name = "webserver";

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

