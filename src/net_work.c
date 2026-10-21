#include "net_work.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "upper_computer/common.h"
#include "upper_computer/protoHelper.h"
#include "main_thread.h"
#include "log.h"

#define MAX_CLIENTS 5
static char str_time[32];
extern pthread_t thr_process, thr_save;  //处理线程
extern struct kfifo gM2RFifoState;
extern struct kfifo gM2PFifoState;
extern struct kfifo gM2SFifoState;
/* Epoll Param */
static int server_fd = -1;
extern int mainEpollfd;
extern char normalSaveRoad[256];
extern char planSaveRoad[256]; //本次上电数据记录路径
extern RunningParam gRunParam;

/* tcp buff struct */
#define MAXSENDLENGTH 8*1024*1024
#define MAXRECEIVELENGTH 1*1024*1024
static int cmdReply;
struct recvbuf
{
    unsigned char buf[MAXRECEIVELENGTH];
    int rsize;
};

struct client_info
{
    int fd;
    struct recvbuf recv_buf;
    bool brecivedata;
};

struct client_info clients[MAX_CLIENTS];
static int occupy_fd = -1;

unsigned char gSendbuf[MAXSENDLENGTH];
char logdata[512];

/*
 * 初始化tcp
 */
int init_tcp()
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].fd = -1;
        clients[i].brecivedata=false;
    }

    occupy_fd = -1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        printf("init server_fd fail");
        return -1;
    }

    uint32_t nSendBuf = 8 * 1024 * 1024;
    uint32_t nRecvBuf = 1 * 1024 * 1024;
    if (setsockopt(server_fd, SOL_SOCKET, SO_SNDBUF, (const char*) &nSendBuf, sizeof(uint32_t)) < 0)	//设置最大发送缓冲区
    {
        writelog("fail to set send buff length\n");
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVBUF, (const char*) &nRecvBuf, sizeof(uint32_t)) < 0)	//设置最大接收缓冲区
    {
        writelog("fail to set recv buff length\n");
    }

    int keepAlive = 1; // 开启keepalive属性
    int keepIdle = 5; // 如该连接在60秒内没有任何数据往来,则进行探测
    int keepInterval = 1; // 探测时发包的时间间隔为5 秒
    int keepCount = 3; // 探测尝试的次数.如果第1次探测包就收到响应了,则后2次的不再发.

    if (setsockopt(server_fd, SOL_SOCKET, SO_KEEPALIVE, (void*) &keepAlive, sizeof(keepAlive)) < 0)
    {
        writelog("fail to set SO_KEEPALIVE\n");
        printf("fail to set SO_KEEPALIVE\n");
    }
    if (setsockopt(server_fd, SOL_TCP, TCP_KEEPIDLE, (void*) &keepIdle, sizeof(keepIdle)) < 0)
    {
        writelog("fail to set TCP_KEEPIDLE\n");
        printf("fail to set TCP_KEEPIDLE\n");
    }
    if (setsockopt(server_fd, SOL_TCP, TCP_KEEPINTVL, (void*) &keepInterval, sizeof(keepInterval)) < 0)
    {
        writelog("fail to set TCP_KEEPINTVL\n");
        printf("fail to set TCP_KEEPINTVL\n");
    }
    if (setsockopt(server_fd, SOL_TCP, TCP_KEEPCNT, (void*) &keepCount, sizeof(keepCount)) < 0)
    {
        writelog("fail to set TCP_KEEPCNT\n");
        printf("fail to set TCP_KEEPCNT\n");
    }
    struct timeval timeout;
    timeout.tv_sec = 0;  // 设置超时时间为0。1秒
    timeout.tv_usec = 300 * 1000;
    if (setsockopt(server_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("setsockopt");
    }

    int tcp_timeout = 5000; //心跳超时
    if (setsockopt(server_fd, SOL_TCP, TCP_USER_TIMEOUT, &tcp_timeout, sizeof(int)) < 0)
    {
        writelog("fail to set HeartTimeout\n");
        printf("fail to set HeartTimeout\n");
    }
    //设置关闭socket后马上断线，不再尝试各种重发
    struct linger linger_opt;
    linger_opt.l_onoff = 1;
    linger_opt.l_linger = 0;
    if (setsockopt(server_fd, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt)) < 0)
    {
        writelog("fail to set SO_LINGER \n");
        printf("fail to set SO_LINGER \n");
    }
    //bind
    struct sockaddr_in my_addr;	//设置tcp地址
    bzero(&my_addr, sizeof(my_addr));	//置0
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(3000);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);	//本机任意ip
    int ret = bind(server_fd, (struct sockaddr*) &my_addr, sizeof(my_addr));	//绑定tcp端口号
    if (ret == -1)
    {
        perror("bind error for initTCP\n");
        return -1;
    }
    printf("bind succeed in initTCP\n");
    printf("[%s(%d)]\n", (char*) inet_ntoa(my_addr.sin_addr), my_addr.sin_port);

    //开启listen
    if (listen(server_fd, 5) != 0)	//监听最大10个客户端
        printf("fail to sockfd listen \n");
    printf("network listen start\n");

    //将服务端端口套接字放入监听，否则无法收到上位机客户端消息
    struct epoll_event ev;
    ev.data.fd = server_fd;
    ev.events = EPOLLIN;

    ret = epoll_ctl(mainEpollfd, EPOLL_CTL_ADD, server_fd, &ev);
    if (-1 == ret)
    {
        perror("epoll_ctl fail");
        return -1;
    }
    printf("server_fd join to epoll\n");
    return 0;
}
/*
 * 关闭客户端
 */
