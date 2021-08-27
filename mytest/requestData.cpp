#include "requestData.h"



pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;

extern connection_pool* m_connPool;

priority_queue<mytimer*, vector<mytimer*>, timeCmp> myTimerQueue;

pthread_mutex_t MimeType::lock = PTHREAD_MUTEX_INITIALIZER;


requestData::requestData():
    now_read_pos(0), state(STATE_PARSE_URI),h_state(h_start),
    keep_alive(false), againTimes(0), timer(NULL)
{
    cout<<"requestData constructed ! " << endl;
}

requestData::requestData(int _epollfd, int _fd, string _path):
    now_read_pos(0), state(STATE_PARSE_URI), h_state(h_start),
    keep_alive(false), againTimes(0), timer(NULL),
    path(_path), fd(_fd), epfd(_epollfd)
{}

requestData::~requestData()
{
    cout<<"~requestData()" << endl;
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.ptr = (void *)this;
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev);
    if(timer != NULL){
        timer->clearReq();
        timer = NULL;
    }
    close(fd);
}

void requestData::addTimer(mytimer *mtimer)
{
    if(timer == NULL)
        timer = mtimer;
}

void requestData::seperateTimer()
{
    if(timer){
        timer->clearReq();
        timer = NULL;
    }
}

void requestData::reset() 
{
    againTimes = 0;
    content.clear();
    file_name.clear();
    now_read_pos = 0;
    state = STATE_PARSE_URI;
    h_state = h_start;
    headers.clear();
    keep_alive = false;
}

int requestData::get_fd()
{
    return fd;
}

void requestData::setFd(int _fd)
{
    fd = _fd;
}

void requestData::handleRequest()
{
	do{
		int i;
		char line[4096] = {0};
		char path[2048];
		int tem = 0;
		char tchar[1024];
		int len;
		if(state == H_READ){
			len = readn(fd, line, 4096);
			cout<<line<<endl;
			if(len < 0){
				send_error(fd, 404, "Not Found", "NO such file or direntry");
				state = H_ERROR;
				break;
			}
			else if (len == 0){
				state == H_ERROR;
				break;
			}

			for(i = tem; i < len; ++i){
				if(line[i] == '\r' && line[i + 1] == '\n'){
					tchar[i - tem] = line[i];
					tchar[i - tem + 1] = line[i + 1];
					break;
				}
				else	
					tchar[i - tem] = line[i];
			}
			if(i >= len){
				state = H_ERROR;
				break;
			}
			tem = i + 2;

			if(strncasecmp(tchar, "GET", 3) == 0)
				state = H_GET;
			else if (strncasecmp(tchar, "POST", 4) == 0)
				state = H_POST;
			else{
				state = H_ERROR;
				break;
			}

		}



		if(state == H_GET){
			int j;
			int judge = 0;
			for(j = 4;;++j){
				if(line[j] == '?') judge = 1;
				if(line[j] == ' ') break;
				path[j - 4] = line[j];
			}
			path[j - 4] = '\0';

			if(judge == 1)
				state = H_CONNECT;
			else 
				state = H_REPLY;
		}


		if(state == H_POST){
			cout << "it is post !!!!!" << endl;
			while(tem < len){
				if(line[tem] == '\r')
					break;
				
				for(i = tem; i < len; i++){

					if (line[i] == '\r' && line[i + 1] == '\n'){
						tchar[i - tem] = line[i];
						tchar[i - tem + 1] = line[i + 1];
						break;
					}
					else{
						tchar[i - tem] = line[i];
					}
				}

				if(i >= len){
					state = H_ERROR;
					break;
				}

				tchar[i - tem + 2] = '\0';
				tem = i + 2;

				char first[128];
				char second[128];

				for(i = 0;; i++){
					if(tchar[i] == '\0'){
						state = H_ERROR;
						break;
					}
					if(tchar[i] == ':'){
						break;
					}
					first[i] = tchar[i];
				}

				first[i] = '\0';
				i += 2;
				int j = i;

				for(;; ++i){
					if(tchar[i] == '\0'){
						state = H_ERROR;
						break;
					}
					if(tchar[i] == '\r' && tchar[i + 1] == '\n'){
						break;
					}
					second[i - j] = tchar[i];
				}

				second[i - j] = '\0';

				if(strcmp(second, "keep-alive") == 0){
					keep_alive = true;
				}

			}

			state = H_REPLY;
		}


		if(state == H_CONNECT){
			char id[1024];
			int i_id = 0;
			char passwd[1024];
			int i_passwd = 0;
			int judge = 0;
			for(int j = 0;; ++j){
				if(path[j] == '\0') break;
				if(path[j] == '?'){
					path[j] = '\0';
					judge = 1;
					continue;
				}
				if(judge == 1){
					if(path[j] == '='){
						judge = 2;
						continue;
					}
				}

				if(judge == 2){
					if(path[j] == '&'){
						id[i_id] = '\0';
						judge = 3;
						continue;
					}
					id[i_id++] = path[j];
				}

				if(judge == 3){
					if(path[j] == '='){
						judge = 4;
						continue;
					}
				}

				if(judge == 4){
					passwd[i_passwd++] = path[j];
				}
			}
			
			passwd[i_passwd] = '\0';



			MYSQL *mysql = NULL;

			connectionRAII mysqlcon(&mysql, m_connPool);
			char sql[1024] = {0};
			
			sprintf(sql, "SELECT * FROM people where id='%s' and passwd='%s'", id,passwd );

			if(mysql_query(mysql, sql)){
				printf("mysql_restore_result(): %s\n", mysql_error(mysql));
			}


			MYSQL_RES* result = mysql_store_result(mysql);
			if(!mysql_fetch_row(result)){
				strcpy(path, "/error.html");
			}

			state = H_REPLY;




		}


		if(state == H_REPLY){
			this->decode_str(path, path);

			char *file = path + 1;

			if (strcmp(path, "/") == 0) {
				file = "./";
			}

				//printf("file = %s\n",file);

			struct stat sbuf;


			int ret = stat(file, &sbuf);

			if (ret == -1) {
				send_error(fd, 404, "Not Found", "NO such file or direntry");
                state = H_ERROR;
				break;
			}
			if (S_ISDIR(sbuf.st_mode)) {  		// 目录
				// 发送头信息
				this->send_respond(fd, 200, "OK", get_file_type(".html"), -1);
				// 发送目录信息
				this->send_dir(fd, file);
			}


			if (S_ISREG(sbuf.st_mode)) {     //是一个普通文件
				//回应http协议应答
				this->send_respond(fd, 200, "OK", get_file_type(file), sbuf.st_size);

				//发送文件
				this->send_file(fd, file);
			}
		}








	}while(false);


	if(state == H_ERROR || keep_alive == false){
		delete this;
		return ;
	}


	this->reset();

	pthread_mutex_lock(&qlock);

	mytimer* mtimer = new mytimer(this, 500);

	this->addTimer(mtimer);
	myTimerQueue.push(mtimer);

	pthread_mutex_unlock(&qlock);

	__uint32_t  _epol_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
	if(epoll_mod(epfd, fd, static_cast<void*>(this),_epol_event) < 0){
		delete this;
		return ;
	}








}

