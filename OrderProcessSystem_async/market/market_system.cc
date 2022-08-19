#ifndef MARKET_SYSTEM_CC
#define MARKET_SYSTEM_CC
#include "market_system.h"
#include "../timer/timer.h"

// 构造函数
SellAndBuyContainer::SellAndBuyContainer(){
    sell_mutex_ptr=new std::mutex();
    buy_mutex_ptr=new std::mutex();
	sell.clear();
	buy.clear();
}

MarketSystem* MarketSystem::m_instance=new MarketSystem;

// 构造函数
MarketSystem::MarketSystem(){
    id=0;
	marketPrice=5.0;
	timer=new Timer();
	std::thread t(&Timer::run, timer);
	t.detach();
}

// 创建订单
uint64_t MarketSystem::createOrder(const NewOrderRequest& request, std::string& errorMessage){
	// 判断订单的合法性
	if(!checkRequest(request, errorMessage)){
		return 0;
	}
	// 获取订单的用户ID
	uint64_t clientID=request.clientid();
	// 初始化订单ID
	uint64_t orderID;
	// 判断是否是对敲
	if(orderSystem.isImproperMatchedOrder(request)){
		errorMessage="Improper Matched Order!";
		return 0;
	}
	{
		// 加锁,保护订单编号动态增加,作用域结束自动解锁
		std::unique_lock<std::mutex> w_(orderID_mutex);
		// 为订单分配ID
		orderID=++id;
	}
	// 获取订单对应的股票ID
	std::string stockID=request.stockid();
	// 为stockID分配容器对象和锁
	if(!isStockExistsInHash(stockID)){
		insertStock(stockID);
	}
	// 将订单存入订单集合中
    orderSystem.insertOrder(orderID, request);
	return orderID;
}

// 创建订单并且保存执行结果
uint64_t MarketSystem::processCreateOrder(const NewOrderRequest& request, std::vector<std::pair<uint64_t, ExecutionReport> >& reports){
	// 错误信息
	std::string errorMessage="";

	// 初始化应答
	ExecutionReport report;
	initReport(report, request);

	// 创建订单
	uint64_t orderID=0;
	if((orderID=createOrder(request, errorMessage))==0){
		// 订单创建失败的消息
		report.set_time(getTime());
		report.set_errormessage(errorMessage);
		reports.push_back(std::make_pair(0, report));
	}else{
		// 输出订单创建成功的消息
		report.set_stat(ExecutionReport::ORDER_ACCEPT);
		report.set_orderid(orderID);
		report.set_time(getTime());
		reports.push_back(std::make_pair(orderID, report));
	}
	// 将订单ID返回给服务器
	return orderID;
}

// 根据新订单请求做出应答消息
void MarketSystem::processNewOrder(const NewOrderRequest& request, const uint64_t& orderID, std::vector<std::pair<uint64_t, ExecutionReport> >& reports){
	// 获取订单对应的股票ID
	auto stockID=request.stockid();
	// 获取订单对应的用户ID
	auto clientID=request.clientid();

    // 自动撮合订单
    if(isStockExistsInHash(stockID)){
        if(request.direction()==NewOrderRequest::SELL){
            // 存在该股票, 搜索买订单
            sellOrders(orderID, stockID, clientID, reports);
        }else{
            // 存在该股票, 搜索卖订单
            buyOrders(orderID, stockID, clientID, reports);
        }
    }

    // 订单信息
    NewOrderRequest orderInfo;	
    // 剩余待购买订单数不为0, 加入buy集合, 否则从订单集合中删除该订单
	if(orderSystem.getOrderInfo(orderID, orderInfo)&&orderInfo.orderqty()>0){
        // 将订单挂在股票索引上等待购买
        if(request.direction()==NewOrderRequest::SELL){
            addOrderToSell(stockID, orderID);
        }else{
            addOrderToBuy(stockID, orderID);
        }
        // 添加计时任务至计时器
		timer->addTask(orderID);
    }else{
        // 删除订单
        orderSystem.deleteOrder(orderID);
    }
}

// 根据撤销订单请求做出应答消息
void MarketSystem::processCancelOrder(const CancelOrderRequest& request, ExecutionReport& report){
	// 错误信息
	std::string errorMessage="";
	uint64_t orderID=request.orderid();
    // 订单信息
	NewOrderRequest orderInfo;
	// 获取order信息
	if(!orderSystem.getOrderInfo(orderID, orderInfo)){
		errorMessage="Error: Can not find OrderID!";
		report.set_time(getTime());
		report.set_errormessage(errorMessage);
		return;	
	}
	std::string stockID=orderInfo.stockid();
	uint64_t clientID=orderInfo.clientid();
	
	// 从定时器中删除任务
	timer->delTask(orderID);

	if(orderInfo.direction()==NewOrderRequest::SELL){
		// 从卖集合容器中删除订单
		delOrderFromSell(stockID, orderID);
	}else{
		// 从买集合容器中删除订单
		delOrderFromBuy(stockID, orderID);
	}
	{
		// 从订单容器中删除订单
		if(!orderSystem.deleteOrder(orderID)){
			errorMessage="Error: Can not find OrderID!";
			report.set_time(getTime());
			report.set_errormessage(errorMessage);
			return;	
		}
	}

	report.set_stat(ExecutionReport::CANCELED);
	report.set_clientid(orderInfo.clientid());
	report.set_stockid(stockID);
	report.set_orderqty(orderInfo.orderqty());
	report.set_orderprice(orderInfo.price());
	report.set_leaveqty(orderInfo.orderqty());
	report.set_time(getTime());
}

