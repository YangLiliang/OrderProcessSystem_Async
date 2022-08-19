#ifndef ORDER_SYSTEM_H
#define ORDER_SYSTEM_H

#include <algorithm>
#include <string>
#include <iostream>
#include <unordered_map>
#include <queue>
#include <set>
#include <time.h>
#include <mutex>
#include <utility>
#include <shared_mutex>
#include <thread>
#include <sys/timeb.h>
#include "../helper/helper.h"

#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>
#include "../proto/OrderProcessSystem.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

using OPS::NewOrderRequest;
using OPS::CancelOrderRequest;
using OPS::QueryOrderRequest;
using OPS::ExecutionReport;
using OPS::OrderReport;
using OPS::OrderService;

// 售卖容器和购买容器结构体
struct OrderIndex{
    std::map<double, std::set<uint64_t>> sellOrderIndex; // 卖容器索引
    std::map<double, std::set<uint64_t>> buyOrderIndex; //买容器索引
	std::mutex* rw_lock_; // 对买卖集合均加锁
    OrderIndex();
};

// 订单结构体
struct Order{
	NewOrderRequest info_; // 订单信息
	std::shared_mutex* rw_lock_; // 读写锁
    Order(const NewOrderRequest&); // 构造函数
};

class OrderSystem{
public:
    /***************************************************************************************
                                		订单的增删改
	****************************************************************************************/
    // 插入新订单
	void insertOrder(const uint64_t&, const NewOrderRequest&);
	// 删除订单(删除成功返回true)
	bool deleteOrder(const uint64_t&);
	// 修改订单价格，将市价单的价格更新为市场价
	void updateToMarketPrice(const uint64_t&, const double& marketPrice);
    /***************************************************************************************
                                		订单容器操作相关
	****************************************************************************************/
	// 查询订单信息
	bool getOrderInfo(const uint64_t&, NewOrderRequest&);
	// 获取所有订单
	void getAllOrders(std::vector<OrderReport>&);
	// 订单交易
	void tradingOrders(const uint64_t&, const uint64_t&, const bool&, std::vector<std::pair<uint64_t, ExecutionReport> >&);
    // 模拟撮合.订单数量为0返回false
	bool simulationMatch(const uint64_t&, ExecutionReport&);
	/***************************************************************************************
                                		用户索引操作相关
	****************************************************************************************/
	// 判断是否是对敲订单
	bool isImproperMatchedOrder(const NewOrderRequest&);
    /***************************************************************************************
                                		构造函数
	****************************************************************************************/
    OrderSystem();
private:
	/***************************************************************************************
                                		订单容器
	****************************************************************************************/
	// 存放订单的容器<orderID, Order>, 插入与删除需要互斥
	std::unordered_map<uint64_t, Order> orders; 
	/***************************************************************************************
                                		用户索引
	****************************************************************************************/
	// 用户id下的所有订单
	std::unordered_map<uint64_t, OrderIndex> client_index;
    /***************************************************************************************
                                		读写锁
	****************************************************************************************/
    // 访问容器的读写锁
    std::shared_mutex rw_lock;
};
#endif