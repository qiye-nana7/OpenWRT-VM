/***********************************************************************************************
	> File Name: server02.c
	> Author: course team of computer networks,
		  School of Information and Software Engineering, UESTC
	> E-mail: networks_cteam@is.uestc.edu.cn 
	> Created Time: 2020.04.02
        > Web site: http://www.is.uestc.edu.cn/

本程序在示例01的基础上修改而来。其功能是客户端用TCP连接到服务器，然后发送消息给服务器。服务器收到后，
打印消息，并消息的长度返回给客户端。服务器端使用了多进程：每个子进程在打印消息时都会列出自己进程PID。
服务器的端口为12345，运行./srv02 即可执行；
客户端执行示例为 ./cli02 x.x.x.x 12345，其中x.x.x.x为服务器的IP。
 **********************************************************************************************/

#include <stdio.h>		//perror()函数
#include <stdlib.h>		//exit()函数相关
#include <unistd.h>		//C 和 C++ 程序设计语言中提供对 POSIX 操作系统 API 的访问功能的头文件
#include <sys/types.h>		//Unix/Linux系统的基本系统数据类型的头文件,含有size_t,time_t,pid_t等类型
#include <sys/socket.h>		//套接字基本函数相关
#include <netinet/in.h>		//IP地址和端口相关定义，比如struct sockaddr_in等
#include <arpa/inet.h>		//inet_pton()等函数相关
#include <string.h>		//bzero()函数相关
#define PORT 12345		//服务器监听端口
#define BACKLOG	5		//listen函数参数
#define BUF_SIZE 1024		//发送接收缓冲区大小

int handle_message_srv(int clientfd);

int main(void)
{
	int 	listenfd, connectfd;			//分别是监听套接字和连接套接字
	struct sockaddr_in	server, client;		//存放服务器和客户端的地址信息（前者在bind时指定，后者在accept时得到）
	int	sin_size;				// accept时使用，得到客户端地址大小信息
	
	int pid;					//进程号

	if((listenfd=socket(AF_INET, SOCK_STREAM, 0))==-1)	//建立监听套接字
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
		pid=fork();
		if(pid==0){								// pid为0，表示在子进程中
			printf("child %d: I get a connection from %s:%d\n", getpid(), inet_ntoa(client.sin_addr),ntohs(client.sin_port));
			close(listenfd);						// 关闭监听套接字
			if(handle_message_srv(connectfd)==-1){				// msg_char_count返回-1表示客户端结束了连接。
				printf("client closed connection. child %d quit.\n",getpid());
				exit(0);
			}
		} 		
		close(connectfd);	//关闭连接套接字
	} 
}


//==================================================
//   int handle_message_srv(int clientfd)
//   本函数的功能是将接收到的消息字符个数发给客户端。
//   函数的入口参数是客户端的套接字描述符，返回值为
//   0，表示处理成功，否则返回-1.
//   
//==================================================

int handle_message_srv(int clientfd)
{
	ssize_t size = 0;
	char buffer[BUF_SIZE];							//数据的缓冲区
	int char_num;

	for(;;){
		bzero(buffer,BUF_SIZE);						//每次I/O前记得清空缓冲区
		size = recv(clientfd, buffer, BUF_SIZE,0);			//从套接字中读取数据，放到缓冲区buffer中
		if(size == 0)							//如果客户端断开
			return -1;	
		printf("child %d received message: %s",getpid(),buffer);	//打印接收到的字符串
		bzero(buffer,BUF_SIZE);	
		sprintf(buffer, "Server reply: received %d bytes altogether\n", size);	//构建响应字符串
		send(clientfd, buffer, strlen(buffer),0);			//发给客户端
	}	
}
