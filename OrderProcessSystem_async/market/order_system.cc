#ifndef ORDER_SYSTEM_CC
#define ORDER_SYSTEM_CC
#include "order_system.h"

// 订单索引构造函数
OrderIndex::OrderIndex(){
    rw_lock_=new std::mutex();
    sellOrderIndex.clear();
    buyOrderIndex.clear();
}

// 订单构造函数
Order::Order(const NewOrderRequest& info){
    info_=info;
    rw_lock_=new std::shared_mutex();
}

// 订单系统构造函数
OrderSystem::OrderSystem(){}

// 插入新订单
void OrderSystem::insertOrder(const uint64_t& orderID, const NewOrderRequest& request){
    // 创建新订单
    Order newOrder(request);
    // 获取用户ID
    uint64_t clientID=request.clientid();
    // 获取订单价格
    double price=request.price();
    // 获取订单类型
    bool type=(request.direction()==NewOrderRequest::SELL)?true:false;
    {
	    // 加锁,保护hash表的增删
	    std::unique_lock<std::shared_mutex> w(rw_lock);
        // 更新用户索引
        if(!client_index.count(clientID)){
            OrderIndex orderIndex;
            client_index.insert(std::make_pair(clientID, std::move(orderIndex)));
        }
        // 同一用户互斥
        std::unique_lock<std::mutex> w_(*client_index.at(clientID).rw_lock_);
        // 订单信息插入容器
	    orders.insert(std::make_pair(orderID, std::move(newOrder)));
        // 添加订单ID至用户索引
        if(type) client_index.at(clientID).sellOrderIndex[price].emplace(orderID);
        else client_index.at(clientID).buyOrderIndex[price].emplace(orderID);
    }
}

// 删除订单
bool OrderSystem::deleteOrder(const uint64_t& orderID){
    // 删除结果
    bool delRes=false;
    // 订单信息
    NewOrderRequest orderInfo;
    if(!getOrderInfo(orderID, orderInfo)){
        return delRes;
    }
    // 获取用户ID
    uint64_t clientID=orderInfo.clientid();
    // 获取订单价格
    double price=orderInfo.price();
    // 获取订单类型
    bool type;
    if(orderInfo.direction()==NewOrderRequest::SELL) type=true;
    else type=false;

	// 加锁,保护hash表的增删
	std::unique_lock<std::shared_mutex> w(rw_lock);
    // 同一用户互斥
    std::unique_lock<std::mutex> w_(*client_index.at(clientID).rw_lock_);
	// 如果订单存在则删除成功
	if(orders.find(orderID)!=orders.end()){
		orders.erase(orderID);
        delRes=true;
	}
    if(client_index.find(clientID)!=client_index.end()){
        // 从用户索引中删除订单ID
        if(type){
            if(client_index.at(clientID).sellOrderIndex.count(price)&&client_index.at(clientID).sellOrderIndex.at(price).count(orderID)){
                client_index.at(clientID).sellOrderIndex.at(price).erase(orderID);
                // 为空则删除
                if(client_index.at(clientID).sellOrderIndex.at(price).size()==0){
                    client_index.at(clientID).sellOrderIndex.erase(price);
                }
            }
        }else{
            if(client_index.at(clientID).buyOrderIndex.count(price)&&client_index.at(clientID).buyOrderIndex.at(price).count(orderID)){
                client_index.at(clientID).buyOrderIndex.at(price).erase(orderID);
                // 为空则删除
                if(client_index.at(clientID).buyOrderIndex.at(price).size()==0){
                    client_index.at(clientID).buyOrderIndex.erase(price);
                }
            }
        }
    }
	return delRes;
}

// 查询订单信息
bool OrderSystem::getOrderInfo(const uint64_t& orderID, NewOrderRequest& orderInfo){
	// 读锁
	std::shared_lock<std::shared_mutex> r(rw_lock);
	// 订单不存在
	if(orders.find(orderID)==orders.end()){
		return false;
	}
    Order& order=orders.at(orderID);
	// 订单存在
	std::shared_lock<std::shared_mutex> r_(*order.rw_lock_);
	orderInfo=order.info_;
	return true;
}