// 根据查询订单请求做出应答消息
void MarketSystem::processQueryOrder(const QueryOrderRequest& request, std::vector<OrderReport>& reports){
	orderSystem.getAllOrders(reports);
	std::sort(reports.begin(), reports.end(), [&](const OrderReport& a, const OrderReport& b){return a.orderid()<b.orderid();});
}

// 获取模拟撮合产生的消息
void MarketSystem::getMathchReports(std::vector<ExecutionReport>& reports_){
	std::unique_lock<std::mutex> w(matchReportsLock);
	reports_=std::move(matchReports);
	// matchReports.clear();
}

// 模拟撮合
bool MarketSystem::simulationMatch(const uint64_t& orderID){
    // 撮合消息
	ExecutionReport report_;
    if(!orderSystem.simulationMatch(orderID, report_)){
        return false;
    }
    // 保存report
	std::unique_lock<std::mutex> w(matchReportsLock);
	matchReports.push_back(std::move(report_));
	w.unlock();

	NewOrderRequest orderInfo;
	// 订单不存在
	if(!orderSystem.getOrderInfo(orderID, orderInfo)){
		return false;
	}
	// 判断订单数量是否为0，为0删除订单
	if(orderInfo.orderqty()==0){
		if(orderInfo.direction()==NewOrderRequest::SELL){
			// 从卖集合容器中删除订单
			delOrderFromSell(orderInfo.stockid(), orderID);
		}else{
			// 从买集合容器中删除订单
			delOrderFromBuy(orderInfo.stockid(), orderID);
		}
        // 从订单系统中删除订单
		orderSystem.deleteOrder(orderID);
		return false;
	}
	return true;
}

/***************************************************************************************
                                    股票索引相关操作
****************************************************************************************/

// 插入新股票
void MarketSystem::insertStock(const std::string& stockID){
	// 写锁
	std::unique_lock<std::shared_mutex> w(rw_stock_index_mutex);
	if(stock_index.find(stockID)==stock_index.end()){
		SellAndBuyContainer container=SellAndBuyContainer();
		stock_index.insert(std::make_pair(stockID, std::move(container)));
	}
}

// 将订单加入至待售卖容器
void MarketSystem::addOrderToSell(const std::string& stockID, const uint64_t& orderID){
	// 读锁
	std::shared_lock<std::shared_mutex> r(rw_stock_index_mutex);
	// 获取锁
	std::mutex* sell_mutex_ptr=stock_index.at(stockID).sell_mutex_ptr;
	// 对stockID的卖订单集合加锁,作用域结束自动解锁
	std::unique_lock<std::mutex> w(*sell_mutex_ptr);
	stock_index.at(stockID).sell.emplace(orderID);
}

// 将订单加入至待购买容器
void MarketSystem::addOrderToBuy(const std::string& stockID, const uint64_t& orderID){
	// 读锁
	std::shared_lock<std::shared_mutex> r(rw_stock_index_mutex);
	// 获取锁
	std::mutex* buy_mutex_ptr=stock_index.at(stockID).buy_mutex_ptr;
	// 对stockID的卖订单集合加锁,作用域结束自动解锁
	std::unique_lock<std::mutex> w(*buy_mutex_ptr);
	stock_index.at(stockID).buy.emplace(orderID);
}

// 将订单从售卖容器中删除
void MarketSystem::delOrderFromSell(const std::string& stockID, const uint64_t& orderID){
	// 读锁
	std::shared_lock<std::shared_mutex> r(rw_stock_index_mutex);
	// 对stockID的卖订单集合加锁,作用域结束自动解锁
	std::unique_lock<std::mutex> w(*stock_index.at(stockID).sell_mutex_ptr);
	if(stock_index.at(stockID).sell.find(orderID)!=stock_index.at(stockID).sell.end()){
		stock_index.at(stockID).sell.erase(orderID);
	}
}

// 将订单从购买容器中删除
void MarketSystem::delOrderFromBuy(const std::string& stockID, const uint64_t& orderID){
	// 读锁
	std::shared_lock<std::shared_mutex> r(rw_stock_index_mutex);
	// 对stockID的卖订单集合加锁,作用域结束自动解锁
	std::unique_lock<std::mutex> w(*stock_index.at(stockID).buy_mutex_ptr);
	if(stock_index.at(stockID).buy.find(orderID)!=stock_index.at(stockID).buy.end()){
		stock_index.at(stockID).buy.erase(orderID);
	}
}