void close_client(int client_fd)
{
    sprintf(logdata, "开始踢掉客户端  %d\n", client_fd);
    writelog(logdata);
    if (client_fd == -1)
        return;

    if(occupy_fd == client_fd)
    {
        occupy_fd = -1;
    }
    int ret = epoll_ctl(mainEpollfd, EPOLL_CTL_DEL, client_fd, NULL);
    if (-1 == ret)
    {
        perror("client_fd delete from epoll fail!\n");
    }
    printf("client_fd delete from epoll success!\n");
    close(client_fd);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i].fd == client_fd)
        {
            clients[i].fd = -1;
            clients[i].brecivedata=false;
            break;
        }
    }

    int active_clients = 0;
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i].fd != -1)
        {
            active_clients++;
        }
    }

    if (gRunParam.m_state.isConnect && active_clients == 0)
    {
        gRunParam.m_state.isConnect = false;
        sendUnconnected();
    }

    sprintf(logdata, "成功踢掉客户端 %d \n", client_fd);
    writelog(logdata);
    printf(logdata);
}
/*
 * 关闭tcp
 */
int deinit_tcp()
{
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i].fd != -1)
        {
            close_client(clients[i].fd);
        }
    }

    //将服务端端口套接字放入监听，否则无法收到上位机客户端消息
    struct epoll_event ev;
    ev.data.fd = server_fd;
    ev.events = EPOLLIN;
    int ret = epoll_ctl(mainEpollfd, EPOLL_CTL_DEL, server_fd, &ev);
    if (-1 == ret)
    {
        perror("server_fd delete from epoll fail!\n");
        return -1;
    }
    printf("server_fd delete from epoll success!\n");
    close(server_fd);
    return 0;
}
/*
 * 发送数据给客户端
 */
int send_data(char *data)
{
    int ssendlen = 0;
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i].fd == -1 || 	!clients[i].brecivedata)
        {
            continue;
        }
        int len = 0;
        memcpy(gSendbuf, VI_DELIMITER, strlen(VI_DELIMITER));	//add delimiter at end
        len = DataEncode(gSendbuf + strlen(VI_DELIMITER), data);
        if (len <= 0)
        {
            printf("fail to dataencode\n");
            return -2;
        }
        memcpy(gSendbuf + len + strlen(VI_DELIMITER), VI_DELIMITER, strlen(VI_DELIMITER));	//add delimiter at end
        int sendlen = send(clients[i].fd, (char*) gSendbuf, len + 2 * strlen(VI_DELIMITER), MSG_NOSIGNAL);

        if (sendlen == -1)
        {
            printf("send data to upper fail!!!\n ");
            //close_client();
        }
        else
        {
            ssendlen += sendlen;
        }
    }
    return ssendlen;
}
/*
 * 接收上位机数据
 */
