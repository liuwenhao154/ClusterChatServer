#include "chatservice.hpp"
#include <muduo/base/Logging.h>
#include "public.hpp"



ChatService::ChatService() {
    _msgHandlerMap.insert({LOGIN_MSG, bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG,bind(&ChatService::oneChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG,bind(&ChatService::addFriend,this,_1,_2,_3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG,bind(&ChatService::createGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG,bind(&ChatService::addGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG,bind(&ChatService::groupChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGINOUT_MSG,bind(&ChatService::loginout,this,_1,_2,_3)});

    if(_redis.connect()){
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage,this,_1,_2));
    }
}

ChatService* ChatService::instance() {
    static ChatService service;
    return &service;
}

void ChatService::login(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    int id = js["id"].get<int>();
    string password = js["password"];

    User user = _userModel.query(id);
    if(user.getId() == id && user.getPassword() == password){
        if(user.getState() == "online"){
            //用户已登录，登录失败
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["error"] = 2;
            response["errmsg"] = "this account is using, input another";
            conn->send(response.dump());
        }
        else{
            //记录用户登录信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id,conn});
            }

            //登录成功，向redis订阅channel(id)
            _redis.subscribe(id);

            //登录成功
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["error"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            vector<string> msgvec = _offlineMsgModel.query(id);
            if(!msgvec.empty()){
                //读取离线消息推送后删除
                response["offlinemsg"] = msgvec;
                _offlineMsgModel.remove(id);
            }

            //查询好友信息并返回
            vector<User> uservec = _friendModel.query(id);
            if(!uservec.empty()){
                vector<string> vec;
                for(User &user : uservec){
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec.emplace_back(js.dump());
                }
                response["friends"] = vec;
            }

            vector<Group> groupuservec = _groupModel.queryGroups(id);
            if(!groupuservec.empty()){
                vector<string> groupV;
                for(Group& group : groupuservec){
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for(GroupUser& user : group.getUsers()){
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.emplace_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.emplace_back(grpjson.dump());
                }
                response["groups"] = groupV;
            }
            conn->send(response.dump());
        }

    }else{
        //登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["error"] = 1;
        response["errmsg"] = "id or password is invalid!";
        conn->send(response.dump());
    }

}

void ChatService::reg(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    string name = js["name"];
    string password  = js["password"];

    User user;
    user.setName(name);
    user.setPassword(password);

    bool state = _userModel.insert(user);
    if(state){
        //注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["error"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());

    }else{
        //注册失败
        json response;
        char msg[1024];
        sprintf(msg," '%s' is already exist, register fail!",name.c_str());
        response["msgid"] = REG_MSG_ACK;
        response["error"] = 1;
        response["errmsg"] = msg;
        conn->send(response.dump());
    }
}

MsgHandler ChatService::getHandler(int msgid) {
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end()) {
        return [=](const TcpConnectionPtr& conn, json& js, Timestamp time) {
            LOG_ERROR << "msgid:" << msgid << " can not find Handler!";
        };
    }
    return it->second;
}

//注销业务
void ChatService::loginout(const TcpConnectionPtr& conn,json& js,Timestamp time){
    int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if(it != _userConnMap.end()){
            _userConnMap.erase(it);
        }
    }

    //用户下线，取消redis订阅
    _redis.unsubscribe(userid);

    User user(userid,"","","offline");
    _userModel.updateState(user);
}

//用户异常退出
void ChatService::clientCloseException(const TcpConnectionPtr& conn){
    
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for(auto it = _userConnMap.begin();it != _userConnMap.end();++it){
            if(it->second == conn){
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    _redis.unsubscribe(user.getId());

    if(user.getId() != -1){
        user.setState("offline");
        _userModel.updateState(user);
    }
}

void ChatService::oneChat(const TcpConnectionPtr& conn,json& js,Timestamp time){
    int toid = js["toid"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end()){
            //对方在同一服务器且在线，消息转发
            it->second->send(js.dump());
            return;
        }
    }

    //查询是否在其他服务器登录
    User user = _userModel.query(toid);
    if(user.getState() == "online"){
        _redis.publish(toid,js.dump());
        return;
    }

    //对方不在线，存储离线消息
    _offlineMsgModel.insert(toid,js.dump());
}

void ChatService::reset(){
    _userModel.resetState();
}

void ChatService::addFriend(const TcpConnectionPtr& conn,json& js,Timestamp time){
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    _friendModel.insert(userid,friendid);
}

//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr& conn,json& js,Timestamp time){
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    Group group(-1,name,desc);
    if(_groupModel.creatGroup(group)){
        _groupModel.addGroup(userid,group.getId(),"creator");
    }
}
//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr& conn,json& js,Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid,groupid,"normal");    
}
//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr& conn,json& js,Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> uservec = _groupModel.queryGroupUsers(userid,groupid);

    lock_guard<mutex> lock(_connMutex);
    for(int id : uservec){    
        auto it = _userConnMap.find(id);
        if(it != _userConnMap.end()){
            it->second->send(js.dump());
        }
        else{
            //查询是否在其他服务器登录
            User user = _userModel.query(id);
            if(user.getState() == "online"){
                _redis.publish(id,js.dump());
            }else{
                //存储离线消息
                _offlineMsgModel.insert(id,js.dump());
            }
        }
    }
}

void ChatService::handleRedisSubscribeMessage(int userid,string msg){

    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it != _userConnMap.end()){
        it->second->send(msg);
        return;
    }

    _offlineMsgModel.insert(userid,msg);
}
