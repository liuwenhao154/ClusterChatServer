#include "chatserver.hpp"
#include "njson.hpp"
#include "chatservice.hpp"
#include<functional>
#include<string>

using json = nlohmann::json;


ChatServer::ChatServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg):_server(loop,listenAddr,nameArg),_loop(loop){
    //注册连接回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection,this,std::placeholders::_1));
    _server.setMessageCallback(std::bind(&ChatServer::onMessage,this,std::placeholders::_1,
                                    std::placeholders::_2,std::placeholders::_3));
    _server.setThreadNum(10);
}

void ChatServer::start(){
    _server.start();
}

void ChatServer::onConnection(const TcpConnectionPtr& conn){
    //客户端断开连接
    if(!conn->connected()){
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}
void ChatServer::onMessage(const TcpConnectionPtr& conn,
                Buffer* buffer,
                Timestamp time){
    string buf = buffer->retrieveAllAsString();

    json js = json::parse(buf);
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    msgHandler(conn,js,time);
}