// 获取所有订单信息
void OrderSystem::getAllOrders(std::vector<OrderReport>& reports){
    // 订单信息
    NewOrderRequest orderInfo;
	// 读锁
	std::shared_lock<std::shared_mutex> r(rw_lock);
	for(const auto& [orderID, order]:orders){
        {
		    // 对订单加读锁
		    std::shared_lock<std::shared_mutex> r_(*order.rw_lock_);
            orderInfo=order.info_;
        }
		OrderReport report;
		initReport(report, orderInfo, orderID);
		reports.push_back(report);
	}
}

// 买卖订单交易
void OrderSystem::tradingOrders(const uint64_t& sellOrderID, const uint64_t& buyOrderID, const bool& direction, std::vector<std::pair<uint64_t, ExecutionReport> >& reports){
    // 成交价格
	double fillPrice=0.0;
    // 成交数量
    uint32_t tradeNum=0;
    // 订单信息
    NewOrderRequest sellOrderInfo, buyOrderInfo;
    {
	    // 对orders加读锁
	    std::shared_lock<std::shared_mutex> r(rw_lock);
        // 判断两订单是否都存在
        if(!orders.count(sellOrderID)||!orders.count(buyOrderID)) return;
	    Order& sellOrder=orders.at(sellOrderID);
	    Order& buyOrder=orders.at(buyOrderID);
        // 获取订单信息
        sellOrderInfo=sellOrder.info_;
        buyOrderInfo=buyOrder.info_;
        {
	        // 对买卖订单加写锁
	        std::unique_lock<std::shared_mutex> w1(*sellOrder.rw_lock_);
	        std::unique_lock<std::shared_mutex> w2(*buyOrder.rw_lock_);
	        // 计算可卖出的数量
	        tradeNum=std::min(buyOrderInfo.orderqty(), sellOrderInfo.orderqty());
            // 从数据库中修改两订单的库存量
	        buyOrder.info_.set_orderqty(buyOrderInfo.orderqty()-tradeNum);
	        sellOrder.info_.set_orderqty(sellOrderInfo.orderqty()-tradeNum);
        }
    }
    // 获取交易价格
    if(direction==true){ // 卖
	    fillPrice=buyOrderInfo.price();
	}else{ // 买
	    fillPrice=sellOrderInfo.price();
	}
	// 设置sell订单交易成功的应答
	ExecutionReport sellReport;
	initReport(sellReport, sellOrderInfo);
	sellReport.set_stat(ExecutionReport::FILL);
	sellReport.set_orderid(sellOrderID);
	sellReport.set_fillqty(tradeNum);
	sellReport.set_fillprice(fillPrice);
	sellReport.set_leaveqty(sellOrderInfo.orderqty()-tradeNum);
	sellReport.set_time(getTime());
	// 设置buy订单交易成功的应答
	ExecutionReport buyReport;
	initReport(buyReport, buyOrderInfo);
	buyReport.set_stat(ExecutionReport::FILL);
	buyReport.set_orderid(buyOrderID);
	buyReport.set_fillqty(tradeNum);
	buyReport.set_fillprice(fillPrice);
	buyReport.set_leaveqty(buyOrderInfo.orderqty()-tradeNum);
	buyReport.set_time(getTime());
	// 存储report
	reports.push_back(std::make_pair(sellOrderID, sellReport));
	reports.push_back(std::make_pair(buyOrderID, buyReport));
}

