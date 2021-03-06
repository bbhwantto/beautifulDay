#include "mysql.h"

connection_pool::connection_pool()
{
    m_CurConn = 0;
    m_FreeConn = 0;
}


connection_pool * connection_pool::GetInstance()
{
    static connection_pool connPool;
    return &connPool;
}

//构造初始化
void connection_pool::init(std::string url, std::string User, std::string PassWord, std::string DataBaseName, int Port, int MaxConn, int close_log)
{
    pthread_mutex_init(&lock, NULL);

    m_url = url;
    m_Port = Port;
    m_User = User;
    m_PassWord = PassWord;
    m_DatabaseName = DataBaseName;
    m_close_log = close_log;

    for(int i =0; i < MaxConn; i++){
        MYSQL *con = NULL;
        con = mysql_init(con);

        if(con == NULL){
            perror("MySQL Error1");
            exit(1);
        }
        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DataBaseName.c_str(), Port, NULL, 0);

        if(con == NULL){
            perror("MySQL Error2");
            exit(1);
        }
        connList.push_back(con);
        ++m_FreeConn;
    }

    reserve = sem(m_FreeConn);

    m_MaxConn = m_FreeConn;
}

//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection()
{
    MYSQL *con = NULL;
    if(connList.size() == 0)
        return NULL;
    
    reserve.wait();

    pthread_mutex_lock(&lock);

    con = connList.front();
    connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;

    pthread_mutex_unlock(&lock);
    return con;
}

//释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con)
{
    if(NULL == con)
        return false;
    
    pthread_mutex_lock(&lock);

    connList.push_back(con);
    ++m_FreeConn;
    --m_CurConn;

    pthread_mutex_unlock(&lock);

    reserve.post();
    return true;
}


//销毁数据库连接池
void connection_pool::DestroyPool()
{
    pthread_mutex_lock(&lock);
    if(connList.size() > 0){
        for(auto it = connList.begin(); it != connList.end(); ++it){
            MYSQL *con =*it;
            mysql_close(con);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }

    pthread_mutex_unlock(&lock);
}


//当前空闲的连接数
int connection_pool::GetFreeConn()
{
    return this->m_FreeConn;
}

connection_pool::~connection_pool()
{
    DestroyPool();
}

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool)
{
    *SQL = connPool->GetConnection();
    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}

