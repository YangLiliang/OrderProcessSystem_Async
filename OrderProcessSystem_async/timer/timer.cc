#ifndef TIMER_CC
#define TIMER_CC
#include "timer.h"
# include "../market/market_system.h"

TaskNode::TaskNode(const uint64_t& orderID_){
    orderID=orderID_;
    next=this;
    prev=this;
}

// 构造函数
TaskList::TaskList(){
    // 初始化头节点
    head=new TaskNode(0);
    // 索引置空
    taskIndex.clear();
}

void TaskList::addTask(const uint64_t& orderID){
    // 创建节点
    TaskNode* task=new TaskNode(orderID);
    // 插入双链表末尾
    std::unique_lock<std::shared_mutex> w(taskListMutex);
    uint64_t ts=getTimestamp();
    task->timestamp=ts;
    TaskNode* prev=head->prev;
    task->next=head;
    task->prev=prev;
    prev->next=task;
    head->prev=task;
    // 添加至索引
    taskIndex[orderID]=task;
}

void TaskList::delTask(const uint64_t& orderID){
    // 删除指定的任务
    std::unique_lock<std::shared_mutex> w(taskListMutex);
    if(taskIndex.find(orderID)!=taskIndex.end()){
        TaskNode* task=taskIndex[orderID];
        task->next->prev=task->prev;
        task->prev->next=task->next;
        task->next=nullptr;
        task->prev=nullptr;
        // 删除任务节点
        delete task;
        // 从索引中删除
        taskIndex.erase(orderID);
    }
}

bool TaskList::checkFirstTask(){
    // 任务的时间戳
    uint64_t task_ts=0;
    // 获取首节点
    {
        std::shared_lock<std::shared_mutex> r(taskListMutex);
        if(head->next==head){
            return false;
        }
        TaskNode* task=head->next;
        task_ts=task->timestamp;
    }
    // 获取当前时间戳
    uint64_t cur_ts=getTimestamp();
    // 判断时间戳之差
    if(cur_ts-task_ts<DURATION){
        return false;
    }
    return true;
}

bool TaskList::getFirstTask(uint64_t& orderID){
    // 任务
    TaskNode* task=nullptr;
    {
        std::unique_lock<std::shared_mutex> w(taskListMutex);
        if(head->next==head){
            return false;
        }
        // 获取任务
        task=head->next;
        // 任务时间戳
        uint64_t task_ts=task->timestamp;
        // 获取当前时间戳
        uint64_t cur_ts=getTimestamp();
        // 判断时间戳之差
        if(cur_ts-task_ts<DURATION){
            return false;
        }
        // 从链表中移除节点
        task->next->prev=task->prev;
        task->prev->next=task->next;
        // 获取订单ID
        orderID=task->orderID;
        // 从索引中删除
        taskIndex.erase(orderID);
    }
    // 删除任务节点
    delete task;
    return true;
}

Timer::Timer(){
    taskList=new TaskList();
}

void Timer::addTask(const uint64_t& orderID){
    taskList->addTask(orderID);
}

void Timer::delTask(const uint64_t& orderID){
    taskList->delTask(orderID);
}

void Timer::run(){
    // 节点指针
    TaskNode* task;
    while(1){
        while(!taskList->checkFirstTask()){
            // 每隔100ms检查首结点
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        uint64_t orderID;
        while(taskList->getFirstTask(orderID)){
            // 获取交易市场指针
            MarketSystem* ms=MarketSystem::getInstance();
            // 执行模拟撮合
            if(ms->simulationMatch(orderID)){ // 
                // 若还剩余订单，则继续更新并添加至计时器
                addTask(orderID);
            }
        }
    }
}
#endif