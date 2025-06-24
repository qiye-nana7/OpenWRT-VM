/*************************************************************************
	> File Name: srv_v4.2.c
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
		(1) C：发送 "PUT filename\n文件长度" 给 S;
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
#include<sys/types.h>   // socket
#include<sys/socket.h>  // socket
#include<stdio.h>       // printf
#include<stdlib.h>      // exit
#include<string.h>      // bzero
#include<sys/stat.h>	// stat
#include <unistd.h>	// access

//#define SERVER_PORT 8000
#define LENGTH_OF_LISTEN_QUEUE 20
#define BUFFER_SIZE 1024
#define FILE_NAME_MAX_SIZE 512

//============声明自定义函数===============
int init_listen(char *listen_port);
int handle_accept(int server_socket_fd, char *client_ip);
int send_ctl_msg(int conn_socket_fd,char *message,int msg_len);
int recv_ctl_msg(int conn_socket_fd,char *msg_buf,int msg_buf_size);
int handle_command(int conn_socket_fd, char *client_ip);
int file_send(int conn_socket_fd,char * file_name, char *client_ip);
int file_recv(int conn_socket_fd, char * file_name, char *client_ip);
int recv_write_file(int conn_socket_fd, char *file_name, unsigned long long filesize);
off_t get_file_size(const char *filename);

//========================================
int main(int argc, char **argv)
{
	// 声明并初始化一个服务器端的socket地址结构
	if (argc != 2)
	{
		printf("Usage: %s  <listening port>\n",argv[0]);
		exit(0);
	}
	int server_socket_fd = init_listen(argv[1]);


	while (1)
	{
		puts("Waiting for connection...(ctrl+C to quit)\n");	//打印等待连接的提示
		char client_ip[INET_ADDRSTRLEN];			// 用来存放客户端IP
		int conn_socket_fd = handle_accept(server_socket_fd, client_ip);	//后续版本，此处用多进程/线程处理
		if (conn_socket_fd < 0)
			break;
		// 如果accept成功，则处理客户端命令，发送或者接收文件
		handle_command(conn_socket_fd, client_ip);
		// 关闭与客户端的连接
		puts("Client disconnected.\n");		
		close(conn_socket_fd);
	}
	// 关闭监听用的socket
	close(server_socket_fd);
	return 0;
}

//==============================================
//	init_listen()：初始化，建立监听套接字
//==============================================

int init_listen(char *listen_port)
{
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htons(INADDR_ANY);
	server_addr.sin_port = htons(atoi(listen_port));	//argv[1] is server listening port
								
	int server_socket_fd = socket(PF_INET, SOCK_STREAM, 0);	// 创建socket，若成功，返回socket描述符
	if (server_socket_fd < 0)
	{
		perror("Create socket failed:\n");
		exit(1);
	}
	int opt = 1;
	setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	// 绑定socket和socket地址结构
	if (-1 == (bind(server_socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr))))
	{
		perror("Server bind failed:\n");
		exit(1);
	}

	// socket监听
	if (listen(server_socket_fd, LENGTH_OF_LISTEN_QUEUE) == -1)
	{
		perror("Server listen failed:\n");
		exit(1);
	}
	return server_socket_fd;

}

//==========================================================================
//	handle_accept():用accept处理客户端连接，返回连接套接字，出错返回-1
//  char *client_ip用来带出客户端IP，其实用处不大，只是为了显示用而已
//==========================================================================
int handle_accept(int server_socket_fd, char *client_ip)
{
	char cli_ipAddr[INET_ADDRSTRLEN];	// used in inet_ntop(), to store IP of client
										
	struct sockaddr_in client_addr;		// 定义客户端的socket地址结构
	socklen_t client_addr_length = sizeof(client_addr);
	// 接受连接请求，返回一个新的socket(描述符)，这个新socket用于同连接的客户端通信
	// accept函数会把连接到的客户端信息写到client_addr中
	int conn_socket_fd = accept(server_socket_fd, (struct sockaddr*)&client_addr, &client_addr_length);
	if (conn_socket_fd < 0)
	{
		perror("Server accept Failed:\n");
		return -1;
	}

	inet_ntop(AF_INET, &client_addr.sin_addr, cli_ipAddr, sizeof(cli_ipAddr));
	printf("Connected from %s:%d\n", cli_ipAddr, ntohs(client_addr.sin_port));
	strcpy(client_ip, cli_ipAddr);		//strcpy不太安全，但这里还好
	return conn_socket_fd;
}


//=====================================================================================
//	send_ctl_msg():将控制消息发给对方（而不是要传的文件）.消息比较短，一次可以发完
//=====================================================================================
int send_ctl_msg(int conn_socket_fd,char *message,int msg_len)
{
	if(send(conn_socket_fd,message,msg_len,0)<0)	
	{	printf("Send message: %s  error.\n",message);
		return -1;
	}
	return 0;
}

//=====================================================================================
//	recv_ctl_msg():接收控制消息字符串（而不是要传的文件）.消息比较短，一般一次可以收完
//=====================================================================================
int recv_ctl_msg(int conn_socket_fd,char *msg_buf,int msg_buf_size)
{

	int msg_len=0;
	bzero(msg_buf, msg_buf_size);
	if((msg_len=recv(conn_socket_fd,msg_buf,msg_buf_size,0))<0)	
	{	printf("Receive message error.\n");
		return -1;
	}
	return msg_len;	


}

//===============================================
//	get_file_size():取得文件大小，文件需小于2G
//===============================================
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

//===============================================
//	get_file_size():取得文件大小，文件可大于2G
//===============================================
off_t get_file_size(const char *filename)  //linux下c语言编程，获取超大文件大小(size超过2GB)：https://blog.popkx.com/linux-c-get-large-file-size-lager-than-2gb/
{  
    struct stat64 buf;  
    if(stat64(filename, &buf)<0)  
        return 0;  
    return (off_t)buf.st_size;  
}


//==============================================
//handle_command()
//解析客户端发来的命令，然后针对不同命令，给出
//回复，然后接收或者发送文件。未来还可以扩展出
//其它命令及应答。
//file_send()和file_recv()分别处理发送和接收；
// client_ip 只是用来显示，用处不大
//==============================================
int handle_command(int conn_socket_fd, char *client_ip)
{
	//按照协议，客户端发过来的正确命令形如： GET ladygaga.mp3 .故空格前的子串是命令，比如GET，空格后是参数，比如文件名
	//因此，将参数拷贝到para_buf中去，便于处理。
	char buf[BUFFER_SIZE];
	char tmp_buf[BUFFER_SIZE];	//用来把每次接收命令后，socket缓冲区可能还余留的垃圾清除掉
	char para_buf[BUFFER_SIZE-4];	// 命令都是大写的三个字符，再除去空格，后面的参数长度最多为BUFFER_SIZE-4
	while (1)
	{
		puts("Waiting for client's command...");		
		bzero(buf,BUFFER_SIZE);
		int rcv_msg_len;		
		if ((rcv_msg_len=recv_ctl_msg(conn_socket_fd, buf, BUFFER_SIZE)) < 0)		// 接收出错
			return -1;
		if(rcv_msg_len==0)		// 客户端关闭连接了
			return 0;
		printf("Received command: %s ( %d bytes)\n",buf,rcv_msg_len);	//显示字节数是为了教学及调试用，本语句可删去
		// 解析客户端命令
			// strstr(str1, str2) 判断str2是否为str1的字串。
			// 若是，则返回str2在str1中首次出现的地址；否则，返回NULL
		if (strstr(buf, "GET") == buf)	
		{
			
			memcpy(para_buf,&buf[4],BUFFER_SIZE-4);	//copy filename to para_buf
			file_send(conn_socket_fd, para_buf,client_ip);	//调用file_send给客户端发送文件
		}
		else if (strstr(buf, "PUT") == buf)
		{
			memcpy(para_buf,&buf[4],BUFFER_SIZE-4);	//copy filename to para_buf
			file_recv(conn_socket_fd, para_buf,client_ip);//调用file_recv接收客户端发的文件		
		}
		else
		{
			printf("Received a wrong command\n");
			send_ctl_msg(conn_socket_fd,"ERR",3);	//命令有问题，则给对方发 ERR,其实不会，除非恶意客户端
		}
	}
	return 0;        // 成功后返回
}



//==============================================
//  file_send():将file_name指定的文件发送给客户端
//==============================================

int file_send(int conn_socket_fd,char *file_name, char *client_ip)	//handle_command传过来的para_buf就是文件名
{
	char buffer[BUFFER_SIZE];
	unsigned long long filesize;

	printf("Client requesting file: %s\n", file_name);
	
	//判断文件是否存在
	if(access(file_name,F_OK)<0)
	{
		printf("File %s not found\n", file_name);
		strncpy(buffer,"ERR", strlen("ERR"));
		send_ctl_msg(conn_socket_fd,buffer,strlen(buffer));
		return -1;
	}
	//取得文件大小，发给客户端，以便对方知道到底要接收多少字节			
	if((filesize=get_file_size(file_name))<0)	//如果取文件大小出错，则发送ERR，返回
	{
		printf("Obtain file %s size failed!\n", file_name);
		strncpy(buffer,"ERR", strlen("ERR"));
		send_ctl_msg(conn_socket_fd,buffer,strlen(buffer));
		return -1;
	}
							//否则将文件长度变为字符串，发给对方，等对方回复READY后，就发送文件给对方
	sprintf(buffer,"%llu",filesize);		
	
	printf("Send file size %s to client\n",buffer);
	send_ctl_msg(conn_socket_fd,buffer,BUFFER_SIZE);

	recv_ctl_msg(conn_socket_fd, buffer, BUFFER_SIZE);

	if(strcmp(buffer,"READY")!=0)
		return -1;
	// 打开文件并读取文件数据
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
	printf("Send file %s to %s successfully,total %llu bytes.\n",file_name,client_ip,totalsend);
	// 关闭文件
	fclose(fp);
	return 0;
}

//==============================================
//	file_recv():接收客户端发来的文件,
//	文件名和长度(两者用\n分隔)在para_buf中。
//==============================================
int file_recv(int conn_socket_fd, char *para_buf, char *client_ip)
{

    char file_name[FILE_NAME_MAX_SIZE+1];
    char buffer[BUFFER_SIZE];
    int para_i,t;
    // 从para_buf中解析出文件名和长度(两者用\n分隔) ,放到file_name和buffer中
    for(para_i=0,t=0;para_buf[para_i]!='\n' && t<=FILE_NAME_MAX_SIZE;para_i++,t++)	//t<=那个，是为了防止溢出，下同
	file_name[t]=para_buf[para_i];
    file_name[t]='\0';
    for(t=0;para_buf[para_i]!='\0' && t<=BUFFER_SIZE-1;para_i++,t++)
	buffer[t]=para_buf[para_i];
    buffer[t]='\0';

    unsigned long long filesize;	//特别注意：文件大小可能上G，因此要用longlong
    filesize=strtoull(buffer,NULL,0);	//将字符串转为无符号长长整数，参见：https://blog.csdn.net/zx7415963/article/details/47803455

    printf("File name received:%s . size: %llu\n",file_name, filesize);

    // 向客户端发送OK，让它发送文件过来
    strncpy(buffer,"OK",strlen("OK"));
    if(send_ctl_msg(conn_socket_fd, buffer, strlen(buffer)) < 0)
        return -1;				// -1 表示操作失败

    if(recv_write_file(conn_socket_fd, file_name, filesize)<0)	//调用recv_write_file函数做正事了
	return -1;

    return 0;
}

//===================================================
// recv_write_file()是从套接字接收指定大小的字节，
// 写入本地文件中。代码流程是从服务器接收数据到buffer中. 
// 每接收一段数据，便将其写入文件中，循环直到文件接收完
// 并写完为止。
//===================================================
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
		printf("File:\t%s Can not open local file to write\n", file_name);
//	close(conn_socket_fd);
		return -1;	//可定义-1为本函数无法打开本地文件的错误码
	}
	// 从客户端接收数据到buffer中. 每接收一段数据，便将其写入文件中，循环直到文件接收完并写完为止	
	bzero(buffer, BUFFER_SIZE);
	while(i<filesize)
	{
		if((length = recv(conn_socket_fd, buffer, BUFFER_SIZE, 0)) < 0)
		{
			perror("Socket recv error!\n");
			return -2;	//可定义-1为本函数套接字接收错误的错误码		
		}
	
		i+=length;

		printf("\33[2K\r");		//下面这4行用来动态显示当前下载了多少，即字节数在不断变化的玩法
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

