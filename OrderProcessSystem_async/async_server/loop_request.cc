#include "loop_request.h"
// 发送消息类
PushSendMessageCall::PushSendMessageCall(const SendMessageRequest& request, CompletionQueue& cq_, std::unique_ptr<OrderService::Stub>& stub_):
	AbstractLoopCall(), reportsCounter(0){
		responder = stub_->AsyncPushSendMessage(&context, request, &cq_, (void*)this);
		callStatus = PROCESS ;
}

void PushSendMessageCall::Proceed(bool ok){
	if(callStatus == PROCESS){
		if(!ok){
			responder->Finish(&status, (void*)this);
			callStatus = FINISH;
			return ;
		}
		responder->Read(&executionReport_, (void*)this);
	}
	else if(callStatus == FINISH){
		delete this;
	}
}

// 循环申请发送交易信息
LoopRequest::LoopRequest(std::shared_ptr<Channel> channel):
		stub_(OrderService::NewStub(channel)){}

// 查询订单
void LoopRequest::PushSendMessage(){
	SendMessageRequest request=MakeSendMessageRequest();
	// 注册查询订单请求
	new PushSendMessageCall(request, cq_, stub_);
}

// 异步处理完成队列中的事件
void LoopRequest::AsyncCompleteRpc(){
	void* got_tag;
	bool ok=false;
	// 从完成队列中取出请求处理
	while(cq_.Next(&got_tag, &ok)){
		// 基类指针,根据子类执行的虚函数Proceed()
		AbstractLoopCall* call=static_cast<AbstractLoopCall*>(got_tag);
		call->Proceed(ok);
	}
}