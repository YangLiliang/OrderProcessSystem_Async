#include"async_server.h"

// 基类
CommonCallData::CommonCallData(OrderService::AsyncService* service, ServerCompletionQueue* cq, MarketSystem* marketSystem):
	service_(service), cq_(cq), marketSystem_(marketSystem), status_(CREATE){}

// 处理新订单类
CallDataPushNewOrder::CallDataPushNewOrder(OrderService::AsyncService* service, ServerCompletionQueue* cq, MarketSystem* tradingMarket):
		CommonCallData(service, cq, tradingMarket), responder_(&ctx_), new_responder_created_(false), writing_mode_(false), RequestsCounter_(0), ReportsCounter_(0){
	Proceed();
}

void CallDataPushNewOrder::Proceed(bool ok) {
	if(status_==CREATE){
		status_=PROCESS;
		service_->RequestPushNewOrder(&ctx_, &responder_, cq_, cq_, (void*)this);	
	}else if(status_==PROCESS){
		if(!new_responder_created_){
			new CallDataPushNewOrder(service_, cq_, marketSystem_);
			new_responder_created_=true;
		}
		if(!writing_mode_){
			if(!ok){
				//std::cout<<"Reading finish!"<<std::endl;
				writing_mode_=true;
				ok=true;	
			}else{
				//std::cout<<"Reading request..."<<std::endl;
				responder_.Read(&newOrderRequest_, (void*)this);
				// printRequest(newOrderRequest_);
				if(newOrderRequest_.clientid()>0){
					// std::cout<<1<<std::endl;
					uint64_t orderID=marketSystem_->processCreateOrder(newOrderRequest_, reports_);
					// std::cout<<2<<std::endl;
					// std::cout<<orderID<<std::endl;
					if(orderID>0){
						// (orderID_responder_)[orderID]=&responder_;
						Responders::add(orderID, &responder_);
						// std::cout<<3<<std::endl;
						marketSystem_->processNewOrder(newOrderRequest_, orderID, reports_);
						// std::cout<<orderID<<"ok"<<std::endl;
						// std::cout<<4<<std::endl;
					}
				}
			}		
		}
		if(writing_mode_){
			if(!ok||ReportsCounter_>=reports_.size()){
				//status_=FINISH;
				//responder_.Finish(Status(), (void*)this);	
			}else{
				auto& orderID= reports_[ReportsCounter_].first;
				auto& report=reports_[ReportsCounter_].second;
				// printReport(report);
				if(orderID==0){
					responder_.Write(report, (void*)this);
				}else{
					ServerAsyncReaderWriter<ExecutionReport, NewOrderRequest>* stream;
					Responders::get(orderID, stream);
					stream->Write(report, (void*)this);
				}
				++ReportsCounter_;
			}
		}
	}else{
		std::cout<<"Delete!"<<std::endl;
		// delete this;
	}	
}

// 处理撤销订单
CallDataPushCancelOrder::CallDataPushCancelOrder(OrderService::AsyncService* service, ServerCompletionQueue* cq, MarketSystem* marketSystem):
		CommonCallData(service, cq, marketSystem), responder_(&ctx_){
	Proceed();
}
void CallDataPushCancelOrder::Proceed(bool ok) {
	if(status_==CREATE){
		status_=PROCESS;
		service_->RequestPushCancelOrder(&ctx_, &cancelOrderRequest_, &responder_, cq_, cq_, this);	
	}else if(status_==PROCESS){
		new CallDataPushCancelOrder(service_, cq_, marketSystem_);
		// printRequest(cancelOrderRequest_);
		initReport(report_, cancelOrderRequest_);
		marketSystem_->processCancelOrder(cancelOrderRequest_,  report_);
		// printReport(report_);
		status_=FINISH;
		responder_.Finish(report_, Status::OK, this);
	}else{
		GPR_ASSERT(status_==FINISH);
		delete this;
	}
}

// 处理查询订单
CallDataPushQueryOrder::CallDataPushQueryOrder(OrderService::AsyncService* service, ServerCompletionQueue* cq, MarketSystem* marketSystem):
	CommonCallData(service, cq, marketSystem), responder_(&ctx_), new_responder_created_(false), reportsCounter_(0){
		Proceed();
}

