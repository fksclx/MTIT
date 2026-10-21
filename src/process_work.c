/*
 * PROCESS_WORK_H_.c
 *
 *  Created on: 20230607
 *      Author: jw
 */

#include <stdlib.h> //提供了标准库函数，例如内存分配、随机数生成等。 malloc 和 free 等函数
#include <stdio.h> //提供了标准输入输出函数，例如打印、读取文件等。
#include <stdbool.h> //定义了bool类型以及true和false常量。
#include <string.h> //提供了字符串处理函数，例如字符串拷贝、比较等。
#include <sys/epoll.h> //定义了使用`epoll`进行事件驱动编程的函数和数据类型。
#include <fcntl.h>      // 用于文件打开常量，如 O_RDWR
#include <sys/mman.h>   // 用于 mmap 函数
#include <sys/types.h>  // 一些数据类型定义
#include <unistd.h>     // 用于 close 函数（如果需要关闭文件描述符）
#include <aio.h>
#include <stdint.h>
#include "upper_computer/common.h" //全局变量
#include "kfifo.h" //kfifo调用
#include "log.h"//log写入
#include "process_work.h" //自己的头文件
#include "upper_computer/protoHelper.h"

extern pthread_t thr_main, thr_save;  //主线程

extern struct kfifo gM2PFifoState;  //主线程给处理线程发的指令
extern struct kfifo gR2PFifoData;  //从RT收数据

struct kfifo gP2MFifoState;  //PROCESS发送状态MAIN读取
struct kfifo gP2MFifoData;	//PROCESS发送数据MAIN读取
struct kfifo gP2SFifoData;	//PROCESS发送数据SAVE读取
struct kfifo gP2MFifoLog;	//PROCESS发送日志MAIN读取

char *m_p2mFifoStateBuffer; //状态数组
char *m_p2mFifoDataBuffer; //数据数组
char *m_p2sFifoDataBuffer; //数据数组
char *m_p2mFifoLogBuffer; //日志数组
static char datalog[LOG_DATA_SIZE];

static int processEpollfd;  //处理线程epoll队列
static int R2PDatafd = -1;  //RT拉起信号
static int M2PCMDfd = -1;  //主线程信号

static struct signalfd_siginfo fdsig;
static struct epoll_event wait_event[10];
static char gRawDatabuf[FRAME_MAX_SIZE]; //接收数据
static char gStatebuf[STATE_MAX_SIZE]; //接受状态
static char *dataPtr[MAX_CHANNEL]; //多通道指针，指向处理后通道

extern int LOOP_TIME_NEED; //和硬件采样，一次如果采不了125ms的数，就得分多次采集  循环次数

extern struct RunningParam gRunParam; //通道参数
extern bool run_untill_shutDown; //总体停止

TempratureType tempratureTypes;
static int16_t tempratureMap[MAX_CHANNEL]; //存放温度通道指向索引
static int16_t tempratureIndex[MAX_CHANNEL]; //存放温度通道上一次点位

static int64_t begin;
static int64_t end;
static bool isDAQ=false;
static bool isSave = false;
static bool isDelaySaveStart = true;
static bool isSend = false;


static bool currentLimit = false;
static bool channelLimit[MAX_CHANNEL];

extern char limitSaveRoad[256]; //本次上电数据记录路径
extern struct kfifo gProcess2SerialData; //serial发送

/*
 线程循环，需要处理1.存储及tcp转发数据 2.tcp状态变更 3.tcp收到状态机变更
 */

static int initEpoll()
{
    processEpollfd = epoll_create1(0);	//创建epoll实例
    if (-1 == processEpollfd)
    {
        perror("epoll_create");
        return -1;
    }
    return 0;
}

