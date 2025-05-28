#include<iostream>
#include<string>
#include<vector>
#include<thread>
#include<chrono>
#include<ctime>
#include<semaphore.h>
#include<atomic>

#include "user.hpp"
#include "group.hpp"
#include "njson.hpp"
#include "public.hpp"

#include<unistd.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<netinet/in.h>

using json = nlohmann::json;

//记录当前系统登录的用户信息
User g_currentUser; 

//记录当前登录用户的好友信息
vector<User> g_currentUserFriendList;
//记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;
//显示当前登录成功用户的基本信息
void showCurrentUserData();
//接收线程，主线程阻塞等待输入（发送消息），子线程接收消息
void readTaskHandler(int clientfd);
//获取当前时间
string getCurrentTime();
//主聊天界面
void mainMenu(int clientfd);
//控制主聊天界面
bool isMainMenuRunning = false;

//读写进程间通信
sem_t rwsem;
//记录登录状态
atomic_bool g_isLoginSuccess{false};


void help(int fd = 0,string str = "");
void chat(int fd,string str);
void addfriend(int fd,string str);
void creategroup(int fd,string str);
void addgroup(int fd,string str);
void groupchat(int fd,string str);
void loginout(int fd,string str);

void doLoginResponse(json& responsejs);
void doRegResponse(json& responsejs);

unordered_map<string,string> commandMap = {
    {"help","显示所有支持的命令,格式help"},
    {"chat","一对一聊天,格式chat:friendid:message"},
    {"addfriend","添加好友,格式addfriend:friendid"},
    {"creategroup","创建群组,格式creategroup:groupname:groupdesc"},
    {"addgroup","加入群组,格式addgroup:groupid"},
    {"groupchat","群聊,格式groupchat:groupid:message"},
    {"loginout","注销,格式loginout"}
};

unordered_map<string,function<void(int,string)>> commandHandlerMap = {
    {"help", help},
    {"chat",chat},
    {"addfriend",addfriend},
    {"creategroup",creategroup},
    {"addgroup",addgroup},
    {"groupchat",groupchat},
    {"loginout",loginout}
};

//聊天客户端
int main(int argc,char** argv){
    if(argc < 3){
        cerr << "command invalid! example: ./ChatClient 127.0.0.1 6666 " << endl;
        exit(-1);
    }

    char* ip = argv[1];
    u_int16_t port = atoi(argv[2]);

    int client_fd = socket(AF_INET,SOCK_STREAM,0);
    if(-1 == client_fd){
        cerr << "socket create error" << endl;
        exit(-1);
    }

    sockaddr_in server;
    memset(&server,0,sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    if(-1 == connect(client_fd,(sockaddr*)&server,sizeof(sockaddr_in))){
        cerr << "connect server error" << endl;
        close(client_fd);
        exit(-1);
    }

    sem_init(&rwsem,0,0);

    //登录成功，启动接收线程负责接收数据
    std::thread readTask(readTaskHandler,client_fd);
    readTask.detach();

    //main线程接收用户输入
    for(;;){

        cout << "======================" << endl;
        cout << "1.login" <<endl;
        cout << "2.register" << endl;
        cout << "3.quit" << endl;
        cout << "======================" << endl;
        
        cout << "choice: ";
        int choice = 0;
        cin >> choice;
        if (cin.fail()) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "invalid input!" << endl;
            continue;
        }
        cin.get();

        switch (choice)
        {
        //处理用户登录
        case 1:
            {
                int id = 0;
                string password;

                cout << "please input userid:";
                cin >> id;
                cin.get();
                cout << "please input userpassword: ";
                getline(cin,password);

                json js;
                js["msgid"] = LOGIN_MSG;
                js["id"] = id;
                js["password"] = password;
                string request = js.dump();

                g_isLoginSuccess = false;

                int len = send(client_fd,request.c_str(),request.size(),0);
                if(-1 == len){
                    cerr << "send login msg error" << request << endl;
                }

                sem_wait(&rwsem);

                if(g_isLoginSuccess){
                    //聊天主菜单界面
                    isMainMenuRunning = true;
                    mainMenu(client_fd);
                }

            }
            break;
        //处理用户注册
        case 2:
            {
                string name,password;
                
                cout << "username: ";
                getline(cin,name);
                cout << "userpassword: ";
                getline(cin,password);

                json js;
                js["name"] = name;
                js["password"] = password;
                js["msgid"] = REG_MSG;
                string request =  js.dump();

                int len = send(client_fd,request.c_str(),request.size(),0);
                if(-1 == len){
                    cerr << "send reg msg error : " << request << endl;
                }

                sem_wait(&rwsem);
            }
            break;
        //处理用户退出
        case 3:
            close(client_fd);
            sem_destroy(&rwsem);
            exit(0);
        default:
            cout << "invalid input!" << endl;
            break;
        }


    }

}


