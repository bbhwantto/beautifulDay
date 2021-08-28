#include "epoll.h"


extern priority_queue<mytimer*, vector<mytimer*>, timeCmp>myTimerQueue;


void epoll_run(int port)
{
    
    string PATH = "/";

    int i = 0;

    int lfd = init_socket(port);
    if(lfd < 0){
        perror("socket bind failed");
        exit(1);
    }

    if(setSocketNonBlocking(lfd) < 0){
        perror("set socket non block failed");
        exit(1);
    }

    int epfd = epoll_init();
    if(epfd < 0){
        perror("epoll init failed");
        exit(1);
    }





    /*创建线程池，池里最小4个线程，最大100，队列最大65535*/
    threadpool_t *tp = threadpool_create(4, 100, 65535);
    
    struct epoll_event* events;
	events = new epoll_event[MAXEVENTS];


    //设置成ET模式
    __uint32_t event = EPOLLIN | EPOLLET;

    requestData *req = new requestData();
    req->setFd(lfd);
    epoll_add(epfd, lfd, static_cast<void*>(req), event);
    
    
    //循环监听
    while(1){
        int evets_num = epoll_wait(epfd, events, MAXEVENTS, -1);
        //printf("neweqasdas\n");
        if(evets_num < 0){
            perror("epoll wait error");
            break;
        }

        if(evets_num == 0)
        {
           
            continue;
        }

         
        for(int i = 0; i < evets_num; ++i){
            // 获取有事件产生的描述符
            requestData* request = (requestData*)(events[i].data.ptr);
            
            int fd = request->get_fd();
            //printf("fd=%d\n",fd);
            if(fd == lfd){
                acceptConnection(lfd, epfd, PATH);
            }
            else{
                // 排除错误事件
                
				if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)
					|| (!(events[i].events & EPOLLIN)))
				{
					printf("error event\n");
					disconnect(fd,epfd);
					continue;
				}


                // 将请求任务加入到线程池中
				// 加入线程池之前将Timer和request分离
                request->seperateTimer();    //request , timer  分离
                
                int ret = threadpool_add(tp, myHandler, events[i].data.ptr);
            }
        }
        handle_expired_event();
         
    }
}

int init_socket(int port)
{
    int lfd;
    if ((lfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ){
        ERR_EXIT("socket");
    }

    int one = 1;
    // 设置端口复用。
    if(setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        close(lfd);
        return -1;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    servaddr.sin_family = AF_INET;

    if(bind(lfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
        ERR_EXIT("bind");
    }

    if(listen(lfd, SOMAXCONN) < 0){
        ERR_EXIT("listen");
    }

    return lfd;
}



int setSocketNonBlocking(int lfd)
{
    int flag = fcntl(lfd, F_GETFL, 0);
    if(flag == -1)
        return -1;
    flag |= O_NONBLOCK;
    if(fcntl(lfd, F_SETFL, flag) == -1)
        return -1;
    
    return 0;

}



int epoll_init()
{
    int epfd = epoll_create(LISTENQ + 1);
    if(epfd == -1)
        return -1;

    // events = (struct epoll_event*)malloc(sizeof(struct epoll_event) * MAXEVENTS);
    //events = new epoll_event[MAXEVENTS];
    return epfd;
}



int epoll_add(int epoll_fd, int fd, void *request, __uint32_t events)
{
    struct epoll_event ev;
    ev.data.ptr = request;
    ev.events = events;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0){
        perror("epoll_add error");
        return -1;
    }
    
    return 0;
}


int epoll_mod(int epoll_fd, int fd, void *request, __uint32_t events)
{
    cout<<epoll_fd<<endl;
    cout<<fd<<endl;
    struct epoll_event event ;
    event.data.ptr = request;
    event.events = events;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0){
        //printf("asfsdf\n");
        perror("epoll_mod error");
        return -1;
    }
    return 0;
}


int epoll_del(int epoll_fd, int fd, void *request, __uint32_t events)
{
    struct epoll_event event;
    event.events = events;
    event.data.ptr = request;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event) < 0)
    {
        perror("epoll_del error");
        return -1;
    } 
    return 0;
}


// 返回活跃事件数
int my_epoll_wait(int epoll_fd, struct epoll_event* events, int max_events, int timeout)
{
    int ret_count = epoll_wait(epoll_fd, events, max_events, timeout);
    if (ret_count < 0)
    {
        perror("epoll wait error");
    }
    return ret_count;
}


void acceptConnection(int listen_fd, int epoll_fd,string path)
{
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    socklen_t client_addr_len = 0;
    int accept_fd = 0;
    while((accept_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_addr_len)) > 0){
       
        int ret = setSocketNonBlocking(accept_fd);
        
        if(ret < 0){
            perror("Set non block failed!");
            return ;
        }


        requestData *req_info = new requestData(epoll_fd, accept_fd, path);

        // 文件描述符可以读，边缘触发(Edge Triggered)模式，保证一个socket连接在任一时刻只被一个线程处理
        __uint32_t _epo_event = EPOLLIN | EPOLLET | EPOLLONESHOT;

        epoll_add(epoll_fd, accept_fd, static_cast<void*>(req_info), _epo_event);





        // 新增时间信息

        mytimer *mtimer = new mytimer(req_info, TIMER_TIME_OUT);
        //mytimer *mtimer = new mytimer(req_info, TIMER_TIME_OUT);
        req_info->addTimer(mtimer);

        pthread_mutex_lock(&qlock);
        myTimerQueue.push(mtimer);
        pthread_mutex_unlock(&qlock);


    }

}



void myHandler(void *args)
{
    requestData *req_data = (requestData*)args;
    req_data->handleRequest();
}





void handle_expired_event()
{
    //cout<<"duiliesize = "<<myTimerQueue.size()<<endl;
    //printf("nice\n");
    pthread_mutex_lock(&qlock);
    while(!myTimerQueue.empty()){
        
        mytimer *ptimer_now = myTimerQueue.top();
        //cout<<ptimer_now->isDeleted()<<endl;
        if(ptimer_now->isDeleted()){
            int k =1;
            cout<<k<<endl;
            myTimerQueue.pop();
            delete ptimer_now;
        }
        else if (ptimer_now->isvalid() == false){
            int k =2;
            cout<<k<<endl;
            myTimerQueue.pop();
            delete ptimer_now;
        }else{
            break;
        }
    }
    pthread_mutex_unlock(&qlock);
}




void disconnect(int cfd, int epfd)
{
    cout<<"cfd == " << cfd << "!!!!!!!!!!!!" <<endl;
    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);

    if(ret == -1){
        perror("epoll_ctl del cfd error");
        exit(1);
    }

    close(cfd);

}