//初始化信号SIGUSR1
static int initPipeSignal1()
{
    //使得epoll监听到SIGUSR1
    sigset_t sigintMask;	//将信号加入sigset_t
    sigemptyset(&sigintMask);	//初始化，清空信号集里所有信号
    sigaddset(&sigintMask, sigDataR2P);	//在set指向的信号集中加入处理线程给主线程数据信号
    int ret = sigprocmask(SIG_BLOCK, &sigintMask, NULL);	//结果集是当前集合参数集的并集,把信号集添加进去，避免触发这些信号的默认处理方式
    if (ret == -1)
        printf("sigprocmask failed in init.\n");
    R2PDatafd = signalfd(-1, &sigintMask, 0);	//signalfd可以将信号抽象为一个文件描述符，当有信号发生时可以对其read，这样可以将信号的监听放到select、poll、epoll等监听队列中
    if (R2PDatafd == -1)
        printf("P2MDatafd failed in init.\n");

    struct epoll_event sigintEvent;
    sigintEvent.data.fd = R2PDatafd;
    sigintEvent.events = EPOLLIN;
    epoll_ctl(processEpollfd, EPOLL_CTL_ADD, R2PDatafd, &sigintEvent);
    return 0;
}
//初始化信号SIGUSR2
static int initPipeSignal2()
{
    //使得epoll监听到SIGUSR1
    sigset_t sigintMask;	//将信号加入sigset_t
    sigemptyset(&sigintMask);	//初始化，清空信号集里所有信号
    sigaddset(&sigintMask, sigCmdM2P);	//在set指向的信号集中加入RT线程给主线程状态信号
    int ret = sigprocmask(SIG_BLOCK, &sigintMask, NULL);	//结果集是当前集合参数集的并集,把信号集添加进去，避免触发这些信号的默认处理方式
    if (ret == -1)
        printf("sigprocmask failed in init.\n");
    M2PCMDfd = signalfd(-1, &sigintMask, 0);	//signalfd可以将信号抽象为一个文件描述符，当有信号发生时可以对其read，这样可以将信号的监听放到select、poll、epoll等监听队列中
    if (M2PCMDfd == -1)
        printf("P2MDatafd failed in init.\n");

    struct epoll_event sigintEvent;
    sigintEvent.data.fd = M2PCMDfd;
    sigintEvent.events = EPOLLIN;
    epoll_ctl(processEpollfd, EPOLL_CTL_ADD, M2PCMDfd, &sigintEvent);
    return 0;
}

static int initFifo()
{
    int ret = 0;
    m_p2mFifoStateBuffer = (char*) malloc(STATE_FIFO_SIZE);
    if (m_p2mFifoStateBuffer == NULL)
    {
        printf("m_p2mFifoStateBuffer malloc fail\n");
        return -1;
    }
    ret = kfifo_init(&gP2MFifoState, m_p2mFifoStateBuffer, STATE_FIFO_SIZE, 1);
    if (ret != 0)
    {
        printf("gP2MFifoState init fail\n");
        return -1;
    }
    m_p2mFifoDataBuffer = (char*) malloc(DATA_FIFO_SIZE);
    if (m_p2mFifoDataBuffer == NULL)
    {
        printf("m_p2mFifoDataBuffer malloc fail\n");
        return -1;
    }
    ret = kfifo_init(&gP2MFifoData, m_p2mFifoDataBuffer, DATA_FIFO_SIZE, 1);
    if (ret != 0)
    {
        printf("gP2MFifoData init fail\n");
        return -1;
    }
    m_p2sFifoDataBuffer = (char*) malloc(DATA_FIFO_SIZE);
    if (m_p2sFifoDataBuffer == NULL)
    {
        printf("m_p2sFifoDataBuffer malloc fail\n");
        return -1;
    }
    ret = kfifo_init(&gP2SFifoData, m_p2sFifoDataBuffer, DATA_FIFO_SIZE, 1);
    if (ret != 0)
    {
        printf("gP2SFifoData init fail\n");
        return -1;
    }

    m_p2mFifoLogBuffer = (char*) malloc(LOG_FIFO_SIZE);
    if (m_p2mFifoLogBuffer == NULL)
    {
        printf("m_p2mFifoLogBuffer malloc fail\n");
        return -1;
    }
    ret = kfifo_init(&gP2MFifoLog, m_p2mFifoLogBuffer, LOG_FIFO_SIZE, 1);
    if (ret != 0)
    {
        printf("gP2MFifoLog init fail\n");
        return -1;
    }
    return 0;
}
/*
 * 释放两个fifo
 */
static void uninitFifo()
{
    kfifo_free(&gP2MFifoState);
    kfifo_free(&gP2MFifoData);
    kfifo_free(&gP2SFifoData);
    kfifo_free(&gP2MFifoLog);
    printf("process fifoBuffer free success\n");
    return;
}
//初始化队列及2信号
int initProcess()
{
    //初始化线程有关FIFO
    if (initFifo() != 0)
    {
        return -1;
    }
    //初始化队列
    if (initEpoll() != 0)
    {
        printf("initProcessEpoll fail\n");
        return -1;
    }
    //初始化信号1
    if (initPipeSignal1() != 0)
    {
        printf("initProcessSignal1 fail\n");
        return -1;
    }
    //初始化信号2
    if (initPipeSignal2() != 0)
    {
        printf("initProcessSignal2 fail\n");
        return -1;
    }
    return 0;
}
/*
 *
 */