void do_recieve(struct epoll_event *ev)
{
    if (ev->events & EPOLLRDHUP)
    {
        writelog("收到 EPOLL RDHUP 消息\n");
        do_error(ev);
    }
    else
    {
        if (server_fd == ev->data.fd)
        {
            printf("server fd receive data\n");
            if (FindNewClient() <= 0)
                return;
        }
        else
        {
            for (int i = 0; i < MAX_CLIENTS; ++i)
            {
                if (clients[i].fd == ev->data.fd)
                {
                    CatPacketAndPipe(clients[i].fd, clients[i].recv_buf.buf, &clients[i].recv_buf.rsize);
                    break;
                }
            }
        }
    }

}
void do_error(struct epoll_event *ev)
{
    if (server_fd == ev->data.fd)
    {
        writelog("收到服务端错误 \n");
        deinit_tcp();
    }
    else
    {
        sprintf(logdata, "收到客户端错误消息 fd:%d \n", ev->data.fd);
        writelog(logdata);
        close_client(ev->data.fd);
    }
}

/*
 * 发现新客户端
 */
int FindNewClient()
{
    struct sockaddr_in cli_addr;
    unsigned int clilen = sizeof(cli_addr);
    int connfd = accept(server_fd, (struct sockaddr*) &cli_addr, &clilen);
    if (connfd == -1)
    {
        perror("accept error\n");
        return -1;
    }

    // 设置关闭socket后马上断线，不再尝试各种重发
    struct linger linger_opt;
    linger_opt.l_onoff = 1;
    linger_opt.l_linger = 0;
    if (setsockopt(connfd, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt)) < 0)
    {
        writelog("fail to set client SO_LINGER \n");
        printf("fail to set client SO_LINGER \n");
    }

    struct epoll_event event;
    event.data.fd = connfd;
    event.events = EPOLLIN | EPOLLRDHUP;

    int flags = fcntl(connfd, F_GETFL, 0);
    if (flags & O_NONBLOCK)
    {
        printf("sockfd_client is O_NONBLOCK.\n");
    }
    else
    {
        printf("sockfd_client is BLOCK.\n");
    }
    int ret = epoll_ctl(mainEpollfd, EPOLL_CTL_ADD, connfd, &event);
    if (-1 == ret)
    {
        perror("client_fd join to epoll fail\n");
        close(connfd);
        return -1;
    }

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i].fd == -1)
        {
            printf("%d \n", i);
            clients[i].fd = connfd;
            clients[i].recv_buf.rsize = 0;
            break;
        }
    }

    int active_clients = 0;
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i].fd != -1)
        {
            active_clients++;
        }
    }

    if (!gRunParam.m_state.isConnect && active_clients > 0)
    {
        gRunParam.m_state.isConnect = true;
        sendConnected();
    }

    printf("client connected successfully, fd: %d\n", connfd);
    return 0;
}
/*
 * 解析从客户端收到的指令包
 */
