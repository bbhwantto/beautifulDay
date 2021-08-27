#ifndef _REQUESTDATA_H_
#define _REQUESTDATA_H_
#include <string>
#include <unordered_map>
#include <pthread.h>
#include <cstring>
#include <stdio.h>
#include <iostream>

#include "mysql.h"
#include "r_w.h"
#include "epoll.h"
#include "events.h"
#include "threadpool.h"
#include <sys/mman.h>
#include <sys/time.h>

using namespace std;

const int STATE_PARSE_URI = 1;
const int STATE_PARSE_HEADERS = 2;
const int STATE_RECV_BODY = 3;
const int STATE_ANALYSIS = 4;
const int STATE_FINISH = 5;


const int MAX_BUFF = 4096;

// 有请求出现但是读不到数据,可能是Request Aborted,
// 或者来自网络的数据没有达到等原因,
// 对这样的请求尝试超过一定的次数就抛弃
const int AGAIN_MAX_TIMES = 200;

const int PARSE_URI_AGAIN = -1;
const int PARSE_URI_ERROR = -2;
const int PARSE_URI_SUCCESS = 0;

const int PARSE_HEADER_AGAIN = -1;
const int PARSE_HEADER_ERROR = -2;
const int PARSE_HEADER_SUCCESS = 0;

const int ANALYSIS_ERROR = -2;
const int ANALYSIS_SUCCESS = 0;

const int METHOD_POST = 1;
const int METHOD_GET = 2;
const int HTTP_10 = 1;
const int HTTP_11 = 2;

const int EPOLL_WAIT_TIME = 500;





class mytimer;
class requestData;
class MimeType;

class MimeType
{
private:
    static pthread_mutex_t lock;
    static unordered_map<string, string> mime;
    MimeType();
    MimeType(const MimeType &m);
public:
    static string getMime(const string &suffix);
};


enum HTTP_REQUEST
{
    H_ERROR = -1,
    H_NOW,
    H_READ,
    H_GET,
    H_POST,
    H_CONNECT,
    H_REPLY

};

enum HeaderState
{
    h_start = 0,
    h_key,
    h_colon,
    h_spaces_after_colon,
    h_value,
    h_CR,
    h_LF,
    h_end_CR,
    h_end_LF
};



class mytimer
{
public:
    bool deleted;
    size_t expired_time;
    requestData *request_data;

    mytimer(requestData *_request_data, int timeout);
    ~mytimer();
    void update(int timeout);
    bool isvalid();
    void clearReq();
    void setDeleted();
    bool isDeleted() const;
    size_t getExpTime() const;

};

class requestData
{
private:
    string path;
    int againTimes;
    string content;
    int fd;
    int epfd;
    int method;
    int HTTPversion;
    string file_name;
    int now_read_pos;
    int state;
    int h_state;
    bool isfinishl;
    bool keep_alive;
    unordered_map<string, string> headers;

    mytimer *timer;

private:
    int parse_URI();
    int parse_Headers();
    int analysisRequest();

public:
    requestData();
    requestData(int _epollfd, int _fd, string _path);
    ~requestData();
    void addTimer(mytimer *mtimer);
    void reset();
    void seperateTimer();
    int get_fd();
    void setFd(int _fd);
    void handleRequest();
    void handleError(int fd, int err_num, string short_msg);


    
    int hexit(char c);
    void send_respond(int fd, int number, char *disp, const char *type, int len);
    void decode_str(char *to, char *from);
    void send_file(int fd, const char *file);
    const char *get_file_type(const char *name);
    void encode_str(char* to, int tosize, const char* from);
    void send_dir(int fd, const char * file);
};


struct timeCmp
{
    bool operator()(const mytimer *a, const mytimer *b) const;
};








#endif