void* process_run(void *arg)
{
    if (initProcess() != 0)
    {
        return NULL;
    }
    printf("process_thread ready\n");
    thread_wait_start();	//主线程开启
    printf("process_thread go\n");
    while (run_untill_shutDown)
    {
        int waitnum = epoll_wait(processEpollfd, wait_event, 10, 3000);
        for (int i = 0; i < waitnum; i++)
        {
            if (wait_event[i].events & EPOLLIN)
            {
                //RT线程发来的数据信号
                if (R2PDatafd == wait_event[i].data.fd)
                {
                    recvRtDataSignal();
                    continue;
                }
                //主线程发来的命令信号
                else if (M2PCMDfd == wait_event[i].data.fd)
                {
                    recvMainCMDSignal();
                    continue;
                }
            }
        }
    }
    uninitFifo();
    return NULL;
}

static int data_length;
//用于计算拷贝到数组的位置，不是采集帧协议的位置
static void channelPtrMove()
{
	int frameSize = 0;
	for (int i = 0; i < gRunParam.m_chnldefs.m_nChnls; i++)
	{
		dataPtr[i] = &gRawDatabuf[MAX_CHANNEL * 4 + MAX_CHANNEL * 8 + frameSize]; //2*4 = 8字节时间 +64*2 =136字节 单通道点数
		frameSize += gRunParam.m_chnldefs.m_chnlParam[i]->nFrameSize;
	}
}

static int frameCount[MAX_CHANNEL];
static int64_t frameStart[MAX_CHANNEL];
void recvRtDataSignal()
{
    int s = read(R2PDatafd, (void*) &fdsig, sizeof(struct signalfd_siginfo));
    if (s != sizeof(fdsig))
    {
        printf("read failed.\n");
        return;
    }
    begin = GetTimeStampU();
    data_length = kfifo_read_pack(&gR2PFifoData, &gRawDatabuf, DATA_FIFO_SIZE);	//收到信号，从fifo中读取到数组
    if (data_length <= 0)
    {
        printf("read fifo failed.\n");
        return;
    }
	if(!isDAQ)
		return;
	memcpy(&frameCount[0], &gRawDatabuf[0], MAX_CHANNEL * 4);


	if (isSave)
	{
		if(!isDelaySaveStart)
		{
			memcpy(&frameStart[0], &gRawDatabuf[ MAX_CHANNEL * 4], MAX_CHANNEL *8);
			int64_t times = frameStart[0]/frameCount[0];
			int64_t needTimes = gRunParam.nSaveTimeDelay/LOOP_TIME_NEED;//
			printf("%lld %lld %lld\n",frameStart[0],times,needTimes);
			if(times >= needTimes)
				isDelaySaveStart=true;
		}
		else
		{
			//存储
			if (kfifo_write_pack(&gP2SFifoData, &gRawDatabuf, data_length) == PIPE_FULL)
			{
				printf("SAVEPIPE_FULL fail!!!!!!!!!!!!\n");
				int i = SAVEPIPE_FULL;
				kfifo_write_pack(&gP2MFifoState, &i, sizeof(i));
				pthread_kill(thr_main, sigStateP2M); //发出给主线程的信号
			}
			else
			{
				pthread_kill(thr_save, sigDataP2S); //发出给主线程的信号，数据发+9，状态发+10
			}
		}
	}
	if (isSend)
	{
		//转发
		if (kfifo_write_pack(&gP2MFifoData, &gRawDatabuf, data_length) == PIPE_FULL)
		{
			printf("MAINPIPE_FULL fail!!!!!!!!!!!!\n");
			int i = MAINPIPE_FULL;
			kfifo_write_pack(&gP2MFifoState, &i, sizeof(i));
			pthread_kill(thr_main, sigStateP2M); //发出给主线程的信号
		}
		else
		{
			pthread_kill(thr_main, sigDataP2M); //发出给主线程的信号，数据发+9，状态发+10
		}
	}
	end = GetTimeStampU() - begin;
	if (end > LOOP_TIME_NEED * 1000 / 2)
		printf(" process speed more than RT/2: %lld us fifo length:%d \n", end, gR2PFifoData.in - gR2PFifoData.out);
}
float getVoltageByTemperature(float temp, int channelIndex)
{
	if (tempratureMap[channelIndex] == -1)
	{
		return -1; // 无效通道，返回错误值
	}
	TempratureParams *params = tempratureTypes.m_params[tempratureMap[channelIndex]];
	int maxCount = params->m_nTempratureDatas;
	int index=0;
	// 先检查当前索引是否合适

	while (index < maxCount - 1)
	{
		TempratureVoltage* tv = params->m_tempratureData[index];
		tv = params->m_tempratureData[index];
		if (temp >= tv->Temprature_begin && temp <= tv->Temprature_end)
		{
			return (temp - tv->b) / tv->k;
		}
		index++;
	}
	return 0;
}
void countTemprature(float *data, int channelIndex, int frameSize)
{
	if (tempratureMap[channelIndex] == -1)
	{
		return;
	}
	TempratureParams *params = tempratureTypes.m_params[tempratureMap[channelIndex]];
	int maxCount = params->m_nTempratureDatas;
	for (int i = 0; i < frameSize; i++)
	{
		float tempVoltage = data[i];
		if (tempVoltage >= params->m_tempratureData[tempratureIndex[channelIndex]]->Voltage_begin
				&& tempVoltage <= params->m_tempratureData[tempratureIndex[channelIndex]]->Voltage_end)
		{
			data[i] = tempVoltage * params->m_tempratureData[tempratureIndex[channelIndex]]->k + params->m_tempratureData[tempratureIndex[channelIndex]]->b;
		}
		else if (tempVoltage < params->m_tempratureData[tempratureIndex[channelIndex]]->Voltage_begin)
		{
			tempratureIndex[channelIndex]--;
			while (tempVoltage < params->m_tempratureData[tempratureIndex[channelIndex]]->Voltage_begin && tempratureIndex[channelIndex] > 0)
			{
				tempratureIndex[channelIndex]--;
			}
			data[i] = tempVoltage * params->m_tempratureData[tempratureIndex[channelIndex]]->k + params->m_tempratureData[tempratureIndex[channelIndex]]->b;

		}
		else if (tempVoltage > params->m_tempratureData[tempratureIndex[channelIndex]]->Voltage_end)
		{
			tempratureIndex[channelIndex]++;
			while (tempVoltage > params->m_tempratureData[tempratureIndex[channelIndex]]->Voltage_end && tempratureIndex[channelIndex] < maxCount)
			{
				tempratureIndex[channelIndex]++;
			}
			data[i] = tempVoltage * params->m_tempratureData[tempratureIndex[channelIndex]]->k + params->m_tempratureData[tempratureIndex[channelIndex]]->b;
		}
	}
}
static void recvMainCMDSignal()
{
	int s = read(M2PCMDfd, (void*) &fdsig, sizeof(struct signalfd_siginfo));
	if (s != sizeof(fdsig))
	{
		printf("read failed.\n");
		return;
	}
	data_length = kfifo_read_pack(&gM2PFifoState, &gStatebuf, STATE_FIFO_SIZE);	//收到信号，从fifo中读取到数组
	if (data_length <= 0)
		return;
	if (data_length != sizeof(int)) //收到的是rt指令,需要转发
	{
		sprintf(datalog, "收到main发存储状态信号，但无法解析，长度为%d\n", data_length);
		printf(datalog);
		return;
	}
	int i = 0;
	memcpy(&i, &gStatebuf, data_length);
	if (i == STOPSAVE)
	{
		isSave = false;
		isDelaySaveStart=true;
	}
	else if (i == STARTSAVE)
	{
		isSave = true;
		isDelaySaveStart=true;
	}
	else if (i == CONNECTED)
	{
		isSend = true;
	}
	else if (i == UNCONNECTED)
	{
		isSend = false;
	}
	else if (i == STARTDAQ)
	{
		for (int i = 0; i < gRunParam.m_chnldefs.m_nAnalogChnls; i++)
		{
			gRunParam.m_chnldefs.m_AnalogChnlParam[i]->bZero = false;
		}
		isDAQ=true;
	}
	else if (i == STOPDAQ)
	{
		isDAQ=false;
	}
	else if (i == UPDATEPARAM)
	{
		channelPtrMove();
		printf("UPDATEPARAM \n");
	}
	else
	{
		printf("未知信号main给process CMD\n");
	}
}

