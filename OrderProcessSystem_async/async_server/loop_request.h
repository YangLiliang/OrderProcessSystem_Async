#ifndef LOOP_REQUEST_H
#define LOOP_REQUEST_H
#include <algorithm>
#include <string>
#include <iostream>
#include <unordered_map>
#include <set>
#include <time.h>
#include <mutex>
#include <memory>
#include <thread>
#include <unistd.h>
#include <stdexcept>
#include <boost/utility.hpp>
#include <boost/type_traits.hpp>
#include <cmath>
#include "../helper/helper.h"

#include <grpc++/grpc++.h>
#include <grpc/support/log.h>
#include "../proto/OrderProcessSystem.grpc.pb.h"
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientAsyncReader;
using grpc::ClientAsyncReaderWriter;
using grpc::ClientAsyncWriter;
using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;

using OPS::NewOrderRequest;
using OPS::CancelOrderRequest;
using OPS::QueryOrderRequest;
using OPS::ExecutionReport;
using OPS::OrderReport;
using OPS::OrderService;

/*****************************************************************************************
 * 在服务端创建一个循环请求对象，每隔一定时间向服务端发送请求，服务端接受请求后向各个客户端发送交易消息
 ****************************************************************************************/

// 循环请求
class LoopRequest{
private:
	std::unique_ptr<OrderService::Stub>stub_;
	CompletionQueue cq_;
public:
	explicit LoopRequest(std::shared_ptr<Channel> channel);
	// 使服务端发送消息
	void PushSendMessage();
	// 异步处理完成队列中的事件
	void AsyncCompleteRpc();
};

// CallData抽象类
class AbstractLoopCall{
public:
    // 状态机
	enum CallStatus {PROCESS, FINISH, DESTROY};
	explicit AbstractLoopCall():callStatus(PROCESS){}
	virtual ~AbstractLoopCall(){}
    // 上下文
	ClientContext context;
	Status status;
	CallStatus callStatus;
	ExecutionReport executionReport_;
	OrderReport queryReport_;
	virtual void Proceed(bool = true) = 0;
};

// 使服务端发送消息类
class PushSendMessageCall:public AbstractLoopCall{
private:
	std::unique_ptr< ClientAsyncReader<ExecutionReport> > responder;
	uint64_t reportsCounter;
public:
	PushSendMessageCall(const SendMessageRequest& request, CompletionQueue& cq_, std::unique_ptr<OrderService::Stub>& stub_);
	virtual void Proceed(bool ok = true) override;
};
#endif