void CatPacketAndPipe(int sockfd, unsigned char *pbuf, int *rsize)
{
    unsigned int len = 0;
    //	printf("in DoReceive lenth :   %d  \n", len);
    if ((len = recv(sockfd, pbuf + *rsize, MAXRECEIVELENGTH - *rsize, 0)) <= 0)
    {
        writelog("客户端发送奇怪消息 %d:\n");
        close_client(sockfd);
    }
    else
    {
        //printf("in DoReceive lenth :   %d  \n", len);
        *rsize += len;
        do
        {
            int position = find_delimiter((char*) pbuf, 0, *rsize);
            if (position < 0)
            {
                return;
            }
            else
            {
                uint8_t *cmdBuf = pbuf;
                int cmdLen = position;
                //		printf("in DoReceive cmdLen :   %d  \n", cmdLen);
                if (position > 0)
                {
                    ProcessCmd(cmdBuf, cmdLen, sockfd);
                }
                (*rsize) -= (position + strlen(VI_DELIMITER));
                if (*rsize > 0)
                {
                    memmove(pbuf, pbuf + position + strlen(VI_DELIMITER), *rsize);
                }
                else if (0 == *rsize)
                {
                    return;
                }
            }
        }
        while (1);
    }
}

/*
 * 解析指令
 */
