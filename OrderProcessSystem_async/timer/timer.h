#ifndef TIMER_H
#define TIMER_H

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include "../helper/helper.h"
#define DURATION 3000
class MarketSystem;

// 任务节点
struct TaskNode{
    // 前驱和后继指针
    struct TaskNode* next;
    struct TaskNode* prev;
    // 订单ID
    uint64_t orderID;
    // 时间戳
    uint64_t timestamp;
    // 回调函数
    std::function<bool(const uint64_t&)> call;
    // 构造函数
    TaskNode(const uint64_t&);
};

// 任务双向循环链表
class TaskList{
public:
    // 构造函数
    TaskList();
    // 添加删除任务
    void addTask(const uint64_t&);
    void delTask(const uint64_t&);
    // 检测首节点
    bool checkFirstTask();
    // 获取首节点
    bool getFirstTask(uint64_t&);
private:
    TaskNode* head;
    // OrderID到结点指针的映射
    std::unordered_map<uint64_t, TaskNode*> taskIndex;
    // 链表的读写锁
    std::shared_mutex taskListMutex;
};

// 计时器
class Timer{
public:
    // 构造函数
    Timer();
    // 添加任务
    void addTask(const uint64_t&);
    // 删除任务
    void delTask(const uint64_t&);
    // 运行计时器
    void run();
private:
    TaskList* taskList;
};
# endif