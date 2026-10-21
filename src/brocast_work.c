#include "brocast_work.h"
#include <stdio.h>       // 标准输入输出函数，如 printf, scanf 等
#include <stdlib.h>      // 标准库函数，如 malloc, free, exit 等
#include <string.h>      // 字符串处理函数，如 strlen, strcpy, memcpy 等
#include <netdb.h>       // 网络数据库操作函数，如 gethostbyname, getaddrinfo 等
#include <netinet/in.h>  // 网络地址结构定义，如 struct sockaddr_in
#include <unistd.h>      // POSIX 操作系统 API 函数，如 close, read, write 等
#include <arpa/inet.h>   // IP 地址转换函数，如 inet_ntoa, inet_aton 等
#include <fcntl.h>       // 文件控制选项，如 fcntl 函数 阻塞非阻塞时调用
#include <errno.h>       // 错误号定义，如 errno 变量
#include "threadcreate.h"
#include "upper_computer/cjson/cJSON.h"
#include "upper_computer/jsonFileReadHelper.h"
#include "upper_computer/common.h"
static int socketBroadcast = 0; //本机UDP套接字
static struct sockaddr_in si_broadcast; //地址结构体，接收udp发送方信息
static socklen_t addrlen; //地址结构体长度，接收udp发送方信息长度
static char recvBuf[512]; //储存收到udp信息
extern struct RunningParam gRunParam; //获取主状态
extern bool run_untill_shutDown; //控制退出用
/*
 * 初始化函数，建立udp套接字并监听8000端口
 */
static int brocast_init()
{
	socketBroadcast = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketBroadcast < 0)
	{
		printf("Broadcast socket error: %d/n", socketBroadcast);
		return -1;
	}
	printf("Broadcast socket created \n");
	// 设置 socket 为非阻塞
	int flags = fcntl(socketBroadcast, F_GETFL, 0);
	fcntl(socketBroadcast, F_SETFL, flags | O_NONBLOCK);

	addrlen = sizeof(si_broadcast);
	memset(&si_broadcast, 0, addrlen);
	si_broadcast.sin_family = AF_INET;
	si_broadcast.sin_port = htons(8000);
	si_broadcast.sin_addr.s_addr = INADDR_ANY; //用本机地址，不制定
	//si_broadcast.sin_addr.s_addr = inet_addr("192.168.1.100");
	//绑定
	int ret = bind(socketBroadcast, (struct sockaddr*) &si_broadcast, sizeof(si_broadcast));
	if (ret == -1)
	{
		printf("bind error for ReceiveBrocast\n");
		return -1;
	}
	return 0;
}
//析构
static int brocast_uninit()
{
	shutdown(socketBroadcast, SHUT_RDWR);
	close(socketBroadcast);
	printf("brocast thread exit\n");
	return 0;
}
//与定制上位机通信必备线程
void* brocast_run(void *arg)
{
	if (brocast_init() != 0)
	{
		printf("DoBrocast start fail\n");
		return NULL;
	}
	printf("brocast_thread ready\n");
	thread_wait_start();	//主线程开启
	printf("brocast_thread go\n");
	while (run_untill_shutDown)
	{
		memset(recvBuf, 0, 512);
		int iRet = recvfrom(socketBroadcast, recvBuf, 511, 0, (struct sockaddr*) &si_broadcast, &addrlen);
		if (iRet == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				usleep(100000); // 休眠 100 毫秒
				continue;
			}
			else
			{
				perror("broadcast udp ::recvfrom");
				break;
			}
		}
		if (iRet > 0)
		{
			printf("DoBrocast Receive:[%s(%d)]:%s \n", (char*) inet_ntoa(si_broadcast.sin_addr), si_broadcast.sin_port, recvBuf);
			char out[16384]= {0};
			hardInfoJsonstr(&out[0]);
			printf("DoBrocast: %s\n", out);
			sendto(socketBroadcast, out, strlen(out), 0, (struct sockaddr*) &si_broadcast, sizeof(si_broadcast));
		}
	}
	brocast_uninit();
	return NULL;
}