void ProcessCmd(uint8_t *cmdBuf, int cmdLen, int sockfd)
{
    //	printf("ProcessCmd  cmdLen:  %d\n", cmdLen);
    int headlen = sizeof(struct Header);    //4 + 4 * sizeof(unsigned int);
    if (cmdLen < headlen)
    {
        printf("in rt_work receive recLen is too short:\n");
        return;
    }
    struct Header head;
    headerDecode(&head, cmdBuf, headlen);
    CMDStrm *message = cmdstrm__unpack(NULL, cmdLen - headlen, (const uint8_t*) (cmdBuf + headlen));
    if (NULL == message)
    {
        printf("error in unpack\n");
        writelog("无法解开的上位机指令\n");
        return;
    }

    //	printf("success in unpack\n");
    sprintf(logdata, "成功解开上位机指令:\ncmd类型:%d cmdNo:%d\n", message->ncmdtype,message->ncmdno);
    writelog(logdata);
    //printf(logdata);
    if(	message->ncmdtype == OCCUPY_DEVICE)
    {
        sprintf(logdata, "OCCUPY_DEVICE\n");
        writelog(logdata);
        printf(logdata);

        if(occupy_fd == -1 && !gRunParam.m_state.isSampling)//无人连接，未开工作直接占用
        {
            printf("无人连接，未开工作直接占用\n");
            occupy_fd = sockfd;
            gRunParam.m_state.runId = message->ncmdno;
            gRunParam.m_state.occupy_reply = -2;

            memset(str_time, 0x00, sizeof(str_time));
            ConvertTimestampToFormat(message->synctime, str_time);
            if (setLocaltime(message->synctime) != 0) //设置失败
                Reply(SYNCTIMEFAIL, message->ncmdno, sockfd);
            Reply(OCCUPY_REPLY, message->ncmdno, sockfd);
            hostCanRecive(sockfd);

            sprintf(logdata, "OCCUPY_DEVICE: succuss,timesync：%s\n",str_time);
            writelog(logdata);
            printf(logdata);
        }
        else if(occupy_fd == -1 && gRunParam.m_state.isSampling )
        {
            printf("无人连接，正在采集 单台 提示重置或恢复\n");
            gRunParam.m_state.occupy_reply = -2;//无人连接，正在采集 单台 提示重置或恢复
            Reply(OCCUPY_REPLY, message->ncmdno, sockfd);
        }
        else if(occupy_fd != -1)
        {
            printf("//已有设备连接 提示重置\n");
            gRunParam.m_state.occupy_reply = -2;//
            Reply(OCCUPY_REPLY, message->ncmdno, sockfd);
        }
        return;
    }
    else if(message->ncmdtype == GETPARAM)
    {
        sprintf(logdata, "收到指令：GETPARAM\n");
        writelog(logdata);
        printf(logdata);
        Reply(DEVICEPARAM, message->ncmdno, sockfd);
        return;
    }
    else if(message->ncmdtype ==  GETSTATE)
    {
        sprintf(logdata, "收到指令：GETSTATE\n");
        writelog(logdata);
        printf(logdata);
        Reply(DEVICESTATE, message->ncmdno, -2);
        return;
    }
    if(sockfd != occupy_fd)
    {
        sprintf(logdata, "丢弃指令：%d not %d\n");
        writelog(logdata);
        printf(logdata);
        return;
    }
    sprintf(logdata, "收到指令：message->ncmdtyp:%d\n",message->ncmdtype);
    //printf(logdata);

    gRunParam.m_state.runId = message->ncmdno;

    switch (message->ncmdtype)
    {
    case UPDATEPARAM:
    {

        sprintf(logdata, "收到指令：冷更新\n");
        writelog(logdata);
        printf(logdata);
        if (gRunParam.m_state.isSampling)
        {
            Reply(UPDATEFAIL, message->ncmdno, -2);
        }
        else
        {
            if(UpdateRunParam(message)==0)
            {
                Reply(UPDATESUCCESS, message->ncmdno, -2);
                Reply(CLOCKLOCKED, message->ncmdno, -2);
                writeColdUpdateInfo();
                sendUpdate();
            }
            else
                Reply(UPDATEFAIL, message->ncmdno, -2);
        }

        break;

    }
    case RESUMEDATASEND:
    {
        sprintf(logdata, "收到指令：RESUMEDATASEND\n");
        writelog(logdata);
        printf(logdata);
        gRunParam.m_state.runId = message->ncmdno;

        hostCanRecive(sockfd);
        Reply(RESUMEDATASEND_REPLY, message->ncmdno, -2);
        break;
    }
    case GETPARAM:
    {
        sprintf(logdata, "收到指令：GETPARAM\n");
        writelog(logdata);
        printf(logdata);
        Reply(DEVICEPARAM, message->ncmdno, -2);
        break;
    }
    case STARTDAQ:
    {
        gRunParam.nStartDaqTime = message->daqstarttime;
        sprintf(logdata, "收到指令： 开始采集,开始时间微妙:%lld\n", gRunParam.nStartDaqTime);
        writelog(logdata);
        printf(logdata);
        sendRtStart();
        break;
    }
    case STOPDAQ:
    {
        sprintf(logdata, "收到指令： 停止采集\n");
        writelog(logdata);
        printf(logdata);
        sendRtStop();
        break;
    }
    case STARTSAVE:
    {
        memset(gRunParam.strSaveName,0,sizeof(gRunParam.strSaveName));
        strcpy(gRunParam.strSaveName,normalSaveRoad);
        strcat(gRunParam.strSaveName, message->strsavefilename);
        gRunParam.nStartSaveTime = message->savestarttime;
        gRunParam.nSaveTimeDelay = (message->savestarttime - gRunParam.nStartDaqTime) / 1000;
        sprintf(logdata, "收到指令：开始存储 存储时间差:%lld ms\n",	gRunParam.nSaveTimeDelay);
        writelog(logdata);
        printf(logdata);
        sendSaveStart();
        break;
    }
    case STOPSAVE:
    {
        sprintf(logdata, "收到指令： 停止存储\n");
        writelog(logdata);
        printf(logdata);
        sendSaveStop();
        break;
    }
    case SYNCTIME:
    {
        memset(str_time, 0x00, sizeof(str_time));
        ConvertTimestampToFormat(message->synctime, str_time);
        sprintf(logdata, "收到指令：同步时间,时间:%s\n", str_time);
        writelog(logdata);
        printf(logdata);
        if (setLocaltime(message->synctime) != 0) //设置失败
            Reply(SYNCTIMEFAIL, message->ncmdno, sockfd);
        else
            //设置成功
            Reply(SYNCTIMESUCCESS, message->ncmdno, sockfd);

        sprintf(logdata, "收到指令：切换运行模式,要求模式:%s\n",  message->operatingmode);
        writelog(logdata);
        printf(logdata);

        OtherhostCanNotRecive(sockfd);
        Reply(SWITCHMODESUCESS, message->ncmdno, sockfd);
        hostCanRecive(sockfd);
        writelog("模式切换成功，允许收数据!\n");
        printf("模式切换成功，允许收数据!\n");

        break;
    }
    case WRITESPI:
    {
        for (int i = 0; i < message->n_zerochannel; i++)
        {
            gRunParam.m_chnldefs.m_AnalogChnlParam[message->zerochannel[i]]->bZero = true;
            sprintf(logdata, "收到指令：修改调零参数，通道号:%d\n", message->zerochannel[i] + 1);
            writelog(logdata);
            printf(logdata);
        }
        gRunParam.m_state.isCountZero = true;
        break;
    }
    //变更系统状态，发送给RT更新参数
    case TOTALINTERLOCK_OPEN:
    {
        sprintf(logdata, "收到指令：总闭锁打开\n");
        writelog(logdata);
        printf(logdata);
        gRunParam.m_state.lock_state = true;
        cmdReply = TOTALINTERLOCK_OPEN;
        kfifo_write_pack(&gM2RFifoState, &cmdReply, sizeof(cmdReply));
        break;
    }
    //变更系统状态，发送给RT更新参数
    case TOTALINTERLOCK_CLOSE:
    {
        sprintf(logdata, "收到指令：总闭锁关闭\n");
        writelog(logdata);
        printf(logdata);
        gRunParam.m_state.lock_state = false;
        cmdReply = TOTALINTERLOCK_CLOSE;
        kfifo_write_pack(&gM2RFifoState, &cmdReply, sizeof(cmdReply));
        break;
    }
    //变更系统状态，发送给RT更新参数
    case HOTUPDATEPARAM:
    {
        sprintf(logdata, "收到指令：热更新\n");
        writelog(logdata);
        printf(logdata);

        if(UpdateHotRunParam(message)==0)
        {
            Reply(UPDATESUCCESS, message->ncmdno, -2);
            Reply(CLOCKLOCKED, message->ncmdno, -2);
            writeHotUpdateInfo();
            cmdReply = HOTUPDATEPARAM;
            kfifo_write_pack(&gM2RFifoState, &cmdReply, sizeof(cmdReply));
        }
        else
            Reply(UPDATEFAIL, message->ncmdno, -2);

        break;
    }
    //变更系统状态，发送给RT更新参数
    case STARTTEST:
    {
        sprintf(logdata, "收到指令：开始谱\n");
        writelog(logdata);
        printf(logdata);

   //     memset(gRunParam.strSaveName,0,sizeof(gRunParam.strSaveName));
    //    strcpy(gRunParam.strSaveName,normalSaveRoad);
      //  strcat(gRunParam.strSaveName, message->strsavefilename);
      //  printf("%s \n",gRunParam.strSaveName);
       //       gRunParam.nStartSaveTime = message->savestarttime;
      //        printf("%lld \n",gRunParam.nStartSaveTime);
        //       gRunParam.nSaveTimeDelay = (message->savestarttime - gRunParam.nStartDaqTime) / 1000;
        //       sprintf(logdata, "收到指令：开始存储 存储时间差:%lld ms\n",   gRunParam.nSaveTimeDelay);
        //      writelog(logdata);
        //      printf(logdata);
        gRunParam.m_state.test_state=1;
        cmdReply = STARTTEST;
        kfifo_write_pack(&gM2RFifoState, &cmdReply, sizeof(cmdReply));
        break;
    }
    //变更系统状态，发送给RT更新参数
    case PAUSETEST:
    {
        sprintf(logdata, "收到指令：暂停谱\n");
        writelog(logdata);
        printf(logdata);
        gRunParam.m_state.test_state=2;
        cmdReply = PAUSETEST;
        kfifo_write_pack(&gM2RFifoState, &cmdReply, sizeof(cmdReply));
        break;
    }
    //变更系统状态，发送给RT更新参数
    case STOPTEST:
    {
        sprintf(logdata, "收到指令：停止谱\n");
        writelog(logdata);
        printf(logdata);
        gRunParam.m_state.test_state=0;
        cmdReply = STOPTEST;
        kfifo_write_pack(&gM2RFifoState, &cmdReply, sizeof(cmdReply));
        break;
    }
    case UPDATEALGORITHM:
    {
        sprintf(logdata, "收到指令：更新算法库\n");
        writelog(logdata);
        printf(logdata);
        if(UpdateLgorithm(message)==0)
        {
            Reply(UPDATESUCCESS, message->ncmdno, -2);
            cmdReply = UPDATEALGORITHM;
            kfifo_write_pack(&gM2RFifoState, &cmdReply, sizeof(cmdReply));
        }
        else
            Reply(UPDATEFAIL, message->ncmdno, -2);
        break;
    }

    case CLEARDATA:
    {
        sprintf(logdata, "收到指令：删除历史文件\n");
        writelog(logdata);
        printf(logdata);
        if (clearLog() == 0 && clearSaveData() == 0)
        {
            syncCMD();
            sendSaveUpdate();
            //发出更新剩余空间指令CLEARDATA
            Reply(CLEARDATASUCESS, message->ncmdno, sockfd);
        }
        else
        {
            syncCMD();
            sendSaveUpdate();
            //发出更新剩余空间指令CLEARDATA
            Reply(CLEARDATAFAIL, message->ncmdno, sockfd);
        }
        break;
    }
    default:
        break;
    }
    cmdstrm__free_unpacked(message, NULL);
}
/*
 * 截取字符串
 */
