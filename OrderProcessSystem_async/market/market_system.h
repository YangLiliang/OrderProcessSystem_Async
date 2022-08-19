#ifndef MARKET_SYSTEM_H
#define MARKET_SYSTEM_H

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
#include "order_system.h"
class Timer;

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
struct SellAndBuyContainer{
	std::set<uint64_t> sell;
	std::set<uint64_t> buy;
	std::mutex* sell_mutex_ptr; // 对卖集合加锁
	std::mutex* buy_mutex_ptr; // 对买集合加锁
    SellAndBuyContainer();
};

// 市场系统 单例模式 饿汉模式
class MarketSystem{
public:
        // 获取实例
	static MarketSystem* getInstance(){
		//if(m_instance==NULL) m_instance=new TradingMarket();
		return m_instance;
	}
	// 创建订单并且保存执行结果
	uint64_t processCreateOrder(const NewOrderRequest&, std::vector<std::pair<uint64_t, ExecutionReport> >&);
	// 根据新订单请求做出应答消息
	void processNewOrder(const NewOrderRequest&, const uint64_t&, std::vector<std::pair<uint64_t, ExecutionReport> >&);
	// 根据撤销订单请求做出应答消息
	void processCancelOrder(const CancelOrderRequest&, ExecutionReport&);
	// 根据查询订单请求做出应答消息
	void processQueryOrder(const QueryOrderRequest&, std::vector<OrderReport>&);
	// 获取模拟撮合产生的消息
	void getMathchReports(std::vector<ExecutionReport>&);
	// 模拟撮合.订单数量为0返回false
	bool simulationMatch(const uint64_t&);
private:
	/***************************************************************************************
                                			构造函数
	****************************************************************************************/
	MarketSystem();
	/***************************************************************************************
                                			单例对象
	****************************************************************************************/
	static MarketSystem* m_instance;
    /***************************************************************************************
                                			订单系统
	****************************************************************************************/
    // 订单ID，动态增加，增加需要互斥
	uint64_t id;
	// 订单ID增加的互斥锁
	std::mutex orderID_mutex;
    // 创建订单
    uint64_t createOrder(const NewOrderRequest&, std::string&);
    // 订单系统
    OrderSystem orderSystem;
    // 市场价格
    double marketPrice;
    /***************************************************************************************
                                			计时器与模拟撮合
	****************************************************************************************/
	Timer* timer;
    // 订单撮合消息
	std::vector<ExecutionReport> matchReports;
	// 存储订单撮合消息互斥锁
	std::mutex matchReportsLock;
    /***************************************************************************************
                                		    股票索引
	****************************************************************************************/
	// 索引：股票ID对应需要被售卖或购买的容器，<stockID, sell and buy container>, 插入与删除需要互斥
	std::unordered_map<std::string, SellAndBuyContainer> stock_index;
	// 插入，删除和访问股票索引的读写锁
	std::shared_mutex rw_stock_index_mutex;
    /***************************************************************************************
                                	        股票索引操作相关
	****************************************************************************************/
	// 插入新股票
	void insertStock(const std::string&);
	// 将订单加入至待售卖容器
	void addOrderToSell(const std::string&, const uint64_t&);
	// 将订单加入至待购买容器
	void addOrderToBuy(const std::string&, const uint64_t&);
	// 将订单从售卖容器中删除
	void delOrderFromSell(const std::string&, const uint64_t&);
	// 将订单从购买容器中删除
	void delOrderFromBuy(const std::string&, const uint64_t&);
	// 判断该股票订单是否在容器中
	bool isStockExistsInHash(const std::string&);
    // 卖订单
	void sellOrders(const uint64_t&, const std::string&, const uint64_t&, std::vector<std::pair<uint64_t, ExecutionReport> >&);
	// 买订单
	void buyOrders(const uint64_t&, const std::string&, const uint64_t&, std::vector<std::pair<uint64_t, ExecutionReport> >&);
};

#endif