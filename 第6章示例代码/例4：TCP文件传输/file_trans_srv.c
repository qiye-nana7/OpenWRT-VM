/***********************************************************************************************
	> File Name: file_trans_srv.c
	> Author: course team of computer networks,
		  School of Information and Software Engineering, UESTC
	> E-mail: networks_cteam@is.uestc.edu.cn 
	> Created Time: 2020.04.05
        > Web site: http://www.is.uestc.edu.cn/

本程序的功能是客户端用TCP连接到服务器，发送文件名给服务器，服务器把该文件发送给客户端。
服务器的端口从命令行指定，运行"./ftrans_srv 端口"即可执行；
客户端执行示例为 ./ftrans_cli x.x.x.x port，其中x.x.x.x为服务器的IP，port是服务器的端口
 **********************************************************************************************/

 
#include<netinet/in.h>  // sockaddr_in
#include<sys/types.h>   // socket
#include<sys/socket.h>  // socket
#include<stdio.h>       // printf
#include<stdlib.h>      // exit
#include<string.h>      // bzero
 
//#define SERVER_PORT 8000
#define LENGTH_OF_LISTEN_QUEUE 20
#define BUFFER_SIZE 1024
#define FILE_NAME_MAX_SIZE 512
 
int main(int argc, char **argv)
{
    // 声明并初始化一个服务器端的socket地址结构
    if (argc != 2){
    printf("usage: %s  <listening port>\n",argv[0]);
    exit(0);
    }
    char cli_ipAddr[INET_ADDRSTRLEN];	// used in inet_ntop(), to store IP of client
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[1]));	//argv[1] is server listening port
 
    // 创建socket，若成功，返回socket描述符
    int server_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if(server_socket_fd < 0)
    {
        perror("Create Socket Failed:\n");
        exit(1);
    }
    int opt = 1;
    setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
 
    // 绑定socket和socket地址结构
    if(-1 == (bind(server_socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr))))
    {
        perror("Server Bind Failed:\n");
        exit(1);
    }
    
    // socket监听
    if(listen(server_socket_fd, LENGTH_OF_LISTEN_QUEUE)==-1)
    {
        perror("Server Listen Failed:\n");
        exit(1);
    }
 
    while(1)
    {
        // 定义客户端的socket地址结构
        struct sockaddr_in client_addr;
        socklen_t client_addr_length = sizeof(client_addr);
 
        // 接受连接请求，返回一个新的socket(描述符)，这个新socket用于同连接的客户端通信
        // accept函数会把连接到的客户端信息写到client_addr中
        int new_server_socket_fd = accept(server_socket_fd, (struct sockaddr*)&client_addr, &client_addr_length);
        if(new_server_socket_fd < 0)
        {
            perror("Server Accept Failed:\n");
            break;
        }

	inet_ntop(AF_INET, &client_addr.sin_addr, cli_ipAddr, sizeof(cli_ipAddr));
	printf("connected from %s:%d\n", cli_ipAddr, ntohs(client_addr.sin_port));

 
        // recv函数接收数据到缓冲区buffer中
        char buffer[BUFFER_SIZE];
        bzero(buffer, BUFFER_SIZE);
        if(recv(new_server_socket_fd, buffer, BUFFER_SIZE, 0) < 0)
        {
            perror("Server Receive filename Failed:\n");
            break;
        }
 
        // 然后从buffer(缓冲区)拷贝到file_name中
        char file_name[FILE_NAME_MAX_SIZE+1];
        bzero(file_name, FILE_NAME_MAX_SIZE+1);
        strncpy(file_name, buffer, strlen(buffer)>FILE_NAME_MAX_SIZE?FILE_NAME_MAX_SIZE:strlen(buffer));
        printf("client request %s\n", file_name);
 
        // 打开文件并读取文件数据
        FILE *fp = fopen(file_name, "r");
        if(NULL == fp)
        {
            printf("File:%s Not Found\n", file_name);
        }
        else
        {
            bzero(buffer, BUFFER_SIZE);
            int length = 0;
            // 每读取一段数据，便将其发送给客户端，循环直到文件读完为止
            while((length = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0)
            {
                if(send(new_server_socket_fd, buffer, length, 0) < 0)
                {
                    printf("Send File:%s Failed.\n", file_name);
                    break;
                }
                bzero(buffer, BUFFER_SIZE);
            }
 
            // 关闭文件
            fclose(fp);
            printf("File:%s Transfer to %s Successful!\n", file_name,cli_ipAddr);
        }
        // 关闭与客户端的连接
        close(new_server_socket_fd);
    }
    // 关闭监听用的socket
    close(server_socket_fd);
    return 0;
}