// 将市价单的价格更新为市价
void OrderSystem::updateToMarketPrice(const uint64_t& orderID, const double& marketPrice){
    // 订单信息
    NewOrderRequest orderInfo;
	// 对orders加读锁
	std::shared_lock<std::shared_mutex> r1(rw_lock);
    if(!orders.count(orderID)) return;
	{
		// 对订单加读锁
		std::shared_lock<std::shared_mutex> r_(*orders.at(orderID).rw_lock_);
		if(orders.at(orderID).info_.ordertype()==NewOrderRequest::LIMIT){
			return;
		}
        orderInfo=orders.at(orderID).info_;
	}
    uint64_t clientID=orderInfo.clientid();
    bool type=(orderInfo.direction()==NewOrderRequest::SELL)?true:false;
    double price=orderInfo.price();
    if(!client_index.count(clientID)){
        return;
    }
    // 同一用户互斥
    std::unique_lock<std::mutex> w_(*client_index.at(clientID).rw_lock_);
    {
        // 对订单加写锁
	    std::unique_lock<std::shared_mutex> w_(*orders.at(orderID).rw_lock_);
	    orders.at(orderID).info_.set_price(marketPrice);
    }
    {
        // 修改用户索引
        if(type){
            if(client_index.at(clientID).sellOrderIndex.count(price)&&client_index.at(clientID).sellOrderIndex.at(price).count(orderID)){
                client_index.at(clientID).sellOrderIndex.at(price).erase(orderID);
                // 为空则删除
                if(client_index.at(clientID).sellOrderIndex.at(price).size()==0){
                    client_index.at(clientID).sellOrderIndex.erase(price);
                }
            }
            client_index.at(clientID).sellOrderIndex[marketPrice].emplace(orderID);
        }else{
            if(client_index.at(clientID).buyOrderIndex.count(price)&&client_index.at(clientID).buyOrderIndex.at(price).count(orderID)){
                client_index.at(clientID).buyOrderIndex.at(price).erase(orderID);
                // 为空则删除
                if(client_index.at(clientID).buyOrderIndex.at(price).size()==0){
                     client_index.at(clientID).buyOrderIndex.erase(price);
                  }
            }
            client_index.at(clientID).buyOrderIndex[marketPrice].emplace(orderID);
        }
    }
}

// 模拟撮合
bool OrderSystem::simulationMatch(const uint64_t& orderID, ExecutionReport& report_){
	// 订单信息
	NewOrderRequest orderInfo;
    // 原订单数量和撮合数量
	uint64_t originQty;
	uint64_t matchQty;
	// 对orders加读锁
	{
		std::shared_lock<std::shared_mutex> r(rw_lock);
		// 订单已被删除
		if(orders.find(orderID)==orders.end()){
			return false;
		}
		// 订单
		Order& order=orders.at(orderID);
		{
			// 对订单加写锁
			std::unique_lock<std::shared_mutex> w(*order.rw_lock_);
			// 获取订单信息
			orderInfo=order.info_;
			// 获取订单数量
			originQty=orderInfo.orderqty();
			// 匹配的数量
			matchQty=(originQty/2-(originQty/2)%100)==0?originQty:(originQty/2-(originQty/2)%100);
			if(matchQty==0) return false;
			orders.at(orderID).info_.set_orderqty(originQty-matchQty);
		}
	}

	// 初始化应答消息
	initReport(report_, orderInfo);
	// 设置应答消息
	report_.set_stat(ExecutionReport::FILL);
	report_.set_orderid(orderID);
	report_.set_orderqty(originQty);
	report_.set_fillqty(matchQty);
	report_.set_fillprice(orderInfo.price());
	report_.set_leaveqty(originQty-matchQty);
	report_.set_time(getTime());
	return true;
}

// 判断是否是对敲订单
bool OrderSystem::isImproperMatchedOrder(const NewOrderRequest& request){
	// 获取用户ID
	uint64_t clientID=request.clientid();
    // 获取订单价格
    double price=request.price();
	// 判断订单类型并遍历判断是否对敲
	if(request.direction()==NewOrderRequest::BUY){
        // 读锁
	    std::shared_lock<std::shared_mutex> r(rw_lock);
        // 存在该用户
        if(client_index.count(clientID)){
            // 获取卖订单ID集合
		    OrderIndex& orderIndex=client_index.at(clientID);
            // 互斥锁
	        std::unique_lock<std::mutex> w_(*orderIndex.rw_lock_);
            // 判断是否存在价格比buy订单一样或更小的sell订单
            if(orderIndex.sellOrderIndex.size()>0&&orderIndex.sellOrderIndex.upper_bound(price)!=orderIndex.sellOrderIndex.begin()){
                return true;
            }
        }
	}else{
		// 读锁
	    std::shared_lock<std::shared_mutex> r(rw_lock);
        // 存在该用户
        if(client_index.count(clientID)){
		    // 获取卖订单ID集合
		    OrderIndex& orderIndex=client_index.at(clientID);
            // 互斥锁
	        std::unique_lock<std::mutex> w_(*orderIndex.rw_lock_);
            // 判断是否存在价格比sell订单一样或更大的buy订单
            if(orderIndex.buyOrderIndex.size()>0&&orderIndex.buyOrderIndex.lower_bound(price)!=orderIndex.buyOrderIndex.end()){
                return true;
            }
        }
	}
	return false;
}
#endif