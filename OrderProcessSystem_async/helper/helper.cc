#ifndef HELPER_CC
#define HELPER_CC
#include "helper.h"

// 获取时间
std::string getTime(){
	std::time_t cur;
	time(&cur);
	std::string time_str=ctime(&cur);
	return time_str;
}

uint64_t getTimestamp(){
	timeb t;
	ftime(&t);
	return t.time*1000+t.millitm;
}


void printRequest(const NewOrderRequest& request){
	std::cout<<"报单请求: "<<std::endl;
	std::cout<<"	客户ID: "<<request.clientid()<<", "<<std::endl;
	if(request.direction()==NewOrderRequest::SELL) std::cout<<"	订单类型: [SELL]"<<", "<<std::endl;
	else std::cout<<"	订单类型: [BUY]"<<", "<<std::endl;
	std::cout<<"	股票ID: "<<request.stockid()<<", "<<std::endl;
	std::cout<<"	订单数量: "<<request.orderqty()<<", "<<std::endl;
	std::cout<<"	订单价格: "<<request.price()<<", "<<std::endl;
	if(request.ordertype()==NewOrderRequest::LIMIT) std::cout<<"	价格类型: [LIMIT]"<<", "<<std::endl;
	else std::cout<<"	价格类型: [CURRENT]"<<", "<<std::endl;
	std::cout<<"	报单时间: "<<request.time();
	std::cout<<std::endl;
}

void printRequest(const CancelOrderRequest& request){
	std::cout<<"撤单请求: "<<std::endl;
	std::cout<<"	订单ID: "<<request.orderid()<<", "<<std::endl;
	std::cout<<"	撤单时间:  "<<request.time();
	std::cout<<std::endl;
}

void printReport(const ExecutionReport& report){
	std::cout<<"执行结果: "<<std::endl;
	if(report.stat()==ExecutionReport::ORDER_ACCEPT){
		std::cout<<"	[报单接受 ORDER_ACCEPT]"<<", "<<std::endl;
	}else if(report.stat()==ExecutionReport::ORDER_REJECT){
		std::cout<<"	[报单拒绝 ORDER_REJECT]"<<", "<<std::endl;
	}else if(report.stat()==ExecutionReport::FILL){
		std::cout<<"	[交易成功 FILL]"<<", "<<std::endl;
	}else if(report.stat()==ExecutionReport::CANCELED){
		std::cout<<"	[订单取消 CANCELED]"<<", "<<std::endl;
	}else{
		std::cout<<"	[撤单拒绝 CANCEL_REJECT]"<<", "<<std::endl;
	}
	if(report.errormessage().size()>0){
		std::cout<<"	错误信息: "<<report.errormessage()<<", "<<std::endl;
	}
	else{
		std::cout<<"	客户ID: "<<report.clientid()<<", "<<std::endl;
		std::cout<<"	订单ID: "<<report.orderid()<<", "<<std::endl;
		std::cout<<"	股票ID: "<<report.stockid()<<", "<<std::endl;
		std::cout<<"	订单数量: "<<report.orderqty()<<", "<<std::endl;
		std::cout<<"	订单价格: "<<report.orderprice()<<", "<<std::endl;
		std::cout<<"	交易数量: "<<report.fillqty()<<", "<<std::endl;
		std::cout<<"	交易价格: "<<report.fillprice()<<", "<<std::endl;
		std::cout<<"	剩余数量: "<<report.leaveqty()<<", "<<std::endl;
		std::cout<<"	交易时间: "<<report.time();
	}
	std::cout<<std::endl;
}

void printReport(const OrderReport& report){
	std::cout<<"查询结果: "<<std::endl;
	std::cout<<"	订单ID: "<<report.orderid()<<", "<<std::endl;
	std::cout<<"	客户ID: "<<report.clientid()<<", "<<std::endl;
	if(report.direction()==OrderReport::SELL) std::cout<<"	订单类型: [SELL]"<<", "<<std::endl;
	else std::cout<<"	订单类型: [BUY]"<<", "<<std::endl;
	std::cout<<"	股票ID: "<<report.stockid()<<", "<<std::endl;
	std::cout<<"	订单数量: "<<report.orderqty()<<", "<<std::endl;
	std::cout<<"	订单价格: "<<report.price()<<", "<<std::endl;
	if(report.ordertype()==OrderReport::LIMIT) std::cout<<"	价格类型: [LIMIT]"<<", "<<std::endl;
	else std::cout<<"	价格类型: [CURRENT]"<<", "<<std::endl;
	std::cout<<"	报单时间: "<<report.time();
	std::cout<<std::endl;
}