void requestData::handleError(int fd, int err_num, string short_msg)

{
    short_msg = " " + short_msg;
    char send_buff[MAX_BUFF];
    string body_buff, header_buff;
    body_buff += "<html><title>TKeed Error</title>";
    body_buff += "<body bgcolor=\"ffffff\">";
    body_buff += to_string(err_num) + short_msg;
    body_buff += "<hr><em> LinYa's Web Server</em>\n</body></html>";

    header_buff += "HTTP/1.1 " + to_string(err_num) + short_msg + "\r\n";
    header_buff += "Content-type: text/html\r\n";
    header_buff += "Connection: close\r\n";
    header_buff += "Content-length: " + to_string(body_buff.size()) + "\r\n";
    header_buff += "\r\n";
    sprintf(send_buff, "%s", header_buff.c_str());
    writen(fd, send_buff, strlen(send_buff));
    sprintf(send_buff, "%s", body_buff.c_str());
    writen(fd, send_buff, strlen(send_buff));

}
















int requestData::hexit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return 0;
}

void requestData::send_respond(int fd, int number, char *disp, const char *type, int len) {
	char buf[1024] = { 0 };
	sprintf(buf, "HTTP/1.1 %d %s\r\n", number, disp);
	writen(fd, buf, strlen(buf));

	sprintf(buf, "Content-Type:%s\r\n", type);
	sprintf(buf + strlen(buf), "Content-Length:%d\r\n", len);

	writen(fd, buf, strlen(buf));
	writen(fd, (void *)"\r\n", 2);
}

//解码  码->中文
void requestData::decode_str(char *to, char *from)
{
	for (; *from != '\0'; ++to, ++from) {
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {
			*to = this->hexit(from[1]) * 16 + this->hexit(from[2]);
			from += 2;
		}
		else {
			*to = *from;
		}
	}
	*to = '\0';
}

//发送文件
void requestData::send_file(int fd, const char *file) {
	int fd_open = open(file, O_RDONLY);
	if (fd_open == -1) {
		send_error(fd, 404, "Not Found", "NO such file or direntry");
		return;
	}
	int n = 0;
	char buf[1024] = { 0 };



	while ((n = read(fd_open, buf, sizeof(buf))) > 0) {
		int ret;
		ret = send(fd, buf, n, 0);
		if (ret == -1) {
			if (errno == EAGAIN || errno == EINTR) {
				continue;
			}
			else
				break;
		}
	}

}