void doLoginResponse(json& responsejs){

    if(0 != responsejs["error"].get<int>()){  //登录失败
        cerr << responsejs["errmsg"] << endl;
        g_isLoginSuccess = false;
    }
    else{
                //记录当前用户信息
        g_currentUser.setId(responsejs["id"].get<int>());
        g_currentUser.setName(responsejs["name"]);

        //记录当前登录用户的好友信息
        if(responsejs.contains("friends")){
            g_currentUserFriendList.clear();

            for(const string& userStr : responsejs["friends"]){
                json userjs = json::parse(userStr);
                User user;
                user.setId(userjs["id"].get<int>());
                user.setName(userjs["name"]);
                user.setState(userjs["state"]);
                g_currentUserFriendList.emplace_back(user);
            }
        }
        //记录当前登录用户的群组列表信息
        if(responsejs.contains("groups")){
            g_currentUserGroupList.clear();

            for(const string& groupjsonStr : responsejs["groups"]){
                json groupjs = json::parse(groupjsonStr);
                Group group;
                group.setId(groupjs["id"].get<int>());
                group.setName(groupjs["groupname"]);
                group.setDesc(groupjs["groupdesc"]);

                for(const string& userjsonStr : groupjs["users"]){
                    json guserjs = json::parse(userjsonStr);
                    GroupUser guser;
                    guser.setId(guserjs["id"].get<int>());
                    guser.setName(guserjs["name"]);
                    guser.setState(guserjs["state"]);
                    guser.setRole(guserjs["role"]);
                    group.getUsers().emplace_back(guser);
                }
                g_currentUserGroupList.emplace_back(group);
            }
        }

        showCurrentUserData();
        
        //显示用户离线消息
        if(responsejs.contains("offlinemsg")){
            for(const string& msgStr : responsejs["offlinemsg"]){
                json js = json::parse(msgStr);
                if(ONE_CHAT_MSG == js["msgid"].get<int>()){
                    cout << js["time"] << " [" << js["id"] << "] " << js["name"]
                        << " said: " << js["msg"] << endl; 
                }
                else{
                    cout << "Group Message | " << js["time"] << " [" << js["groupid"] << "] "
                        << js["id"] << " (" << js["name"] << ") said: " << js["msg"] << endl;
                }
            }
        }
        g_isLoginSuccess = true;
    }
    
}

void doRegResponse(json& responsejs)
{
    if(0 != responsejs["error"].get<int>()){
        cerr << responsejs["errmsg"] << endl;
    }
    else {
        cout << "register success ,userid is " << responsejs["id"]
            << " , do not forget it!" << endl;
    }
}

void showCurrentUserData(){
    cout << "=====================login user==================" << endl;
    cout << "current login user => id: " << g_currentUser.getId() << " name: " << g_currentUser.getName() << endl;
    cout << "---------------------friend list-----------------" << endl;
    if(!g_currentUserFriendList.empty()){
        for(User& user:g_currentUserFriendList){
            cout << "id : " <<  user.getId() << " name: " << user.getName() << " state: " << user.getState() << endl;
        }
    }
    cout << "---------------------group list-------------------" << endl;
    if(!g_currentUserGroupList.empty()){
        for(Group& group : g_currentUserGroupList){
            cout << "groupid : " << group.getId() << " groupname: " << group.getName() << " groupdesc: " << group.getDesc() << endl;
            vector<GroupUser> guservec = group.getUsers();
            if(!guservec.empty()){
                cout << "--------------------------------------" << endl;
                for(GroupUser& guser : guservec){
                    cout << "userid : " << guser.getId() << " username: " << guser.getName()
                        << " userstate: " << guser.getState() << " userrole:" << guser.getRole() << endl;
                }
                cout << "--------------------------------------" << endl;
            }
        }
    }
    cout << "==================================================" << endl;
}

