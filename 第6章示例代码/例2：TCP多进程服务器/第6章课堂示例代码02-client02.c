/***********************************************************************************************
	> File Name: client02.c
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
#include <stdlib.h>		//exit()函数，atoi()函数
#include <unistd.h>		//C 和 C++ 程序设计语言中提供对 POSIX 操作系统 API 的访问功能的头文件
#include <sys/types.h>		//Unix/Linux系统的基本系统数据类型的头文件,含有size_t,time_t,pid_t等类型
#include <sys/socket.h>		//套接字基本函数
#include <netinet/in.h>		//IP地址和端口相关定义，比如struct sockaddr_in等
#include <arpa/inet.h>		//inet_pton()等函数
#include <string.h>		//bzero()函数

#define BUF_SIZE 1024		//发送接收缓冲区大小

int handle_message_cli(int clientfd);

int main(int argc, char *argv[])
{
	int	clientfd, numbytes;		//clientfd是客户端套接字，numbytes是客户端接收到的字节数
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

	handle_message_cli(clientfd);		

	close(clientfd);
}

//=======================================================
//   void handle_message_cli(int clientfd)
//   本函数的功能是将接收到的消息字符个数发给客户端。
//   函数的入口参数是服务器的套接字描述符。
//   对套接字read/write的错误控制机制，以及限制输入字符串长度，
//   请大家补充。
//   输入#时，函数返回。
//   
//=======================================================

int handle_message_cli(int clientfd)
{
	int size = 0;
	char buffer[BUF_SIZE];						
	
	while(1){							//为学习起见，下面使用read和write处理所有IO
		bzero(buffer,BUF_SIZE);					//每次I/O前记得清空缓冲区
		read(0,buffer,BUF_SIZE);				//从标准输入（键盘）读取数据放到缓冲区buffer中
		size=strlen(buffer);

			if(buffer[0]=='#')				//若输入的第一个字符是#，结束并返回
				return 0;			
			write(clientfd, buffer, size);			//把键盘输入的字符串发送给服务器
			bzero(buffer,BUF_SIZE);
			size = read(clientfd, buffer, 1024);		//从服务器接收数据
			write(1,buffer,BUF_SIZE);			//写到标准输出,即打印接收到的字符串
	}	
}
