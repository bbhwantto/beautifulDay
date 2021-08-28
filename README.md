# beautifulDay

这是一个Linux下C++轻量级Web服务器，是我自己练手的一个小项目。




概述：

        1）利用epoll 边缘触发（ET）IO 复用模型，非阻塞 IO   
        2）使用优先队列定时器，定时关闭一些不活跃连接
        3）利用有限状态机解析 HTTP 请求报文，实现处理静态资源的请求
        4）使用线程池，用多线程来处理服务器与客户端之间的数据收发
        5）利用对当前文件判断，来确定发送的是文件还是目录
        6）通过文件名判断文件类型，就可以发送jpeg、MP3、html等格式内容

服务器测试环境：

 	1.Ubuntu版本16.04

 	2.MySQL版本5.7.29
	
	
测试前确认已安装MySQL数据库：


	// 建立yourdb库
	create database yourdb;
	
	// 创建user表
	USE yourdb;
	CREATE TABLE user(
	    username char(50) NULL,
	    passwd char(50) NULL
	)ENGINE=InnoDB;
	
	// 添加数据
	//数据库登录名,密码,库名
	string user = "root";
	string passwd = "root";
	string databasename = "yourdb";
       
make之后  自定义端口号和选择的目录就可以运行了
