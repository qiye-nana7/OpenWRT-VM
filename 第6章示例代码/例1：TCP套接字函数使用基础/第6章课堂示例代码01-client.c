/***********************************************************************************************
	> File Name: client.c
	> Author: course team of computer networks,
		  School of Information and Software Engineering, UESTC
	> E-mail: networks_cteam@is.uestc.edu.cn 
	> Created Time: 2020.04.02
        > Web site: http://www.is.uestc.edu.cn/
 

本程序的功能是客户端用TCP连接到服务器，然后服务器给客户端发送一条欢迎消息。服务器的端口为12345，
运行./srv01 即可执行；客户端执行示例为 ./cli01 x.x.x.x 12345，其中x.x.x.x为服务器的IP。

**********************************************************************************************/

#include <stdio.h>		
#include <stdlib.h>		//exit()函数，atoi()函数
#include <unistd.h>		//C 和 C++ 程序设计语言中提供对 POSIX 操作系统 API 的访问功能的头文件
#include <sys/types.h>		//Unix/Linux系统的基本系统数据类型的头文件,含有size_t,time_t,pid_t等类型
#include <sys/socket.h>		//套接字基本函数
#include <netinet/in.h>		//IP地址和端口相关定义，比如struct sockaddr_in等
#include <arpa/inet.h>		//inet_pton()等函数
#include <string.h>		//bzero()函数

#define PORT 		1234
#define MAXDATASIZE	100

int main(int argc, char *argv[])
{
	int	clientfd, numbytes;		//clientfd是客户端套接字，numbytes是客户端接收到的字节数
	char	buf[MAXDATASIZE];		//缓冲区，用于存放从服务器接收到的信息
    	struct sockaddr_in server_addr;		//存放服务器端地址信息，connect()使用	
    	if (argc != 3){				//如果命令行用法不对，则提醒并退出
    		printf("usage: %s  <server IP address>  <server port>\n",argv[0]);
    		exit(0);
    	}

	if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Create socket failed.");
		exit(1);
	}

    	bzero(&server_addr, sizeof(server_addr));
    	server_addr.sin_family = AF_INET;

    	if(inet_pton(AF_INET, argv[1], &server_addr.sin_addr) == 0)	// argv[1] 为服务器IP字符串，需要用inet_pton转换为IP地址
    	{
        	perror("Server IP Address Error:\n");
        	exit(1);
    	}

    	server_addr.sin_port = htons(atoi(argv[2]));	// argv[2] 为服务器端口，需要用atoi及htons转换

	if (connect(clientfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
		perror("connect failed.");
		exit(1);
	}

	if((numbytes = recv(clientfd, buf, MAXDATASIZE, 0)) == -1) {
		perror("recv error.");
		exit(1);
	}

	buf[numbytes] = '\0';
	printf("Message from server: %s\n",buf);
	close(clientfd);
}