int find_delimiter(char *data, int start, int end)
{
    if (start == end)
        return -1;
    int sLen = strlen(VI_DELIMITER);
    start -= sLen;
    if (start < 0)
        start = 0;
    data[end] = 0;

    char *pe = data + end - sLen;
    char *ps = data + start;
    char *pp = NULL;

    for (; ps <= pe; ++ps)
    {
        bool bMatch = true;
        for (int cmdReply = 0; cmdReply < sLen && bMatch; ++cmdReply)
        {
            bMatch = (ps[cmdReply] == VI_DELIMITER[cmdReply]);
        }
        if (bMatch)
        {
            pp = ps;
            break;
        }
    }

    if (pp == NULL)
        return -1;

    return pp - data;
}
/*
 * 回复指令
 */
void Reply(int replytype, int cmdNo, int sockfd)
{
    if (sockfd == -2)
    {
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (clients[i].fd == -1)
            {
                continue;
            }
            memcpy(gSendbuf, VI_DELIMITER, strlen(VI_DELIMITER));	//add delimiter at end
            int len = ReplyEncode(replytype, cmdNo, (char*) gSendbuf + strlen(VI_DELIMITER));
            if (len > 0)
            {
                memcpy(gSendbuf + len + strlen(VI_DELIMITER), VI_DELIMITER, strlen(VI_DELIMITER));
                //add delimiter at end
                int sendlen = send(clients[i].fd, (char*) gSendbuf, len + 2 * strlen(VI_DELIMITER), MSG_NOSIGNAL);
                if (sendlen == -1)
                {
                    printf("send reply fail!!! ");
                    writelog("发送回复失败\n");
                }
            }
        }
    }
    else
    {
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (clients[i].fd == -1 || clients[i].fd !=sockfd )
            {
                continue;
            }
            //printf("Reply %d,%d \n", replytype, cmdNo);
            memcpy(gSendbuf, VI_DELIMITER, strlen(VI_DELIMITER));	//add delimiter at end
            int len = ReplyEncode(replytype, cmdNo, (char*) gSendbuf + strlen(VI_DELIMITER));
            if (len > 0)
            {
                memcpy(gSendbuf + len + strlen(VI_DELIMITER), VI_DELIMITER, strlen(VI_DELIMITER));
                //add delimiter at end
                int sendlen = send(clients[i].fd, (char*) gSendbuf, len + 2 * strlen(VI_DELIMITER), MSG_NOSIGNAL);
                if (sendlen == -1)
                {
                    printf("send reply fail!!! ");
                    writelog("发送回复失败\n");
                    //	close_client();
                }
            }
        }
    }
}

