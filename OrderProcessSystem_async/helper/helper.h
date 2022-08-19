#ifndef HELPER_H
#define HELPER_H

#include <string>
#include <iostream>
#include <time.h>
#include <sys/timeb.h>
#include "../proto/OrderProcessSystem.grpc.pb.h"

#define TYPE_LIMIT true
#define TYPE_MARKET false
#define DIRE_SELL true
#define DIRE_BUY false

using grpc::Server;
using grpc::ServerAsyncReaderWriter;
using grpc::ServerAsyncWriter;
using grpc::ServerAsyncReader;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerCompletionQueue;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using OPS::NewOrderRequest;
using OPS::CancelOrderRequest;
using OPS::QueryOrderRequest;
using OPS::SendMessageRequest;
using OPS::ExecutionReport;
using OPS::OrderReport;
using OPS::OrderService;

// 输出请求和响应消息
void printRequest(const NewOrderRequest&);
void printRequest(const CancelOrderRequest&);
void printReport(const ExecutionReport&);
void printReport(const OrderReport&);
// 判断请求的格式
bool checkRequest(const NewOrderRequest&, std::string&);
void initReport(ExecutionReport&, const NewOrderRequest&);
void initReport(ExecutionReport&, const CancelOrderRequest&);
void initReport(OrderReport&, const NewOrderRequest&, const uint64_t&);

// 获取系统时间，年月日时分秒
std::string getTime();
// 获取时间戳
uint64_t getTimestamp();

// 创建新订单请求
NewOrderRequest MakeNewOrderRequest(const bool&, const bool&, 
				const uint64_t&, const std::string&,
				const uint32_t&, const double&);

// 创建撤销订单请求
CancelOrderRequest MakeCancelOrderRequest(const uint64_t&);

// 创建查询订单请求
QueryOrderRequest MakeQueryOrderRequest();

// 创建发送消息请求
SendMessageRequest MakeSendMessageRequest();
#endif 
