#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>

#include "config/config.h"

//  为什么使用fprintf?
//  1. fprintf可以直接将日志输出到文件或标准输出，而不需要额外的缓冲机制。这使得日志记录更加高效和直接。
//  2. fprintf提供了格式化输出的功能，可以方便地将变量和文本组合在一起，生成更具可读性的日志信息。
//  3. fprintf是C标准库的一部分，具有广泛的兼容性和稳定性，可以在各种平台上使用，而不需要担心不同平台之间的差异。

#define LOG_INFO(fmt, ...) \
    do { \
        fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__); \
    } while(0)

#define LOG_ERROR(fmt, ...) \
    do { \
        fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__); \
    } while(0)


int main(int argc, char* argv[]) {
    // 获取配置单例并解析命令行
    Config& config = Config::getInstance();
    config.parse_args(argc, argv);

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
