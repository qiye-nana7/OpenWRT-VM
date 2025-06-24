
/***********************************************************************************************
	> File Name: proxy_epoll-3.c
	> Author: course team of computer networks,
		  School of Information and Software Engineering, UESTC
	> E-mail: networks_cteam@is.uestc.edu.cn 
	> Created Time: 2020.02.14
        > Web site: http://www.is.uestc.edu.cn/

本程序的功能是提供通信流透明转发的功能，即类似代理服务器的角色，将程序的通信流转发到别的服务器。
它可以配合其它代理服务器软件如squid，ngnix等进行多级代理转发，可实现保护通信隐私的目的。

程序部署方法如下：用一台远程的服务器A运行squid或ngnix等代理软件，然后在本机运行本程序.
假设本程序编译后名称为 myproxy，则使用示例格式如下：

./myproxy 8888 A的IP A的代理监听端口

其中8888是本机监听端口。

使用时，将本机的浏览器代理地址设置为127.0.0.1:8888，然后就可以通过A间接上网了。

如果要使用本程序保护通信隐私，比如防止被嗅探软件在通信链路上抓包，可采取3级代理方式：
本机浏览器<---->本机myproxy<====>远程服务器myproxy<---->远程服务器代理软件<---->目的服务器

在实际通信中，其中，<====> 这段是外部网络，因此，只要在本机myproxy<====>远程服务器myproxy 这段
采取加密/认证等措施，就可以达到防止嗅探，保护通信隐私的目的。

程序中，int ggg;开头的那几行示例代码（374-376行），就是可加入加密/认证的地方，大家可以采用任何
自己喜欢的方式进行加密/认证，替换那几行代码———无论是常见的一些加解密方法（例如用openssl），
还是自己开发的算法。

备注：
1. 程序使用到了c++的stl库，因此需要用g++来编译。编译命令格式：
g++ proxy_epoll-4.c proxy_epoll.h -o 输出文件名

2. 程序使用了daemon技术，即运行后进入后台，而不是占用控制台窗口。运行日志，在proxy_epoll_log
文件中。

3. Tips: https协议协商时较慢，故使用epoll的LT模式（缺省模式），ET模式可能导致接收/发送不完整，
需自行用循环多次接收。

***********************************************************************************************/
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <syslog.h>/* syslog定义*/
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>/* 错误号定义*/
#include <fcntl.h>/* 文件控制定义*/
#include <unistd.h> /* Unix 标准函数定义*/
#include <signal.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <map> /* C++ STL map容器*/
#include "proxy_epoll.h"

using namespace std;

#define TCP_PROTO "tcp"
#define LOG_FILE "/tmp/daemon.log"
#define MAXEVENTS 64

// 开启daemon, stdout, stderr将被输出到/tmp/daemon.log
int main(int argc, char *argv[])
{
	int i, len, proxy_port;
	if(argc < 4){
		printf("Usage:%s <proxy-port> <host | ip> <service-name | port-number>\n", argv[0]);
		exit(1);
	}
	char buf[32];
	strcpy(buf, argv[1]);
	len = strlen(buf);
    for(i=0 ; i < len; i++)
        if(!isdigit(buf[i]))
            break;
    if(len != i){
        printf("Invalid proxy port %s\n", proxy_port);
        exit(1);
	}
    proxy_port = atoi(argv[1]);
    proxy(proxy_port,argv);
	return 0;
}
// 写错误日志
void errout(char *msg)
{
	if(msg)// 开启daemon错误将被输出到/tmp/daemon.log
        printf("%s\n", msg);
	exit(1);
}
// 重定向标准输入输出
void redirect_stdIO(char *szInFile, char *szOutFile, char *szErrFile)
{
	int fd;
	openlog("proxy_epoll_log", LOG_CONS | LOG_PID, 0);
	if (NULL!= szInFile) {
        fd = open(szInFile, O_RDONLY| O_CREAT, 0666);
        if (fd> 0) {
            // 标准输入重定向
            if (dup2(fd, STDIN_FILENO)< 0) {
                syslog(LOG_ERR, "redirect_stdIO dup2 in");
                exit(1);
            }
            
            close(fd);
        } 
		else
            syslog(LOG_ERR, "redirect_stdIO open %s: %s\n", szInFile, strerror(errno));
    }
    
    if (NULL != szOutFile) {
        fd = open(szOutFile, O_WRONLY| O_CREAT | O_APPEND/*| O_TRUNC*/, 0666);
        if (fd> 0) {
            // 标准输出重定向
            if (dup2(fd, STDOUT_FILENO)< 0) {
                syslog(LOG_ERR, "redirect_stdIO dup2 out");
                exit(1);
            }
            
            close(fd);
        }
        else
            syslog(LOG_ERR, "redirect_stdIO open %s: %s\n", szOutFile, strerror(errno));
    }
    
    if (NULL!= szErrFile) {
        fd = open(szErrFile, O_WRONLY| O_CREAT | O_APPEND/*| O_TRUNC*/, 0666);
        if (fd> 0) {
            // 标准错误重定向
            if (dup2(fd, STDERR_FILENO)< 0){
                syslog(LOG_ERR, "RedirectIO dup2 error\n");
                exit(1);
            }
            
            close(fd);
        }
        else
            syslog(LOG_ERR, "redirect_stdIO open %s: %s\n", szErrFile, strerror(errno));
    }	
    
	closelog();
}
// 设置文件描述符为非阻塞
int set_socket_noblock(int fd)
{
	int flag, ret;
	if((flag = fcntl(fd, F_GETFL, 0)) < 0)
		errout("error fcntl\n");
	ret = fcntl(fd, F_SETFL, flag | O_NONBLOCK);
	return ret;
}

