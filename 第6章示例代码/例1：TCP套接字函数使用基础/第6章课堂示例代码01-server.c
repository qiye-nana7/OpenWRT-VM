/***********************************************************************************************
	> File Name: server.c
	> Author: course team of computer networks,
		  School of Information and Software Engineering, UESTC
	> E-mail: networks_cteam@is.uestc.edu.cn 
	> Created Time: 2020.04.02
        > Web site: http://www.is.uestc.edu.cn/
 

本程序的功能是客户端用TCP连接到服务器，然后服务器给客户端发送一条欢迎消息。服务器的端口为12345，
运行./srv01 即可执行；客户端执行示例为 ./cli01 x.x.x.x 12345，其中x.x.x.x为服务器的IP。

**********************************************************************************************/

#include <stdio.h>		
#include <stdlib.h>		//exit()函数相关
#include <unistd.h>		//C 和 C++ 程序设计语言中提供对 POSIX 操作系统 API 的访问功能的头文件
#include <sys/types.h>		//Unix/Linux系统的基本系统数据类型的头文件,含有size_t,time_t,pid_t等类型
#include <sys/socket.h>		//套接字基本函数相关
#include <netinet/in.h>		//IP地址和端口相关定义，比如struct sockaddr_in等
#include <arpa/inet.h>		//inet_pton()等函数相关
#include <string.h>		//bzero()函数相关
#define PORT 12345		//服务器监听端口
#define BACKLOG	5		//listen函数参数

int main(void)
{
	int 	listenfd, connectfd;			//分别是监听套接字和连接套接字
	struct sockaddr_in	server, client;		//存放服务器和客户端的地址信息（前者在bind时指定，后者在accept时得到）
	int	sin_size;				// accept时使用，得到客户端地址大小信息

	if((listenfd=socket(AF_INET, SOCK_STREAM, 0))==-1)	//建立监听套接子
	{
		perror("Create socket failed.");		//perror是系统函数，参见https://www.cnblogs.com/noxy/p/11188583.html
		exit(-1);	}

	int opt = SO_REUSEADDR;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));	//将地址和端口设为可立即重用（后续再解释）

	bzero(&server, sizeof(server));						//这4行是设置地址结构变量的标准做法，可直接套用
	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listenfd, (struct sockaddr *)&server, sizeof(struct sockaddr))==-1) {	//把server里的地址信息绑定到监听套接字上
		perror("Bind error.");
		exit(-1);
	}

	if (listen(listenfd, BACKLOG) == -1) {						//开始监听
		perror("listen error.");
		exit(-1);
	}

	sin_size = sizeof(struct sockaddr_in);

	while(1) {
		if ((connectfd = accept(listenfd, (struct sockaddr *)&client, &sin_size)) == -1) {	//接受客户端连接（从监听队列里取出）
			perror("accept error.");
			exit(-1);
		}
		printf("You get a connection from %s\n", inet_ntoa(client.sin_addr));
		send(connectfd, "Welcome to my server.\n", 22, 0);					//给客户端发一条信息（字符串）
		close(connectfd);	//关闭连接套接字
	} 

	close(listenfd);		//关闭监听套接子
}
