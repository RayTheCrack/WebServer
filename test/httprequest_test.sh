#!/bin/bash

# HTTP请求解析测试程序

g++ -std=c++23 -Wall -Wextra -O2 -pthread \
    -I./code \
    -o bin/test_httprequest \
    code/http/test_httprequest.cpp \
    code/pool/sqlconnpool.cpp \
    code/config/config.cpp \
    code/log/log.cpp \
    code/http/httprequest.cpp \
    code/buffer/buffer.cpp \
    -lmysqlclient -lpthread 

echo "编译完成！运行测试程序："
echo "./bin/test_httprequest"