// 守护进程
void daemonize(int listenfd)
{
	pid_t pid;
	// 屏蔽控制终端操作信号I/O stop signal
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
signal(SIGPIPE, SIG_IGN);
	// 重设文件创建掩模
	umask(0);
    // 使守护进程后台运行
	if((pid = fork()) <0)
		errout("fork error\n");
	else if(pid != 0)//父进程终止运行;子进程过继给init进程，其退出状态也由init进程处理,避免了产生僵死进程
        exit(0);
    // 脱离控制终端，登录会话和进程组,调用setsid()使子进程成为会话组长
	setsid();
    // 重定向标准输入输出
	redirect_stdIO("/dev/null", LOG_FILE, LOG_FILE);//重定向标准输入输
	// 改变当前工作目录
    chdir("/");
	/* close any open file descriptors */
	int fd, fdtablesize;
	// fd切记从3开始,想想为什么？
	for(fd = 3, fdtablesize = getdtablesize(); fd < fdtablesize; fd++)
        if (fd != listenfd)
            close(fd);
    // signal(SIGCLD,(sigfunc *)reap_status);
}

int connect_isolate(int usersockfd, char *argv[])
{
	int i, len;
	int isosockfd = -1, connstat = 0;
	struct hostent *hostp; // host entry
	struct servent *servp;
	char buf[64];
	char isolate_host[64];
	char service_name[32];
	strcpy(isolate_host, argv[2]);
	strcpy(service_name, argv[3]);
	struct sockaddr_in  hostaddr;
	bzero(&hostaddr, sizeof(struct sockaddr_in));
	hostaddr.sin_family = AF_INET;
	// parse the isolate  
	if( inet_pton(AF_INET, isolate_host,&hostaddr.sin_addr) != 1){
		if((hostp = gethostbyname(isolate_host)) != NULL)
            bcopy(hostp->h_addr, &hostaddr.sin_addr, hostp->h_length);
		else
            return -1;
    }
    if((servp = getservbyname(service_name, TCP_PROTO)) != NULL)
        hostaddr.sin_port = servp->s_port;
    else if(atoi(service_name) >0)
        hostaddr.sin_port = htons(atoi(service_name));
    else
        return -1;
    // open a socket to connect isolate host
    if((isosockfd = socket(AF_INET, SOCK_STREAM, 0)) <0)
        return -1;
    len = sizeof(hostaddr);
    // attempt a connection
    connstat = connect(isosockfd, (struct sockaddr*)&hostaddr, len);
    switch(connstat){
        case 0:
            break;
        case ETIMEDOUT:
        case ECONNREFUSED:
        case ENETUNREACH:
            strcpy(buf, strerror(errno));
            strcat(buf,"/r/n");
            write(usersockfd,buf,strlen(buf));
            close(usersockfd);
            return -1;
            /*die peacefully if we can't establish a connection*/
        default:
            return -1;
    }
    return isosockfd;
}

