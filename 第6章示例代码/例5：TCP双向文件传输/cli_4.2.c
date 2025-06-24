/*************************************************************************
	> File Name: cli_v4.2.c
	> Author: course team of computer networks,
		  School of Information and Software Engineering, UESTC
	> E-mail: networks_cteam@is.uestc.edu.cn 
	> Created Time: 2020.04.05
        > Web site: http://www.is.uestc.edu.cn/
 ************************************************************************/
/*----------------------------------------------------------------------
本程序协议流程描述（v1.3）

术语：分别用C,S代表客户端和服务器
A.C和S建立TCP连接
B.C选择以下功能之一：
1.C下载
(1) C：发送 "GET filename" 给 S;
(2) S：如果命令语法正确，且文件存在，回复 文件长度 给 C，否则回复"ERR" 给 C;
(3) C：如果收到文件长度,发送 "READY" 给 S，否则转B.;
(4) S：如果收到 "READY",发送 文件 给 C，否则等待;
(5) C：接收文件;
(6) C：转B.
2.C上传
(1) C：发送 "PUT filename 文件长度" 给 S;
(2) S：如果命令语法正确，且可以正常接收，回复 "OK"  给 C，否则回复"ERR" 给 C;
(3) C：如果收到"OK",发送 文件 给 S，否则转B.;
(4) S：接收文件
(5) C：发送完毕,转B.
3.退出
-----------------------------------------------------------------------*/

#ifndef _LARGEFILE64_SOURCE	//这部分#ifndef等，是为了取得超过2G的大文件的长度使用：https://blog.popkx.com/linux-c-get-large-file-size-lager-than-2gb/ ，具体在自定义函数get_file_size()里使用
#define _LARGEFILE64_SOURCE
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include<netinet/in.h>  // sockaddr_in
#include<sys/types.h>	// socket
#include<sys/socket.h>  // socket
#include<stdio.h>	// printf
#include<stdlib.h>	// exit,strtoull
#include<string.h>	// bzero,strncmp
#include<sys/stat.h>	// stat,stat64
#include <unistd.h>	// access
 
#define SERVER_PORT 8000
#define BUFFER_SIZE 1024
#define FILE_NAME_MAX_SIZE 512


//============声明自定义函数===============
int connect_remote(char *server_ip,char *port);
int handle_menu(int client_socket_fd);
int put_file(int client_socket_fd);
int get_file(int client_socket_fd);
int send_ctl_msg(int conn_socket_fd,char *message,int msg_len);
int recv_ctl_msg(int conn_socket_fd,char *msg_buf,int msg_buf_size);
int recv_write_file(int conn_socket_fd, char *file_name, unsigned long long filesize);
off_t get_file_size(const char *filename);

//========================================
int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("Usage: %s  <server IP address>  <server port>\n",argv[0]);	//argv[1],argv[2]应分别为服务器的IP、端口
		exit(0);
	}

	int client_socket_fd;
	client_socket_fd = connect_remote(argv[1],argv[2]);	//	建立到远程服务器的连接套接字，该值放到client_socket_fd 中。

	handle_menu(client_socket_fd);		//将client_socket_fd传给handle_menu函数，后者去根据用户的选择，在client_socket_fd上收、发文件
	
	close(client_socket_fd);
	
	return 0;
}


//==============================================
//	send_ctl_msg():将控制消息发给客户端（而不是要传的文件）
//==============================================
int send_ctl_msg(int conn_socket_fd,char *message,int msg_len)
{
	int send_len;	
	if((send_len=send(conn_socket_fd,message,msg_len,0))<0)	
	{	printf("Send message: %s failed\n",message);
		return -1;
	}
	return send_len;
}

//==============================================
//	recv_ctl_msg():接收客户端的控制消息字符串（而不是要传的文件）
//==============================================
int recv_ctl_msg(int conn_socket_fd,char *msg_buf,int msg_buf_size)
{
	int total_recv_len=0;
	bzero(msg_buf, msg_buf_size);
	if((total_recv_len=recv(conn_socket_fd,msg_buf,msg_buf_size,0))<0)	
	{	printf("Receive message failed\n");
		return -1;
	}
	return total_recv_len;	//此处绝不能考虑\0！因为recv接收的是TCP流，上层怎么看待流里面的结尾标记啥的，是上层的事情。
}