// 判断订单的合法性
bool checkRequest(const NewOrderRequest& request, std::string& errorMessage){
	if(request.clientid()<=0){
		errorMessage="Error: ClientID is illegal!";
	}else if(request.stockid().size()<=0){
		errorMessage="Error: StockID is illegal!";
	}else if(request.direction()!=NewOrderRequest::SELL&&request.direction()!=NewOrderRequest::BUY){
		errorMessage="Error: Order direction is illegal!";
	}else if(request.orderqty()<=0){
		errorMessage="Error: Order quantity is illegal!";
	}else if(request.price()<=0){
		errorMessage="Error: Order price is illegal!";
	}else if(request.ordertype()!=NewOrderRequest::LIMIT&&request.ordertype()!=NewOrderRequest::MARKET){
		errorMessage="Error: Order type is illegal!";
	}
	if(errorMessage.size()>0) return false;
	return true;
}

// 初始化应答
void initReport(ExecutionReport& report, const NewOrderRequest& request){
	// 订单状态
	report.set_stat(ExecutionReport::ORDER_REJECT);
	// 客户ID
	report.set_clientid(request.clientid());
	// 订单ID
	report.set_orderid(0);
	// 股票代码
	report.set_stockid(request.stockid());
	// 订单总量
	report.set_orderqty(request.orderqty());
	// 订单价格
	report.set_orderprice(request.price());
	// 订单成交数量
	report.set_fillqty(0);
	// 订单成交价格
	report.set_fillprice(0);
	// 剩余待成交数量
	report.set_leaveqty(request.orderqty());
	// 错误信息
	report.set_errormessage("");
	// 时间
	report.set_time("");
}
// 初始化应答
void initReport(ExecutionReport& report, const CancelOrderRequest& request){
	report.set_stat(ExecutionReport::CANCEL_REJECT);
	report.set_clientid(0);
	report.set_orderid(request.orderid());
	report.set_stockid("");
	report.set_orderqty(0);
	report.set_orderprice(0);
	report.set_fillqty(0);
	report.set_fillprice(0);
	report.set_leaveqty(0);
	report.set_errormessage("");
	report.set_time("");
}
// 初始化应答
void initReport(OrderReport& report, const NewOrderRequest& request, const uint64_t& orderID){
	report.set_orderid(orderID);
	if(request.ordertype()==NewOrderRequest::LIMIT) report.set_ordertype(OrderReport::LIMIT);
	else report.set_ordertype(OrderReport::MARKET);
	
	if(request.direction()==NewOrderRequest::SELL) report.set_direction(OrderReport::SELL);
	else report.set_direction(OrderReport::BUY);
	
	report.set_clientid(request.clientid());
	report.set_stockid(request.stockid());
	report.set_orderqty(request.orderqty());
	report.set_price(request.price());
	report.set_time(request.time());
}

// 创建新订单请求
NewOrderRequest MakeNewOrderRequest(const bool& type, const bool& direction, 
				const uint64_t& clientID, const std::string& stockID,
				const uint32_t& orderQty, const double& price){
	NewOrderRequest request;
	if(type==TYPE_LIMIT) request.set_ordertype(NewOrderRequest::LIMIT);
	else request.set_ordertype(NewOrderRequest::MARKET);
	
	if(direction==DIRE_SELL) request.set_direction(NewOrderRequest::SELL);
	else request.set_direction(NewOrderRequest::BUY);
	
	request.set_clientid(clientID);
	request.set_stockid(stockID);
	request.set_orderqty(orderQty);
	request.set_price(price);
	request.set_time(getTime());
	return request;
}

// 创建撤销订单请求
CancelOrderRequest MakeCancelOrderRequest(const uint64_t& orderID){
	CancelOrderRequest request;
	request.set_orderid(orderID);
	request.set_time(getTime());
	return request;
}

// 创建查询订单请求
QueryOrderRequest MakeQueryOrderRequest(){
	QueryOrderRequest request;
	request.set_time(getTime());
	return request;
}

// 创建发送消息请求
SendMessageRequest MakeSendMessageRequest(){
    SendMessageRequest request;
    request.set_time(getTime());
	return request;
}
#endif 