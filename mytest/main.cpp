#include "threadpool.h"
#include "r_w.h"
#include "epoll.h"
#include "requestData.h"
#include "events.h"
#include "mysql.h"


#include <queue>
#include <sys/time.h>
#include <stdio.h>
#include <vector>
#include <iostream>

#include <string>
#include <sys/epoll.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdlib>

using namespace std;

extern pthread_mutex_t qlock;
extern priority_queue<mytimer*, vector<mytimer*>, timeCmp>myTimerQueue;


connection_pool * m_connPool;



int main(int argc, char *argv[])
{
    //屏蔽信号阻塞
    handle_for_sigpie();

    string PATH = "/";
    if(argc < 3)
    {
        cout<<"eg: ./a.out port path\n";
        exit(1);
    }

    // 采用指定的端口
    int port = atoi(argv[1]);

    // 修改进程工作目录, 方便后续操作
    int ret = chdir (argv[2]);

    if(ret == -1){
        perror("chdir error");
        exit(1);
    }


    //数据库登录名,密码,库名
    string users ="root";
    string passwd = "123456";
    string databasename = "yourdb";



    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost", users, passwd, databasename, 3306, 10, 1);



    epoll_run(port);
    return 0;
}