/* 
 创建代理服务器端，等待客户端的连接
 并对接受到的数据进行相应的处理
*/
void proxy(int proxy_port, char *argv[])
{
	int i,ret, nready, len;
	map<int, int> sockfd_map;
	int efd, listenfd, usersockfd, isosockfd;
    //声明epoll_event结构体的变量,ev用于注册事件,数组用于回传要处理的事件
	struct epoll_event ev, *pevents;
	struct sockaddr_in serv, cli;
	bzero(&serv, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_port = htons(proxy_port);
	serv.sin_addr.s_addr = htonl(INADDR_ANY);
	// daemonize
	daemonize(listenfd);
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if(listenfd < 0)
        errout("socket error");
	// set listen nonblock
	ret = set_socket_noblock(listenfd);
	if(ret < 0)
        errout("set socket noblock");
	ret = bind(listenfd, (struct sockaddr*)&serv, sizeof(serv));
	if(ret < 0)
        errout("bind error");
	// ready to listen
	ret = listen(listenfd,10);
	if(ret <0 )
        errout("listen error");
	efd = epoll_create1(0);
	if( efd == -1)
        errout("epoll_create1 error");
    //设置与要处理的事件相关的文件描述符
	ev.data.fd = listenfd;
    //设置要处理的事件类型
	ev.events = EPOLLIN;
    //注册epoll事件
	if(epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &ev) < 0)
        errout("epoll_ctl error");
    //生成用于处理accept的epoll专用的文件描述符
	pevents =(struct epoll_event*)calloc(MAXEVENTS, sizeof(struct epoll_event));
	// event loop
	for(; ;){
        //等待epoll事件的发生
		nready = epoll_wait(efd, pevents, MAXEVENTS, -1);
        //处理所发生的所有事件 
		for(i = 0; i < nready; i++){
            // 如果fd出错，或者还不可读
			if((pevents[i].events & EPOLLERR) ||
               (pevents[i].events & EPOLLHUP) ||
               (!(pevents[i].events & EPOLLIN))){
				perror("epoll_wait");
				close(pevents[i].data.fd);
				continue;
			}// 监听到有新的连接
			else if(listenfd == pevents[i].data.fd){
				// one or more connections
				for(; ;){
					socklen_t	cli_len = sizeof(cli);
					// 注意我们虽然不关注 accept 中cli返回信息，但是其参数不能为0
					// 否则出现错误Bad address
					usersockfd = accept(listenfd, (struct sockaddr*)&cli, &cli_len);
					if(usersockfd == -1){
						if(errno == EAGAIN || errno == EWOULDBLOCK){
							//we have processed the connection
							break;// no new client
						}else{// accept failed
							perror("accept");
							break;
						}
					}
					ret = set_socket_noblock(usersockfd);
					if(ret < 0)
                        break;
					// 连接被代理的远程的主机,其中isosockfd为与远程主机连接返回的套接字
					int i_try=1;                    
					while((isosockfd = connect_isolate(usersockfd, argv)) < 0){ //暂时连不上远程主机，可试10次，每次5秒
						if(i_try==10)	
	                        	errout("connect isolate error");
						else i_try++;							
					}

					ret = set_socket_noblock(isosockfd);
					if(ret < 0)
   						break;
                     //设置用于读操作的文件描述符(用户端和代理服务器通信的套接字 usersockfd)
					ev.data.fd = usersockfd;
					ev.events = EPOLLIN ;
                    //注册ev
					ret = epoll_ctl(efd, EPOLL_CTL_ADD, usersockfd, &ev);
					if(ret < 0)
                        errout("epoll ctl error");
                    //设置用于读操作的文件描述符(代理服务器和远程主机通信的套接字 isosockfd)
					ev.data.fd = isosockfd;
					ev.events = EPOLLIN ;
					ret = epoll_ctl(efd, EPOLL_CTL_ADD, isosockfd, &ev);
					if(ret < 0)
                        			errout("epoll ctl error");
                    // 把这对套接字加入到 map中，这样我们只要知道其中的一个套接字就能很快的找到另一个套接字，从而
                    // 通过代理服务器 把信息从客户端和远程主机间进行转发。
                    sockfd_map[usersockfd] = isosockfd;
					sockfd_map[isosockfd] = usersockfd;
				}
				continue;
			} else {
                /* We have data on the fd waiting to be read. Read and
                 display it to the other end. We must read whatever data 
                 is available completely, as we are running in 
                 edge-triggered mode and won't get a notification again 
                 for the same data.*/
				int done = 0;
				while (1) {
					ssize_t count;
					char buf[2048];
					//read the send data 
					count = read(pevents[i].data.fd, buf, sizeof(buf));
					if(count ==-1){	
						/* If errno == EAGAIN, that means we have read all data. 
                         So go back to the main loop. */
						if(errno != EAGAIN){
							perror ("read");
							done = 1;
						}
						break;
					}
					else if (count == 0){
						/* End of file. The remote has closed the connection. */
						done = 1;
						break;
					}
int ggg;	//最简单的加密：按位求反
for(ggg=0;ggg<sizeof(buf);ggg++)
	buf[ggg]=~buf[ggg];
					// count >0, 通过代理服务器的中转把信息发到另一端
					write(sockfd_map[pevents[i].data.fd], buf, count);
					if (ret == -1){	
						perror ("write");
						abort ();
					}
				}
				if (done){
					/* Closing the descriptor will make epoll remove it
                     from the set of descriptors which are monitored. */
					fprintf (stderr, "Closed connection on descriptor %d\n", pevents[i].data.fd);
					close(pevents[i].data.fd);
					close(sockfd_map[pevents[i].data.fd]);
					/* 从map 中把无用的描述符从epoll专用的文件描述符中删去 */
                    sockfd_map.erase(pevents[i].data.fd);
                    sockfd_map.erase(sockfd_map[pevents[i].data.fd]);
				}
			}
		}
	}
	free(pevents);
	pevents = NULL;
}