void CallDataPushQueryOrder::Proceed(bool ok){
	if(status_ == CREATE){
		status_ = PROCESS ;
		service_->RequestPushQueryOrder(&ctx_, &queryOrderRequest_, &responder_, cq_, cq_, this);
	}
	else if(status_ == PROCESS){
		if(!new_responder_created_){
			new CallDataPushQueryOrder(service_, cq_, marketSystem_);
			new_responder_created_ = true ;
			marketSystem_->processQueryOrder(queryOrderRequest_, queryOrderReports_);
		}
		if(reportsCounter_ >= queryOrderReports_.size()){
			status_ = FINISH;
			responder_.Finish(Status(), (void*)this);
		}
		else{
			responder_.Write(queryOrderReports_[reportsCounter_], (void*)this);
			++reportsCounter_;
		}
	}
	else if(status_ == FINISH){
		delete this;
	}
}

// 处理查询模拟撮合结果
CallDataPushSendMessage::CallDataPushSendMessage(OrderService::AsyncService* service, ServerCompletionQueue* cq, MarketSystem* marketSystem):
	CommonCallData(service, cq, marketSystem), responder_(&ctx_), new_responder_created_(false), ReportsCounter_(0){
		Proceed();
}

// TODO： 修复bug
void CallDataPushSendMessage::Proceed(bool ok){
	if(status_ == CREATE){
		status_ = PROCESS ;
		service_->RequestPushSendMessage(&ctx_, &sendMessageRequest_, &responder_, cq_, cq_, this);
	}else if(status_ == PROCESS){
		if(!new_responder_created_){
			new CallDataPushSendMessage(service_, cq_, marketSystem_);
			new_responder_created_ = true ;
			marketSystem_->getMathchReports(reports_);
		}
		if(ReportsCounter_>=reports_.size()){
			status_ = FINISH;
			responder_.Finish(Status(), (void*)this);
		}else{
			auto report=reports_[ReportsCounter_];
			auto orderID=report.orderid();
			// 获取读写流
			ServerAsyncReaderWriter<ExecutionReport, NewOrderRequest>* stream;
			Responders::get(orderID, stream);
			// E0819 13:08:21.206054622   17285 proto_buffer_writer.h:65]   assertion failed: !byte_buffer->Valid()
			// 已放弃 (核心已转储)
			stream->Write(report, (void*)this);
			++ReportsCounter_;
		}
	}else{
		delete this;
	}
}

void ServerImpl::getSimulateMatchReports(){
	LoopRequest loopRequest(grpc::CreateChannel("localhost:50010", grpc::InsecureChannelCredentials()));
	std::thread thread_=std::thread(&LoopRequest::AsyncCompleteRpc, &loopRequest);
	while(1){
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		loopRequest.PushSendMessage();
	}
	thread_.join();
}

// 服务端类
void ServerImpl::Run(){
	std::string server_address("0.0.0.0:50010");
	ServerBuilder builder;
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	// 注册服务
	builder.RegisterService(&service_);
	// 建立完成队列
	cq_=builder.AddCompletionQueue();
	server_=builder.BuildAndStart();
	std::cout<<"Server listening on: "<<server_address<<std::endl;	
	std::thread thread1=std::thread(&ServerImpl::getSimulateMatchReports, this);
	// 处理服务器的主循环
	for(int i=0;i<9;i++){
		// 创建多线程处理
		std::thread thread_=std::thread(&ServerImpl::HandleRpcs, this);
		thread_.join();
	}
}

// 主循环
void ServerImpl::HandleRpcs(){
	// 注册请求处理
	new CallDataPushNewOrder(&service_, cq_.get(), marketSystem_);
	new CallDataPushCancelOrder(&service_, cq_.get(), marketSystem_);
	new CallDataPushQueryOrder(&service_, cq_.get(), marketSystem_);
	new CallDataPushSendMessage(&service_, cq_.get(), marketSystem_);
	void* tag;
	bool ok;
	// 从完成队列中取出请求处理
	while(true){
		// 当WriteDone时ok为0
		GPR_ASSERT(cq_->Next(&tag, &ok));
		// 基类指针,根据子类类型执行虚函数Proceed()
		CommonCallData* calldata=static_cast<CommonCallData*>(tag);
		calldata->Proceed(ok);
	}
}

int main(int argc, char** argv) {
  ServerImpl server;
  server.Run();
  return 0;
}