//==============================================
//	connect_remote：建立到远程服务器的连接套接字
//==============================================
int connect_remote(char *server_ip,char *port)
{
	// 声明并初始化一个客户端的socket地址结构
	struct sockaddr_in client_addr;
	bzero(&client_addr, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = htons(INADDR_ANY);
	client_addr.sin_port = htons(0);
 
	// 创建socket，若成功，返回socket描述符
	int client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(client_socket_fd < 0)
	{
		perror("Create socket failed\n");
		exit(1);
	}
 
	// 绑定客户端的socket和客户端的socket地址结构 非必需
	if(-1 == (bind(client_socket_fd, (struct sockaddr*)&client_addr, sizeof(client_addr))))
	{
		perror("Client bind failed\n");
		exit(1);
	}
 
	// 声明一个服务器端的socket地址结构，并用服务器那边的IP地址及端口对其进行初始化，用于后面的连接
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	if(inet_pton(AF_INET, server_ip, &server_addr.sin_addr) == 0)	// server_ip 为服务器的IP
	{
		perror("Server IP address error\n");
		exit(1);
	}
	server_addr.sin_port = htons(atoi(port));	// port 为服务器的端口
	socklen_t server_addr_length = sizeof(server_addr);
 
	// 向服务器发起连接，连接成功后，client_socket_fd代表了客户端和服务器的一个socket连接
	if(connect(client_socket_fd, (struct sockaddr*)&server_addr, server_addr_length) < 0)
	{
		perror("Can not connect to server.\n");
		exit(0);
	}
	else
		printf("Connect to server successfully.\n");
	
	return  client_socket_fd;
}

//==================================================
//handle_menu()
//显示菜单，按1发送文件，按2接收文件，其它键退出，
//然后分别调用put_file()和get_file()来处理发送和接收；
//==================================================
int handle_menu(int client_socket_fd)
{
	char choicekey=0;
	while(1)
	{
		printf("Press 1:Send file\t2:Receive file\t\tother:Quit\n");
		choicekey=getchar();		
		//scanf("%d",&choicekey);
		if(choicekey=='1')
			{
			put_file(client_socket_fd);	//函数有返回码，但此版本暂时不处理
			while(getchar()!='\n');			
			}
		else if(choicekey=='2')
			{
			get_file(client_socket_fd);	//函数有返回码，但此版本暂时不处理
			while(getchar()!='\n');			
			}
			
		else
			return 0;
	}
}

//==================================================
//put_file()是将本地文件发送到远程服务器。
//代码流程是先输入文件名，再判断其是否存在，如果
//存在的话就先发送文件名和长度给服务器，然后再发送文件
//内容给服务器。
//==================================================
int put_file(int conn_socket_fd)
{
	// 输入文件名 并放到缓冲区buffer中等待发送
	char file_name[FILE_NAME_MAX_SIZE+1];
	char buffer[BUFFER_SIZE];
	bzero(file_name, FILE_NAME_MAX_SIZE+1);
	bzero(buffer, BUFFER_SIZE);

	printf("Please input file name send to server: ");
	scanf("%s", file_name);		//未控制输入长度，不安全，后续版本应修改为安全的方式

	//正确判断一个文件是否存在用access函数
	if(access(file_name,F_OK)<0)
	{
	printf("File %s not found\n", file_name);
	return -1;
	}	
	
	unsigned long long filesize;	//注意类型，这是为了能处理超过2G的文件
	
	if((filesize=get_file_size(file_name))==0)
	return -1;
	
	sprintf(buffer,"PUT %s\n%llu",file_name,filesize); //构造PUT命令

	printf("PUT command is: %s \n",buffer);	//调试用，可删

	// 向服务器发送buffer中的PUT命令字符串
	if(send_ctl_msg(conn_socket_fd, buffer, strlen(buffer)) < 0)
		return -1;				// -1 表示操作失败

	if(recv_ctl_msg(conn_socket_fd, buffer,BUFFER_SIZE)<0)
	{
	printf("Receive server response failed\n");
	return -1;				// -1 表示操作失败
	} 

	// 如果收到的信息不是OK，表示出错,返回
	if(strstr(buffer, "OK") != buffer)	//用strstr做比较，更方便	
	{
	printf("Receive an error from server\n");
	return -1;				// -1 表示操作失败
	}

	FILE *fp;
	fp = fopen(file_name, "r");
	int length = 0;
	unsigned long long totalsend=0;
	bzero(buffer, BUFFER_SIZE);

	// 每读取一段数据，便将其发送给客户端，循环直到文件读完为止
	while ((length = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0)
	{
		if (send(conn_socket_fd, buffer, length, 0) < 0)	//发送出错，就返回-1
		{
			fclose(fp);
			return -1;
		}
		bzero(buffer, BUFFER_SIZE);
		totalsend+=length;
	}
	printf("Send file %s to server successfully, total %llu bytes.\n",file_name,totalsend);
	// 关闭文件
	fclose(fp);
	return 0;
	
}

//==============================================
//get_file()是从远程服务器下载文件。
//代码流程是先输入要下载的文件名XX，发送给服务器，
//服务器返回文件大小，然后据此接收服务器发送过来的
//数据，存到本地文件XX里。
//==============================================
int get_file(int client_socket_fd)
{
	// 输入文件名 并放到缓冲区buffer中等待发送
	char file_name[FILE_NAME_MAX_SIZE+1];
	bzero(file_name, FILE_NAME_MAX_SIZE+1);
	printf("Please input file name on server: ");
	scanf("%s", file_name);
 
	char buffer[BUFFER_SIZE],tmpbuf[BUFFER_SIZE];
	bzero(buffer, BUFFER_SIZE);
	bzero(tmpbuf,BUFFER_SIZE);
	
	strncpy(tmpbuf, file_name, strlen(file_name)>BUFFER_SIZE?BUFFER_SIZE:strlen(file_name));

	sprintf(buffer, "GET %s", tmpbuf);		//构造GET命令

	printf("GET command is: %s \n",buffer);	//调试用，可删	

	// 向服务器发送buffer中的GET命令字符串
	if(send_ctl_msg(client_socket_fd, buffer, strlen(buffer)) < 0)
		return -1;				// -1 表示操作失败

	if(recv_ctl_msg(client_socket_fd, buffer,BUFFER_SIZE)<0)
	{
	printf("Receive server response failed\n");
	return -1;				// -1 表示操作失败
	} 

	// 如果收到的信息是ERR，表示出错,返回
	if(strstr(buffer, "ERR") == buffer)		
	{
		printf("File not exists on server or can't obtain file size.\n");
		return -1;				// -1 表示操作失败
	}

	unsigned long long filesize;

	filesize=strtoull(buffer,NULL,0);	//将字符串转为无符号长长整数，参见：https://blog.csdn.net/zx7415963/article/details/47803455

	printf("File size is: %llu\n",filesize);
	
	// 如果收到的不是ERR，则是文件大小。于是向服务器发送READY，服务器将发送文件过来
	strncpy(buffer,"READY",strlen("READY")+1);
	if(send_ctl_msg(client_socket_fd, buffer, strlen(buffer)) < 0)
		return -1;				// -1 表示操作失败

	if(recv_write_file(client_socket_fd, file_name, filesize)<0)	//调用recv_write_file函数做正事了
	return -1;

	return 0;
}

//====================================================
// recv_write_file()是从套接字接收指定大小的字节，
// 写入本地文件中。代码流程是从服务器接收数据到buffer中. 
// 每接收一段数据，便将其写入文件中，循环直到文件接收完
// 并写完为止。
//====================================================
int recv_write_file(int conn_socket_fd, char *file_name, unsigned long long filesize)	
{
	// 循环读写:recv 套接字中服务器传入的内容，fwrite 将读取的内容写至目标文件
	// 打开文件，准备写入
	FILE *fp = fopen(file_name, "w");
	char buffer[BUFFER_SIZE];
	int length = 0;
	unsigned long long i=0;
	if(NULL == fp)
	{
		printf("File:\t%s can not open local file to write\n", file_name);
//	close(conn_socket_fd);
		return -1;	//可定义-1为本函数无法打开本地文件的错误码
	}
	// 从服务器接收数据到buffer中. 每接收一段数据，便将其写入文件中，循环直到文件接收完并写完为止	
	bzero(buffer, BUFFER_SIZE);
	while(i<filesize)
	{
		if((length = recv(conn_socket_fd, buffer, BUFFER_SIZE, 0)) < 0)
		{
			perror("Socket recv error\n");
			return -2;	//可定义-1为本函数套接字接收错误的错误码		
		}
	
		i+=length;

		printf("\33[2K\r");		//这4行用来下载了多少的显示，类似百分比不断变化的玩法，
		printf(" %llu bytes received now\r",i);	
		fflush(stdout);
	//	usleep(1000*300);	//延迟0.3秒，对于远程不需要，本地测试，由于太快，可使用这个。
	
		if(fwrite(buffer, sizeof(char), length, fp) < length)
		{
			printf("File:\t%s write from socket failed\n", file_name);
			break;
		}
	//	fflush(fp);	//测试时，发现实际存盘的文件小于原始文件，猜测是文件缓冲区原因，故每次强制fflush存盘————结果发现后面写成close而不是fclose了....
		bzero(buffer, BUFFER_SIZE);
	}
 
	// 接收成功后，关闭文件
	printf("\nReceive file %s successfully!\n", file_name);
	fclose(fp);
	return 0;
}

//=================================================
//	get_file_size():取得文件大小，文件需小于2G
//=================================================
/*
unsigned long get_file_size_smallfile(const char *filename)  
{  
	struct stat buf;  		//参见stat/fstat函数的使用
	if(stat(filename, &buf)<0)  
	{  
		return 0;  
	}  
	return (unsigned long)buf.st_size;  
} 
*/

//=================================================
//	get_file_size():取得文件大小，文件可大于2G
//=================================================
off_t get_file_size(const char *filename)  //linux下c语言编程，获取超大文件大小(size超过2GB)：https://blog.popkx.com/linux-c-get-large-file-size-lager-than-2gb/
{  
    struct stat64 buf;  
    if(stat64(filename, &buf)<0)  
        return 0;  
    return (off_t)buf.st_size;  
}

