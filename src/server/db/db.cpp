#include "db.h"
#include<muduo/base/Logging.h>

static string server = "127.0.0.1";
static string user = "chatuser";
static string password = "123456";
static string dbname = "chat";

MySQL::MySQL(){
    _conn = mysql_init(nullptr);
}

MySQL::~MySQL(){
    if(_conn != nullptr)
        mysql_close(_conn);
}

bool MySQL::connect(){
    MYSQL *p = mysql_real_connect(_conn,server.c_str(),user.c_str(),
            password.c_str(),dbname.c_str(),3306,nullptr,0);
    if(p != nullptr){
        mysql_query(_conn,"set names gbk");
        LOG_INFO << "connect mysql success!";
    }
    else LOG_INFO << "connect mysql fail!" << mysql_error(_conn);
    return p;
}

bool MySQL::update(string sql){
    if(mysql_query(_conn,sql.c_str())){
        LOG_INFO << __FILE__ << " : " << __LINE__ << " : " << sql << "更新失败" << mysql_error(_conn);
        return false;
    }
    return true;
}

MYSQL_RES* MySQL::query(string sql){
    if(mysql_query(_conn,sql.c_str())){
        LOG_INFO << __FILE__ << " : " << __LINE__ << " : " << sql << "查询失败" << mysql_error(_conn);
        return nullptr;
    }
    return mysql_use_result(_conn);
}

MYSQL* MySQL::getConnection(){
    return this->_conn;
}