// 判断该股票订单是否在容器中
bool MarketSystem::isStockExistsInHash(const std::string& stockID){
	// 读锁
	std::shared_lock<std::shared_mutex> r(rw_stock_index_mutex);
	if(stock_index.find(stockID)!=stock_index.end()){
		return true;
	}
	return false;
}

// 卖订单操作
void MarketSystem::sellOrders(const uint64_t& sellOrderID, const std::string& stockID, const uint64_t& clientID,
		std::vector<std::pair<uint64_t, ExecutionReport> >& reports){
    // 加读锁
    std::shared_lock<std::shared_mutex> r(rw_stock_index_mutex);
    // 不存在股票ID则退出
    if(stock_index.find(stockID)==stock_index.end()){
        return;
    }
	// 获取买订单容器
	auto& buyOrderSet=stock_index.at(stockID).buy;
    // 获取买订单容器锁
    auto& buyOrderSetLock=stock_index.at(stockID).buy_mutex_ptr;
    r.unlock();
	// 对buyOrderSet加锁,作用域结束自动解锁
	std::unique_lock<std::mutex> w(*buyOrderSetLock);
		// 遍历容器
	for(auto it=buyOrderSet.begin(); it!=buyOrderSet.end();){
		NewOrderRequest sellOrderInfo, buyOrderInfo;
		// 卖订单数量为0，结束遍历
		if(!orderSystem.getOrderInfo(sellOrderID, sellOrderInfo)||sellOrderInfo.orderqty()==0){
			break;
		} 
		auto buyOrderID=*it;
        if(!orderSystem.getOrderInfo(buyOrderID, buyOrderInfo)||buyOrderInfo.orderqty()==0){
            // 无该买订单或该买订单数量为0
            buyOrderSet.erase(it++);
            orderSystem.deleteOrder(buyOrderID);
            continue;
        }else{
            // 存在订单并符合交易价格
            if(sellOrderInfo.clientid()!=buyOrderInfo.clientid()&&sellOrderInfo.price()<=buyOrderInfo.price()){
                // 删除计时任务
                timer->delTask(buyOrderID);
                orderSystem.tradingOrders(sellOrderID, buyOrderID, true, reports);
                // 再次检查订单剩余数量
                if(!orderSystem.getOrderInfo(buyOrderID, buyOrderInfo)||buyOrderInfo.orderqty()==0){
                    buyOrderSet.erase(it++);
                    orderSystem.deleteOrder(buyOrderID);
                    continue;
                }else{
                    // 添加计时任务
                    timer->addTask(buyOrderID);
                }
            }
            it++;
        }
	}
}

// 买订单操作
void MarketSystem::buyOrders(const uint64_t& buyOrderID, const std::string& stockID, const uint64_t& clientID,
		std::vector<std::pair<uint64_t, ExecutionReport> >& reports){
    // 加读锁
    std::shared_lock<std::shared_mutex> r(rw_stock_index_mutex);
    // 不存在股票ID则退出
    if(stock_index.find(stockID)==stock_index.end()){
        return;
    }
	// 获取卖订单容器
	auto& sellOrderSet=stock_index.at(stockID).sell;
    // 获取买订单容器锁
    auto sellOrderSetLock=stock_index.at(stockID).sell_mutex_ptr;
    r.unlock();
	// 对buyOrderSet加锁,作用域结束自动解锁
	std::unique_lock<std::mutex> w(*sellOrderSetLock);
		// 遍历容器
	for(auto it=sellOrderSet.begin(); it!=sellOrderSet.end();){
		NewOrderRequest sellOrderInfo, buyOrderInfo;
		// 卖订单数量为0，结束遍历
		if(!orderSystem.getOrderInfo(buyOrderID, buyOrderInfo)||buyOrderInfo.orderqty()==0){
			break;
		} 
		auto sellOrderID=*it;
        if(!orderSystem.getOrderInfo(sellOrderID, sellOrderInfo)||sellOrderInfo.orderqty()==0){
            // 无该买订单或该买订单数量为0
            sellOrderSet.erase(it++);
            orderSystem.deleteOrder(sellOrderID);
            continue;
        }else{
            // 存在订单并符合交易价格
            if(sellOrderInfo.clientid()!=buyOrderInfo.clientid()&&sellOrderInfo.price()<=buyOrderInfo.price()){
                // 删除计时任务
                timer->delTask(sellOrderID);
                orderSystem.tradingOrders(sellOrderID, buyOrderID, false, reports);
                // 再次检查订单剩余数量
                if(!orderSystem.getOrderInfo(sellOrderID, sellOrderInfo)||sellOrderInfo.orderqty()==0){
                    sellOrderSet.erase(it++);
                    orderSystem.deleteOrder(sellOrderID);
                    continue;
                }else{
                    // 添加计时任务
                    timer->addTask(sellOrderID);
                }
            }
            it++;
        }
	}
}
#endif