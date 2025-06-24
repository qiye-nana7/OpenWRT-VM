/***********************************************************************************************
	> File Name: file_trans_srv.c
	> Author: course team of computer networks,
		  School of Information and Software Engineering, UESTC
	> E-mail: networks_cteam@is.uestc.edu.cn 
	> Created Time: 2020.04.05
        > Web site: http://www.is.uestc.edu.cn/

������Ĺ����ǿͻ�����TCP���ӵ��������������ļ��������������������Ѹ��ļ����͸��ͻ��ˡ�
�������Ķ˿ڴ�������ָ��������"./ftrans_srv �˿�"����ִ�У�
�ͻ���ִ��ʾ��Ϊ ./ftrans_cli x.x.x.x port������x.x.x.xΪ��������IP��port�Ƿ������Ķ˿�
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
    // ��������ʼ��һ���������˵�socket��ַ�ṹ
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
 
    // ����socket�����ɹ�������socket������
    int server_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if(server_socket_fd < 0)
    {
        perror("Create Socket Failed:\n");
        exit(1);
    }
    int opt = 1;
    setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
 
    // ��socket��socket��ַ�ṹ
    if(-1 == (bind(server_socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr))))
    {
        perror("Server Bind Failed:\n");
        exit(1);
    }
    
    // socket����
    if(listen(server_socket_fd, LENGTH_OF_LISTEN_QUEUE)==-1)
    {
        perror("Server Listen Failed:\n");
        exit(1);
    }
 
    while(1)
    {
        // ����ͻ��˵�socket��ַ�ṹ
        struct sockaddr_in client_addr;
        socklen_t client_addr_length = sizeof(client_addr);
 
        // �����������󣬷���һ���µ�socket(������)�������socket����ͬ���ӵĿͻ���ͨ��
        // accept����������ӵ��Ŀͻ�����Ϣд��client_addr��
        int new_server_socket_fd = accept(server_socket_fd, (struct sockaddr*)&client_addr, &client_addr_length);
        if(new_server_socket_fd < 0)
        {
            perror("Server Accept Failed:\n");
            break;
        }

	inet_ntop(AF_INET, &client_addr.sin_addr, cli_ipAddr, sizeof(cli_ipAddr));
	printf("connected from %s:%d\n", cli_ipAddr, ntohs(client_addr.sin_port));

 
        // recv�����������ݵ�������buffer��
        char buffer[BUFFER_SIZE];
        bzero(buffer, BUFFER_SIZE);
        if(recv(new_server_socket_fd, buffer, BUFFER_SIZE, 0) < 0)
        {
            perror("Server Receive filename Failed:\n");
            break;
        }
 
        // Ȼ���buffer(������)������file_name��
        char file_name[FILE_NAME_MAX_SIZE+1];
        bzero(file_name, FILE_NAME_MAX_SIZE+1);
        strncpy(file_name, buffer, strlen(buffer)>FILE_NAME_MAX_SIZE?FILE_NAME_MAX_SIZE:strlen(buffer));
        printf("client request %s\n", file_name);
 
        // ���ļ�����ȡ�ļ�����
        FILE *fp = fopen(file_name, "r");
        if(NULL == fp)
        {
            printf("File:%s Not Found\n", file_name);
        }
        else
        {
            bzero(buffer, BUFFER_SIZE);
            int length = 0;
            // ÿ��ȡһ�����ݣ��㽫�䷢�͸��ͻ��ˣ�ѭ��ֱ���ļ�����Ϊֹ
            while((length = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0)
            {
                if(send(new_server_socket_fd, buffer, length, 0) < 0)
                {
                    printf("Send File:%s Failed.\n", file_name);
                    break;
                }
                bzero(buffer, BUFFER_SIZE);
            }
 
            // �ر��ļ�
            fclose(fp);
            printf("File:%s Transfer to %s Successful!\n", file_name,cli_ipAddr);
        }
        // �ر���ͻ��˵�����
        close(new_server_socket_fd);
    }
    // �رռ����õ�socket
    close(server_socket_fd);
    return 0;
}
