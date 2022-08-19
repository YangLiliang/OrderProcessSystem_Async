#ifndef GENERATE_REQUESTS_CC
#define GENERATE_REQUESTS_CC
#include "generate_requests.h"
int main(){
    srand((unsigned)time(NULL));
    uint64_t clientID, requestNum;
    std::string fileName;
    std::cout<<"请输入用户ID以及输出文本文件名!"<<std::endl;
    std::cin>>clientID>>fileName;
    std::cout<<"请输入生成的条数"<<std::endl;
    std::cin>>requestNum;
    std::ofstream out(fileName);
    if(out.is_open()){
        out<<requestNum<<std::endl;
        for(int i=0;i<requestNum;i++){
            out<<"LIMIT"<<" ";
            int type=rand();
            if(type&1) out<<"SELL"<<" ";
            else out<<"BUY"<<" ";
            out<<clientID<<" ";
            std::string stockID="600030";
            // for(int j=0;j<6;j++){
            //     int rand_num=rand()%10;
            //     stockID.push_back('0'+rand_num);
            // }
            out<<stockID<<" ";
            uint64_t num=rand()%10000+1;
            out<<num<<" ";
            double price=static_cast<double>(rand())/(static_cast<double>(RAND_MAX/100))+1;
            out<<price<<std::endl;
        }
    }
    return 0;
}
#endif