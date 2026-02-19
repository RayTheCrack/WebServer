#!/bin/bash

# 数据库连接池测试程序

g++ -std=c++23 -Wall -Wextra -O2 -pthread \
    -I./code \
    -o bin/test_sqlpool \
    code/pool/test_sqlpool.cpp \
    code/pool/sqlconnpool.cpp \
    code/config/config.cpp \
    code/log/log.cpp \
    code/buffer/buffer.cpp \
    -lmysqlclient

echo "编译完成！运行测试程序："
echo "./bin/test_sqlpool"