/*
 * RT线程不用EPOLL队列，不需要信号
 */
void sendRtStart()
{
    cmdReply = STARTDAQ;
    kfifo_write_pack(&gM2PFifoState, &cmdReply, sizeof(cmdReply));
    pthread_kill(thr_process, sigCmdM2P); //发出给处理线程的信号
    kfifo_write_pack(&gM2RFifoState, &cmdReply, sizeof(cmdReply));
}
void sendRtStop()
{
    cmdReply = STOPDAQ;
    kfifo_write_pack(&gM2RFifoState, &cmdReply, sizeof(cmdReply));
    kfifo_write_pack(&gM2PFifoState, &cmdReply, sizeof(cmdReply));
    pthread_kill(thr_process, sigCmdM2P); //发出给处理线程的信号
}
/*
 * 存储、处理线程用EPOLL队列，需要信号
 */
void sendSaveStart()
{
    cmdReply = STARTSAVE;
    kfifo_write_pack(&gM2SFifoState, &cmdReply, sizeof(cmdReply));
    pthread_kill(thr_save, sigCmdM2S); //发出给存储线程的信号
    kfifo_write_pack(&gM2PFifoState, &cmdReply, sizeof(cmdReply));
    pthread_kill(thr_process, sigCmdM2P); //发出给处理线程的信号
}
void sendSaveStop()
{
    cmdReply = STOPSAVE;
    kfifo_write_pack(&gM2SFifoState, &cmdReply, sizeof(cmdReply));
    pthread_kill(thr_save, sigCmdM2S); //发出给存储线程的信号
    kfifo_write_pack(&gM2PFifoState, &cmdReply, sizeof(cmdReply));
    pthread_kill(thr_process, sigCmdM2P); //发出给处理线程的信号
}
void sendSaveUpdate()
{
    cmdReply = CLEARDATA;
    kfifo_write_pack(&gM2SFifoState, &cmdReply, sizeof(cmdReply));
    pthread_kill(thr_save, sigCmdM2S); //发出给存储线程的信号
}

