# A Web Server Based on C++17

## Introduction

一个基于C++17的Web服务器。支持GET、HEAD以及POST方法。支持HTTP长连接、短连接，以及管线化请求。日志使用[spdlog](https://github.com/gabime/spdlog)。

## Envoirment

- OS: Ubuntu 20.04
- Complier: g++ 9.3.0
- CMake:  3.20

## Build

mkdir build&&cd build

cmake ..

make

## Usage

cd ../bin/release

./WebServer [-p port_number] [-s subreactor_number ] [-l log_file_path(start with .)]

## Technical points

- 采用多Reactor多线程模式，并使用边沿触发的Epoll多路复用技术。
- 基于时间轮算法的定时器以实现请求报文传输以及长连接的超时回调。
- One loop per thread，主线程MainReactor负责accept并将连接socket分发给SubReactors；子线程中的SubReactors负责监听连接socket上的事件以及调用相应的回调函数。
- 为了避免shared_ptr带来的污染，使用raw pointer + unique_ptr的形式管理资源。raw pointer用于访问资源，unique_ptr掌管对象的生命周期。
- 使用状态机解析HTTP请求，支持管线化。

## Problem

Webench测得的长连接和短连接的QPS很低，且两者的值很接近。一般情况下，长连接的QPS应是短连接的2~3倍。
##天喻
