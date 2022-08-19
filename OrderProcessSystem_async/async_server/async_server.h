#ifndef SERVER_H
#define SERVER_H
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
#include <functional>
#include <stdexcept>
#include <boost/utility.hpp>
#include <boost/type_traits.hpp>
#include <cmath>
#include <assert.h>
#include "../helper/helper.h"
#include "../market/market_system.h"
#include "loop_request.h"

#include <grpc++/grpc++.h>
#include <grpc/support/log.h>
#include "../proto/OrderProcessSystem.grpc.pb.h"

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

class Responders{
private:
	static std::shared_mutex* rw_lock;
	static std::unordered_map<uint64_t, ServerAsyncReaderWriter<ExecutionReport, NewOrderRequest>*> orderID_responder_;
	// Responders(){}
public:
	static void add(const uint64_t& orderID, ServerAsyncReaderWriter<ExecutionReport, NewOrderRequest>* responder_){
		std::unique_lock<std::shared_mutex> w(*rw_lock);
		orderID_responder_[orderID]=responder_;
	}
	static void get(const uint64_t& orderID, ServerAsyncReaderWriter<ExecutionReport, NewOrderRequest>*& responder_){
		std::shared_lock<std::shared_mutex> r(*rw_lock);
		if(orderID_responder_.count(orderID)) responder_=orderID_responder_.at(orderID);
		else std::cout<<"bug"<<std::endl;
	}
};

std::shared_mutex* Responders::rw_lock=new std::shared_mutex();
std::unordered_map<uint64_t, ServerAsyncReaderWriter<ExecutionReport, NewOrderRequest>*> Responders::orderID_responder_;

// 基类
class CommonCallData{
public:
	OrderService::AsyncService* service_;
	ServerCompletionQueue* cq_;
	ServerContext ctx_;
	NewOrderRequest newOrderRequest_;
	CancelOrderRequest cancelOrderRequest_;
	QueryOrderRequest queryOrderRequest_;
	SendMessageRequest sendMessageRequest_;
	ExecutionReport report_;
	NewOrderRequest orderReport_;
	// 交易市场
	MarketSystem* marketSystem_;
	// 状态机
	enum CallStatus {CREATE, PROCESS, FINISH};
	// 当前的服务状态
	CallStatus status_;
	// 构造函数
	explicit CommonCallData(OrderService::AsyncService*, ServerCompletionQueue*, MarketSystem*);
	// 析构函数
	virtual ~CommonCallData(){}
	virtual void Proceed(bool=true)=0;
};

// 处理新订单类
class CallDataPushNewOrder:public CommonCallData{
private:
	ServerAsyncReaderWriter<ExecutionReport, NewOrderRequest> responder_;
	bool new_responder_created_;
	bool writing_mode_;
	uint32_t RequestsCounter_;
	uint32_t ReportsCounter_;
	std::vector<std::pair<uint64_t, ExecutionReport> > reports_;
public:
	CallDataPushNewOrder(OrderService::AsyncService*, ServerCompletionQueue*, MarketSystem*);
	virtual void Proceed(bool =true) override;
};

// 处理撤销订单
class CallDataPushCancelOrder:public CommonCallData{
private:
	ServerAsyncResponseWriter<ExecutionReport> responder_;
	
public:
	CallDataPushCancelOrder(OrderService::AsyncService*, ServerCompletionQueue*, MarketSystem*);
	virtual void Proceed(bool = true) override;
};

// 处理查询订单
class CallDataPushQueryOrder:public CommonCallData{
private:
	ServerAsyncWriter<OrderReport> responder_;
	bool new_responder_created_;
	uint32_t reportsCounter_;
	std::vector<OrderReport> queryOrderReports_;
public:
	CallDataPushQueryOrder(OrderService::AsyncService*, ServerCompletionQueue*, MarketSystem*);
	virtual void Proceed(bool =true) override;
};

// 处理模拟撮合订单
class CallDataPushSendMessage:public CommonCallData{
private:
	ServerAsyncWriter<ExecutionReport> responder_;
	uint32_t ReportsCounter_;
	std::vector<ExecutionReport> reports_;
	bool new_responder_created_;
public:
	CallDataPushSendMessage(OrderService::AsyncService*, ServerCompletionQueue*, MarketSystem*);
	virtual void Proceed(bool =true) override;
};

// 服务端类
class ServerImpl final{
public:
	ServerImpl(){
		marketSystem_=MarketSystem::getInstance();
	}
	~ServerImpl(){
		server_->Shutdown();
		cq_->Shutdown();	
		delete marketSystem_;
	}
	void Run();
private:
	std::unique_ptr<ServerCompletionQueue> cq_;
 	OrderService::AsyncService service_;
  	std::unique_ptr<Server> server_;
	MarketSystem* marketSystem_;
	void HandleRpcs();
	void getSimulateMatchReports();
};
#endif