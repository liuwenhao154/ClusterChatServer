#ifndef OFFLINEMSGMODEL_H
#define OFFLINEMSGMODEL_H

#include<string>
#include<vector>
using namespace std;

class offlineMsgModel{
    public:
    //存储用户离线消息
        void insert(int userid, string msg);

    //删除用户离线消息
        void remove(int userid);

    //查询用户离线消息
        vector<string> query(int userid);

};

#endif