#include "offlinemsgmodel.hpp"
#include "db.h"


//存储用户离线消息
void offlineMsgModel::insert(int userid, string msg){
    char sql[1024];
    sprintf(sql,"insert into offlinemessage values(%d,'%s')",userid,msg.c_str());

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
    
}

//删除用户离线消息
void offlineMsgModel::remove(int userid){
    char sql[1024];
    sprintf(sql,"delete from offlinemessage where userid = %d",userid);

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}

//查询用户离线消息
vector<string> offlineMsgModel::query(int userid){
    char sql[1024];
    sprintf(sql,"select message from offlinemessage where userid = %d",userid);

    vector<string> vec;
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
        }
    }
    return vec;
}