void sendConnected()
{
    cmdReply = CONNECTED;
    kfifo_write_pack(&gM2PFifoState, &cmdReply, sizeof(cmdReply));
    pthread_kill(thr_process, sigCmdM2P); //发出给处理线程的信号
}
void sendUnconnected()
{
    cmdReply = UNCONNECTED;
    kfifo_write_pack(&gM2PFifoState, &cmdReply, sizeof(cmdReply));
    pthread_kill(thr_process, sigCmdM2P); //发出给处理线程的信号
}
void sendLimit()
{
    cmdReply = OPENLIMIT;
    kfifo_write_pack(&gM2PFifoState, &cmdReply, sizeof(cmdReply));
    pthread_kill(thr_process, sigCmdM2P); //发出给处理线程的信号
}
void sendUnLimit()
{
    cmdReply = CANCELLIMIT;
    kfifo_write_pack(&gM2PFifoState, &cmdReply, sizeof(cmdReply));
    pthread_kill(thr_process, sigCmdM2P); //发出给处理线程的信号
}
void sendUpdate()
{
    UpdateDataMsg();
    cmdReply = UPDATEPARAM;
    kfifo_write_pack(&gM2RFifoState, &cmdReply, sizeof(cmdReply));
    kfifo_write_pack(&gM2PFifoState, &cmdReply, sizeof(cmdReply));
    pthread_kill(thr_process, sigCmdM2P); //发出给处理线程的信号


}

void hostCanRecive(int sockfd)
{
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i].fd ==  sockfd)
        {
            clients[i].brecivedata=true;
        }
    }
}
void OtherhostCanNotRecive(int sockfd)
{
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i].fd == -1 || clients[i].fd ==  sockfd)
        {
            continue;
        }
        clients[i].brecivedata=false;
        Reply(CLOSEHOST,  gRunParam.m_state.runId, clients[i].fd);
    }
}
