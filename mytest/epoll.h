#ifndef _EPOLL_H
#define _EPOLL_H
#include "requestData.h"
#include "threadpool.h"
#include "r_w.h"
#include "mysql.h"

#include <vector>
#include <queue>
#include <string>
#include <sys/epoll.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/wait.h>
#include <poll.h>
#include <sys/stat.h>

using namespace std;

#define ERR_EXIT(m) \
     do \
	 {\
		perror(m);\
		exit(EXIT_FAILURE);\
	 }while(0)



const int MAXEVENTS = 5000;
const int LISTENQ = 1024;
extern pthread_mutex_t qlock;


const int TIMER_TIME_OUT = 500;


int epoll_init();
int epoll_add(int epoll_fd, int fd, void *request, __uint32_t events);
int epoll_mod(int epoll_fd, int fd, void *request, __uint32_t events);
int epoll_del(int epoll_fd, int fd, void *request, __uint32_t events);
int my_epoll_wait(int epoll_fd, struct epoll_event *events, int max_events, int timeout);

void epoll_run(int port);
int init_socket(int port);
int setSocketNonBlocking(int fd);
void acceptConnection(int listen_fd, int epoll_fd,string path);
void myHandler(void *args);
void handle_expired_event();
void disconnect(int cfd, int epfd);



#endif
