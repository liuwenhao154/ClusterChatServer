#include "groupmodel.hpp"
#include "db.h"

bool GroupModel::creatGroup(Group& group){
    char sql[1024] = {0};
    sprintf(sql, "insert into allgroup(groupname, groupdesc) values('%s','%s')",
            group.getName().c_str(), group.getDesc().c_str());

    MySQL mysql;
    if(mysql.connect()){
        if(mysql.update(sql)){
            group.setId(mysql_insert_id(mysql.getConnection())); // 获取自增id
            return true;
        }
    }
    return false;
}
//加入群组
void GroupModel::addGroup(int userid,int groupid,string role){
    char sql[1024] = {0};
    sprintf(sql,"insert into groupuser values(%d,%d,'%s')",groupid,userid,role.c_str());
    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}
//查询用户所在群组信息
vector<Group> GroupModel::queryGroups(int userid){
    char sql[1024] = {0};
    // sprintf(sql,"select g.id,g.groupname,g.groupdesc from groupuser u 
    //             inner join allgroup g on  u.groupid = g.id where u.userid = %d",userid)

    sprintf(sql, "select a.id, a.groupname, a.groupdesc \
                    from allgroup a \
                    inner join groupuser b on a.id = b.groupid \
                    where b.userid = %d", userid);
    vector<Group> groupvec;
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                groupvec.emplace_back(group);
            }
            mysql_free_result(res);
        }
    }

    for(Group& group : groupvec){
        sprintf(sql,"select a.id,a.name,a.state,b.grouprole from user a \
                        inner join groupuser b on b.userid = a.id where b.groupid=%d",group.getId());

        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                GroupUser user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                user.setRole(row[3]);
                group.getUsers().emplace_back(user);
            }
            mysql_free_result(res);
        }
    }

    return groupvec;
}
//查询用户所在指定群组的其他成员信息
vector<int> GroupModel::queryGroupUsers(int userid,int groupid){
    char sql[1024] = {0};
    sprintf(sql,"select userid from groupuser where groupid = %d and userid != %d",groupid,userid);
    vector<int> vec;
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                vec.emplace_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
    }
    return vec;
}