// 通过文件名获取文件的类型
const char *requestData::get_file_type(const char *name)
{
	const char* dot;

	// 自右向左查找‘.’字符, 如不存在返回NULL
	dot = strrchr(name, '.');
	if (dot == NULL)
		return "text/plain; charset=utf-8";
	if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
		return "text/html; charset=utf-8";
	if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
		return "image/jpeg";
	if (strcmp(dot, ".gif") == 0)
		return "image/gif";
	if (strcmp(dot, ".png") == 0)
		return "image/png";
	if (strcmp(dot, ".css") == 0)
		return "text/css";
	if (strcmp(dot, ".au") == 0)
		return "audio/basic";
	if (strcmp(dot, ".wav") == 0)
		return "audio/wav";
	if (strcmp(dot, ".avi") == 0)
		return "video/x-msvideo";
	if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
		return "video/quicktime";
	if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
		return "video/mpeg";
	if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
		return "model/vrml";
	if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
		return "audio/midi";
	if (strcmp(dot, ".mp3") == 0)
		return "audio/mpeg";
	if (strcmp(dot, ".ogg") == 0)
		return "application/ogg";
	if (strcmp(dot, ".pac") == 0)
		return "application/x-ns-proxy-autoconfig";

	return "text/plain; charset=utf-8";
}

//编码 中文->码
void requestData::encode_str(char* to, int tosize, const char* from)
{
	int tolen;

	for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from) {
		if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0) {
			*to = *from;
			++to;
			++tolen;
		}
		else {
			sprintf(to, "%%%02x", (int)*from & 0xff);
			to += 3;
			tolen += 3;
		}
	}
	*to = '\0';
}

void requestData::send_dir(int fd, const char * file) {
	int ret;

	char buf[4094] = { 0 };
	sprintf(buf, "<html><head><title>目录名: %s</title></head>", file);
	sprintf(buf + strlen(buf), "<body><h1>当前目录: %s</h1><table>", file);

	char path[1024] = { 0 };
	char enstr[1024] = { 0 };

	struct dirent** ptr;
	int num = scandir(file, &ptr, NULL, alphasort);



	for (int i = 0; i < num; i++) {

		char *name = ptr[i]->d_name;
		sprintf(path, "%s/%s", file, name);

		//判断是否存在
		struct stat st;
		stat(path, &st);


		//编码  中文->码
		
		this->encode_str(enstr, sizeof(enstr), name);


		if (S_ISDIR(st.st_mode)) {  		// 目录

			sprintf(buf + strlen(buf),
				"<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>",
				enstr, name, (long)st.st_size);
		}

		else if (S_ISREG(st.st_mode)) {     //是一个普通文件
			sprintf(buf + strlen(buf),
				"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
				enstr, name, (long)st.st_size);
		}

		/*
				int src_fd = open(file, O_RDONLY, 0);
				char *src_addr = static_cast<char*>(mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0));
				close(src_fd);
				munmap(src_addr, st.st_size);

		*/

		ret = writen(fd, buf, strlen(buf));
		memset(buf, 0, sizeof(buf));
	}

	sprintf(buf + strlen(buf), "</table></body></html>");
	send(fd, buf, strlen(buf), 0);
}
















mytimer::mytimer(requestData *_request_data, int timeout) : deleted(false), request_data(_request_data)
{
	//cout<<"mytimer()" << endl;
	struct timeval now;

	gettimeofday(&now, NULL);
	expired_time = (now.tv_sec * 1000) + (now.tv_usec / 1000) + timeout;
}

mytimer::~mytimer()
{
	//cout << "~mytimer()" << endl;
	if(request_data != NULL)
	{
		cout << "request_data = " << request_data << endl;
		delete request_data;
		request_data = NULL;
	}
}

void mytimer::update(int timeout)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	expired_time = (now.tv_sec * 1000) + (now.tv_usec / 1000) + timeout;
}

void mytimer::setDeleted()
{
	deleted = true;
}

bool mytimer::isvalid()
{
	struct timeval now;
	gettimeofday(&now, NULL);   //获得当前精确时间
	/*
	long int tv_sec; // 秒数
	long int tv_usec; // 微秒数
	*/

	size_t temp = ((now.tv_sec * 1000) + (now.tv_usec / 1000));
	if (temp < expired_time)   //还没到时间
	{
		return true;
	}
	else
	{
		this->setDeleted();
		return false;
	}
}

void mytimer::clearReq()
{
	request_data = NULL;
	this->setDeleted();
}

bool mytimer::isDeleted() const
{
	return deleted;
}

size_t mytimer::getExpTime() const
{
	return expired_time;
}

bool timeCmp::operator()(const mytimer *a, const mytimer *b) const
{
	return a->getExpTime() > b->getExpTime();
}