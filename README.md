# WebServer

用C++实现的高性能WEB服务器，经过webbenchh压力测试可以实现上万的QPS

## 特点 & 功能

* 基本基于现代C++标准开发，如智能指针、std::format、泛型编程、RAII等，语法简洁 实现高效；
* 实现HTTP协议，支持GET、POST方法，支持长连接；
* 基于I/O多路复用技术Epoll 与线程池实现多线程的主从Reactor高并发网络模型；
* 利用正则与状态机解析HTTP请求报文，实现处理静态资源的请求；
* 利用标准库容器封装char，实现自动增长的缓冲区；
* 基于小根堆实现定时器，关闭超时的非活动连接；
* 基于单例模式与阻塞队列实现异步日志系统，记录服务器运行状态；
* 使用RAII机制实现了数据库连接池，减少数据库连接建立与关闭的开销，同时实现了用户注册登录功能。

## 环境要求

* Linux
* C++20
* MySql

## 目录树
```
.
├── code           源代码
│   ├── buffer
│   ├── config
│   ├── http
│   ├── log
│   ├── timer
│   ├── pool
│   ├── server
│   └── main.cpp
├── test           单元测试
│   ├── Makefile
│   └── test.cpp
├── resources      静态资源
│   ├── index.html
│   ├── image
│   ├── video
│   ├── js
│   └── css
├── bin            可执行文件
│   └── server
├── log            日志文件
├── webbench-1.5   压力测试
├── Makefile
├── config.conf
├── LICENSE
└── readme.md
```


## 项目启动

需要先配置好对应的数据库

```bash
// 建立yourdb库
create database yourdb;

// 创建user表
USE yourdb;
CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
)ENGINE=InnoDB;

// 添加数据
INSERT INTO user(username, password) VALUES('name', 'password');
```

```bash
make
./bin/server
```

## 单元测试
```bash
cd test
make
./test
```

## 压力测试
![image-webbench](https://github.com/markparticle/WebServer/blob/master/readme.assest/%E5%8E%8B%E5%8A%9B%E6%B5%8B%E8%AF%95.png)
```bash
./webbench-1.5/webbench -c 100 -t 10 http://ip:port/
./webbench-1.5/webbench -c 1000 -t 10 http://ip:port/
./webbench-1.5/webbench -c 5000 -t 10 http://ip:port/
./webbench-1.5/webbench -c 10000 -t 10 http://ip:port/
```
* 测试环境: Ubuntu:19.10 cpu:i5-8400 内存:8G 
* QPS 10000+

## TODO
* 完善单元测试
* 实现循环缓冲区

## 致谢
Linux高性能服务器编程，游双著.

[@qinguoyi](https://github.com/qinguoyi/TinyWebServer)
