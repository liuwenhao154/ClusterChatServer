#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"
#include <string>
#include <vector>
using namespace std;

class GroupModel{
    public:
        //创建群组
        bool creatGroup(Group& group);
        //加入群组
        void addGroup(int userid,int groupid,string role);
        //查询用户所在群组信息
        vector<Group> queryGroups(int userid);
        //查询用户所在指定群组的其他成员信息
        vector<int> queryGroupUsers(int userid,int groupid);

};

#endif