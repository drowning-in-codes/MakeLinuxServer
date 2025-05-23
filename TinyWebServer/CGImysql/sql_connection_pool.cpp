#include "sql_connection_pool.h"
#include <cstdlib>
#include <iostream>
#include <list>
#include <mysql/mysql.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

connection_pool::connection_pool() {
  m_CurConn = 0;
  m_FreeConn = 0;
}

// 获取连接池实例
connection_pool *connection_pool::GetInstance() {
  static connection_pool connPool;
  return &connPool;
}

//构造初始化
void connection_pool::init(std::string url, std::string User,
                           std::string PassWord, std::string DBName, int Port,
                           int MaxConn, int close_log) {
  m_url = url;
  m_Port = Port;
  m_User = User;
  m_PassWord = PassWord;
  m_DatabaseName = DBName;
  m_close_log = close_log;

  for (int i = 0; i < MaxConn; i++) {
    MYSQL *con = NULL;
    con = mysql_init(con);
    if (con == NULL) {
      LOG_ERROR("MYSQL Error");
      exit(1);
    }
    con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(),
                             DBName.c_str(), Port, NULL, 0);
    if (con == NULL) {
      LOG_ERROR("MySQL Error");
      exit(1);
    }
    connList.push_back(con);
    ++m_FreeConn;
  }
  reserve = sem(m_FreeConn);
  m_MaxConn = m_FreeConn;
}

//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection() {
  MYSQL *con = NULL;
  if (0 == connList.size()) {
    return NULL;
  }
  //  如果为0阻塞 否则信号量-1,继续运行
  // 连接池可用连接-1,信号量-1
  reserve.wait();
  lock.lock();
  con = connList.front();
  connList.pop_front();

  --m_FreeConn;
  ++m_CurConn;
  lock.unlock();
  return con;
}

//释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con) {
  if (NULL == con) {
    return false;
  }
  lock.lock();
  connList.push_back(con);
  ++m_FreeConn;
  --m_CurConn;
  lock.unlock();
  // 连接池可用连接+1,信号量+1
  reserve.post();
  return true;
}

//销毁数据库连接池
void connection_pool::DestroyPoll() {
  lock.lock();
  if (connList.size() > 0) {
    std::list<MYSQL *>::iterator it;
    for (it = connList.begin(); it != connList.end(); ++it) {
      MYSQL *con = *it;
      mysql_close(con);
    }
    m_CurConn = 0;
    m_FreeConn = 0;
    connList.clear();
  }
  lock.unlock();
}

// 获取空闲连接
int connection_pool::GetFreeConn() { return this->m_FreeConn; }

connection_pool::~connection_pool() { DestroyPoll(); }

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool) {
  *SQL = connPool->GetConnection();
  conRAII = *SQL;
  poolRAII = connPool;
}
connectionRAII::~connectionRAII() { poolRAII->ReleaseConnection(conRAII); }