void mainMenu(int clientfd){
    help();

    char buffer[1024] = {0};
    while(isMainMenuRunning){
        cin.getline(buffer,1024);
        string commandbuf(buffer);
        string command;
        int idx = commandbuf.find(":");
        if(-1 == idx){
            command = commandbuf;
        }
        else{
            command = commandbuf.substr(0,idx);
        }
        auto it = commandHandlerMap.find(command);
        if(it == commandHandlerMap.end()){
            cerr << "invalid input command!" << endl;
            continue;
        }

        it->second(clientfd,commandbuf.substr(idx + 1,commandbuf.size() - idx - 1));
    }
}

void help(int fd,string str){
    cout << "show command list >>>" << endl;
    for(auto& p: commandMap){
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}

void addfriend(int fd,string str){
    int friendid = stoi(str);
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(fd,buffer.c_str(),buffer.length(),0);
    if(-1 == len){
        cerr << "send addfriend msg error -> " << buffer << endl;
    }
}

void chat(int fd,string str){
    int idx = str.find(":");
    if(-1 == idx){
        cerr << "chat command invalid1" << endl;
        return;
    }

    int friendid = stoi(str.substr(0,idx));
    string message = str.substr(idx + 1,str.size() - idx - 1);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["time"] = getCurrentTime();
    js["msg"] = message;
    string buffer = js.dump();

    int len = send(fd,buffer.c_str(),buffer.length(),0);
    if(-1 == len){
        cerr << "send chat msg error -> " << buffer << endl;
    }
}

void creategroup(int fd,string str){
    int idx = str.find(":");
    if(-1 == idx){
        cerr << "creategroup command invalid1" << endl;
        return;
    }
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = str.substr(0,idx);
    js["groupdesc"] = str.substr(idx+1,str.size() - idx - 1);
    string buffer = js.dump();

    int len = send(fd,buffer.c_str(),buffer.length(),0);
    if(-1 == len){
        cerr << "send creatgroup msg error -> " << buffer << endl;
    }
}

void addgroup(int fd,string str){
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = stoi(str);
    string buffer = js.dump();

    int len = send(fd,buffer.c_str(),buffer.length(),0);
    if(-1 == len){
        cerr << "send addgroup msg error -> " << buffer << endl;
    }

}

void groupchat(int fd,string str){
    int idx = str.find(":");
    if(-1 == idx){
        cerr << "groupchat command invalid!" << endl;
        return;
    }
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = stoi(str.substr(0,idx));
    js["msg"] = str.substr(idx + 1,str.size() - idx - 1);
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(fd,buffer.c_str(),buffer.size(),0);
    if(-1 == len){
        cerr << "send groupchat msg error -> " << buffer << endl;
    }
}

void loginout(int fd,string str){
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(fd,buffer.c_str(),buffer.size(),0);
    if(-1 == len){
        cerr << "send loginout msg error -> " << buffer << endl;
    }
    else{
        isMainMenuRunning = false;
    }
}

void readTaskHandler(int clientfd){
    for(;;){
        char buffer[1024] = {0};
        int len = recv(clientfd,buffer,1024,0);
        if(-1 == len || 0 == len){
            close(clientfd);
            exit(-1);
        }

        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if(ONE_CHAT_MSG == msgtype){
            cout << js["time"] << " [" << js["id"] << "] " << js["name"] << " said: " << js["msg"] << endl;
            continue;
        }
        if (GROUP_CHAT_MSG == msgtype) {
            cout << "Group Message | " << js["time"] << " [" << js["groupid"] << "] "
                << js["id"] << " (" << js["name"] << ") said: " << js["msg"] << endl;
            continue;
        }
        if (LOGIN_MSG_ACK == msgtype) {
            doLoginResponse(js);
            sem_post(&rwsem);
            continue;
        }
        if(REG_MSG_ACK == msgtype){
            doRegResponse(js);
            sem_post(&rwsem);
            continue;
        }
    }
}

//获取当前时间
string getCurrentTime() {
    auto now = chrono::system_clock::now();              // 当前时间点
    time_t now_time_t = chrono::system_clock::to_time_t(now); // 转为time_t
    tm* now_tm = localtime(&now_time_t);                 // 转为本地时间

    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M", now_tm);
    return string(buffer);
}