void sendDataSave(int size)
{
	//	//存储
	//	if (kfifo_write_pack(&gP2LSFifoData, &gTempDatabuf, size) == PIPE_FULL)
	//	{
	//		printf("SAVE FAST PIPE_FULL fail!!!!!!!!!!!!\n");
	//	}
	//	else
	//	{
	//		pthread_kill(thr_lsave, sigP2LS); //发出给主线程的信号，数据发+9，状态发+10
	//	}
}
void sendStartSave()
{
		pthread_kill(thr_save, sigCmdP2S); //发出给udp的信号
}
void sendStopSave()
{
	//	int i = STOPSAVE;
	//	kfifo_write_pack(&gP2LSFifoData, &i, sizeof(i));
	//	pthread_kill(thr_lsave, sigP2LS); //发出给udp的信号
}
void sendLimitTrigger()
{
		int i =LIMITTRIGGERED;
		kfifo_write_pack(&gP2MFifoState, &i, sizeof(i));
		pthread_kill(thr_main, sigStateP2M); //发出给主线程的信号
}
void sendLimitStop()
{
	//	int i =LIMITCLOSED;
	//	kfifo_write_pack(&gP2MFifoState, &i, sizeof(i));
	//	pthread_kill(thr_main, sigStateP2M); //发出给主线程的信号
}
