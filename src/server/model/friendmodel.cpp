#include "friendmodel.hpp"
#include "db.h"

//添加好友
void FriendModel::insert(int userid,int friendid){
    char sql[1024];
    sprintf(sql,"insert into friend values(%d,%d)",userid,friendid);

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}
//返回好友列表
vector<User> FriendModel::query(int userid){
    char sql[1024];
    //sprintf(sql,"select u.id,u.name,u.state from friend f 
    //            left join user on f.friendid = u.id where f.userid = %d",userid);
    sprintf(sql,"select a.id,a.name,a.state from user a inner join friend b on b.friendid = a.id where b.userid=%d",userid);

    vector<User> vec;
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES *res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.emplace_back(user);
            }
            mysql_free_result(res);
        }
    }
    return vec;
}