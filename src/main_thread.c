#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h> //EPOLL
#include <netdb.h> //TCP
#include <netinet/in.h> //TCP
#include <sys/timerfd.h>//timerfd
#include <sys/time.h>//time
#include <dirent.h>
#include "upper_computer/common.h"
#include <unistd.h>
#include <sys/sysinfo.h>
#include "brocast_work.h"//UDP发现线程
#include "main_thread.h"
#include "net_work.h"
#include "save_work.h"
#include "log.h"
#include "threadcreate.h"
#include "process_work.h"
#include "upper_computer/protoHelper.h"

#include "tinyexpr.h"
#include <math.h>
extern struct RunningParam gRunParam;

char *m_m2rFifoStateBuffer; //状态数组
char *m_m2pFifoStateBuffer; //状态数组
char *m_m2sFifoStateBuffer; //状态数组

struct kfifo gM2RFifoState;  //MAIN发送状态RT读取
struct kfifo gM2PFifoState;  //MAIN发送状态PROCESS读取
struct kfifo gM2SFifoState;  //MAIN发送状态SAVE读取

extern struct kfifo gP2MFifoData;   //处理给MAIN数据
extern struct kfifo gS2MFifoState;  //处理发送状态MAIN读取
extern struct kfifo gP2MFifoState;  //处理发送状态MAIN读取
extern struct kfifo gR2MFifoState;  //RT发送状态给MAIN读取
extern struct kfifo gP2MFifoLog;  //处理发送LOG给MAIN读取
extern bool run_untill_shutDown;

extern char autoStartSaveRoad[256]; //本次上电数据记录路径

extern int LOOP_TIME_NEED; //和硬件采样，一次如果采不了125ms的数，就得分多次采集  循环次数
static int P2MDatafd = -1;
static int R2MStatefd = -1;
static int S2MStatefd = -1;
static int P2MStatefd = -1;
static int P2MLogfd = -1;

int mainEpollfd = -1;
static int timerfd = -1;
static struct signalfd_siginfo fdsig;
static struct epoll_event wait_event[10];
static char gDatabuf[FRAME_MAX_SIZE];
static char gStatebuf[STATE_MAX_SIZE];
static char gLogbuf[LOG_DATA_SIZE];
int64_t end0;
int64_t end1;
int64_t end2;
int64_t allTime;

static int initFifo()
{
	int ret = 0;
	m_m2rFifoStateBuffer = (char*) malloc(STATE_FIFO_SIZE);
	if (m_m2rFifoStateBuffer == NULL)
	{
		printf("m_m2rFifoStateBuffer malloc fail\n");
		return -1;
	}
	ret = kfifo_init(&gM2RFifoState, m_m2rFifoStateBuffer, STATE_FIFO_SIZE, 1);
	if (ret != 0)
	{
		printf("gM2RFifoState init fail\n");
		return -1;
	}
	m_m2pFifoStateBuffer = (char*) malloc(STATE_FIFO_SIZE);
	if (m_m2pFifoStateBuffer == NULL)
	{
		printf("m_m2pFifoStateBuffer malloc fail\n");
		return -1;
	}
	ret = kfifo_init(&gM2PFifoState, m_m2pFifoStateBuffer, STATE_FIFO_SIZE, 1);
	if (ret != 0)
	{
		printf("gM2PFifoState init fail\n");
		return -1;
	}
	m_m2sFifoStateBuffer = (char*) malloc(STATE_FIFO_SIZE);
	if (m_m2sFifoStateBuffer == NULL)
	{
		printf("m_m2sFifoStateBuffer malloc fail\n");
		return -1;
	}
	ret = kfifo_init(&gM2SFifoState, m_m2sFifoStateBuffer, STATE_FIFO_SIZE, 1);
	if (ret != 0)
	{
		printf("gM2SFifoState init fail\n");
		return -1;
	}
	return 0;
}
static void uninitFifo()
{
	kfifo_free(&gM2RFifoState);
	kfifo_free(&gM2PFifoState);
	kfifo_free(&gM2SFifoState);
	printf("main fifoBuffer free success\n");
}

/*
 线程循环，需要处理1.存储及tcp转发数据 2.tcp状态变更 3.tcp收到状态机变更
 */
static int initEpoll()
{
	mainEpollfd = epoll_create1(0);	//创建epoll实例
	if (-1 == mainEpollfd)
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
	sigaddset(&sigintMask, sigDataP2M);	//在set指向的信号集中加入处理线程给主线程数据信号
	int ret = sigprocmask(SIG_BLOCK, &sigintMask, NULL);	//结果集是当前集合参数集的并集,把信号集添加进去，避免触发这些信号的默认处理方式
	if (ret == -1)
		printf("sigprocmask failed in init.\n");
	P2MDatafd = signalfd(-1, &sigintMask, 0);	//signalfd可以将信号抽象为一个文件描述符，当有信号发生时可以对其read，这样可以将信号的监听放到select、poll、epoll等监听队列中
	if (P2MDatafd == -1)
		printf("P2MDatafd failed in init.\n");

	struct epoll_event sigintEvent;
	sigintEvent.data.fd = P2MDatafd;
	sigintEvent.events = EPOLLIN;
	epoll_ctl(mainEpollfd, EPOLL_CTL_ADD, P2MDatafd, &sigintEvent);
	return 0;
}
//初始化信号SIGUSR2
static int initPipeSignal2()
{
	//使得epoll监听到SIGUSR1
	sigset_t sigintMask;	//将信号加入sigset_t
	sigemptyset(&sigintMask);	//初始化，清空信号集里所有信号
	sigaddset(&sigintMask, sigStateR2M);	//在set指向的信号集中加入RT线程给主线程状态信号
	int ret = sigprocmask(SIG_BLOCK, &sigintMask, NULL);	//结果集是当前集合参数集的并集,把信号集添加进去，避免触发这些信号的默认处理方式
	if (ret == -1)
		printf("sigprocmask failed in init.\n");
	R2MStatefd = signalfd(-1, &sigintMask, 0);	//signalfd可以将信号抽象为一个文件描述符，当有信号发生时可以对其read，这样可以将信号的监听放到select、poll、epoll等监听队列中
	if (R2MStatefd == -1)
		printf("R2MStatefd failed in init.\n");

	struct epoll_event sigintEvent;
	sigintEvent.data.fd = R2MStatefd;
	sigintEvent.events = EPOLLIN;
	epoll_ctl(mainEpollfd, EPOLL_CTL_ADD, R2MStatefd, &sigintEvent);
	return 0;
}
//初始化信号SIGUSR3
static int initPipeSignal3()
{
	//使得epoll监听到SIGUSR3
	sigset_t sigintMask;	//将信号加入sigset_t
	sigemptyset(&sigintMask);	//初始化，清空信号集里所有信号
	sigaddset(&sigintMask, sigStateS2M);	//在set指向的信号集中加入处理线程给主线程状态信号
	int ret = sigprocmask(SIG_BLOCK, &sigintMask, NULL);	//结果集是当前集合参数集的并集,把信号集添加进去，避免触发这些信号的默认处理方式
	if (ret == -1)
		printf("sigprocmask failed in init.\n");
	S2MStatefd = signalfd(-1, &sigintMask, 0);	//signalfd可以将信号抽象为一个文件描述符，当有信号发生时可以对其read，这样可以将信号的监听放到select、poll、epoll等监听队列中
	if (S2MStatefd == -1)
		printf("S2MStatefd failed in init.\n");

	struct epoll_event sigintEvent;
	sigintEvent.data.fd = S2MStatefd;
	sigintEvent.events = EPOLLIN;
	epoll_ctl(mainEpollfd, EPOLL_CTL_ADD, S2MStatefd, &sigintEvent);
	return 0;
}
//初始化信号SIGUSR4
static int initPipeSignal4()
{
	//使得epoll监听到SIGUSR3
	sigset_t sigintMask;	//将信号加入sigset_t
	sigemptyset(&sigintMask);	//初始化，清空信号集里所有信号
	sigaddset(&sigintMask, sigStateP2M);	//在set指向的信号集中加入处理线程给主线程状态信号
	int ret = sigprocmask(SIG_BLOCK, &sigintMask, NULL);	//结果集是当前集合参数集的并集,把信号集添加进去，避免触发这些信号的默认处理方式
	if (ret == -1)
		printf("sigprocmask failed in init.\n");
	P2MStatefd = signalfd(-1, &sigintMask, 0);	//signalfd可以将信号抽象为一个文件描述符，当有信号发生时可以对其read，这样可以将信号的监听放到select、poll、epoll等监听队列中
	if (P2MStatefd == -1)
		printf("sigStateP2M failed in init.\n");

	struct epoll_event sigintEvent;
	sigintEvent.data.fd = P2MStatefd;
	sigintEvent.events = EPOLLIN;
	epoll_ctl(mainEpollfd, EPOLL_CTL_ADD, P2MStatefd, &sigintEvent);
	return 0;
}
//初始化信号SIGUSR5
static int initPipeSignal5()
{
	//使得epoll监听到SIGUSR3
	sigset_t sigintMask;	//将信号加入sigset_t
	sigemptyset(&sigintMask);	//初始化，清空信号集里所有信号
	sigaddset(&sigintMask, sigLogP2M);	//在set指向的信号集中加入处理线程给主线程状态信号
	int ret = sigprocmask(SIG_BLOCK, &sigintMask, NULL);	//结果集是当前集合参数集的并集,把信号集添加进去，避免触发这些信号的默认处理方式
	if (ret == -1)
		printf("sigprocmask failed in init.\n");
	P2MLogfd = signalfd(-1, &sigintMask, 0);	//signalfd可以将信号抽象为一个文件描述符，当有信号发生时可以对其read，这样可以将信号的监听放到select、poll、epoll等监听队列中
	if (P2MLogfd == -1)
		printf("P2MDatafd failed in init.\n");

	struct epoll_event sigintEvent;
	sigintEvent.data.fd = P2MLogfd;
	sigintEvent.events = EPOLLIN;
	epoll_ctl(mainEpollfd, EPOLL_CTL_ADD, P2MLogfd, &sigintEvent);
	return 0;
}
static int initTimerfd()
{
    timerfd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd == -1)
    {
        perror("timerfd_create");
        return -1;
    }
    //添加到epoll里面
    struct epoll_event sigintEvent;
    sigintEvent.data.fd = timerfd;
    sigintEvent.events = EPOLLIN;
    epoll_ctl(mainEpollfd, EPOLL_CTL_ADD, timerfd, &sigintEvent);
    return 0;
}

int main_init()
{

	//	申请获取DMA数值的BUF与DMA数列转换BUF

	if (initEpoll() != 0)
	{
		printf("initEpoll fail\n");
		return -1;
	}
	//初始化TCP
	if (init_tcp() != 0)
	{
		printf("initTCP fail\n");
		return -1;
	}
	//初始化信号1
	if (initPipeSignal1() != 0)
	{
		printf("initPipeSignal1 fail\n");
		return -1;
	}
	//初始化信号2
	if (initPipeSignal2() != 0)
	{
		printf("initPipeSignal2 fail\n");
		return -1;
	}
	//初始化信号3
	if (initPipeSignal3() != 0)
	{
		printf("initPipeSignal3 fail\n");
		return -1;
	}
	//初始化信号4
	if (initPipeSignal4() != 0)
	{
		printf("initPipeSignal4 fail\n");
		return -1;
	}
	//初始化信号5
	if (initPipeSignal5() != 0)
	{
		printf("initPipeSignal5 fail\n");
		return -1;
	}
    if (initTimerfd() != 0)
    {
        printf("initTimerfd fail\n");
        return -1;
    }
	return 0;
}
/*
 * 设置本机时间
 * 传入毫秒数
 */
int setLocaltime(int64_t sec)
{
	if (gRunParam.m_state.isSampling)
	{
		writelog("当前处于采集、存储或计划执行中，请先关闭再修改时间\n");
		return -1;
	}
	struct tm t_localtm;
	struct tm t_utctm;
	struct timeval t_timeval;
	time_t t_timet;

	t_timeval.tv_sec = sec / 1000;
	t_timeval.tv_usec = (sec - t_timeval.tv_sec * 1000 + 2) * 1000;						//多弥补2ms延迟

	int rec = settimeofday(&t_timeval, NULL);
	if (rec != 0)
	{
		writelog("设置时间同步错误\n");
		return rec;
	}
	writelog("修改时间成功\n");
	t_timet = time(NULL);
	gmtime_r(&t_timet, &t_utctm);
	char buf[256]; // 足够存储格式化时间字符串
	strftime(buf, sizeof(buf), "修改后utc时间  %a %b %d %H:%M:%S %Y\n", &t_utctm);
	printf(buf);
	writelog(buf);

	t_timet = time(NULL);
	localtime_r(&t_timet, &t_localtm);

	strftime(buf, sizeof(buf), "修改后本地时间  %a %b %d %H:%M:%S %Y\n", &t_localtm);
	printf(buf);
	writelog(buf);

	return 0;
}
/*
 * 设置本机时间
 * 传入秒数
 */
static int setTimertime(int64_t sec)
{
    if (timerfd == -1)
    {
        printf("no timerfd!!\n");
        return -1;
    }
    struct itimerspec new_value;
    struct timespec ts;
    ts.tv_sec = 10;
    ts.tv_nsec = 0;
    new_value.it_value = ts; //首次计时
    struct timespec ts1;
    ts1.tv_sec = sec;
    ts1.tv_nsec = 0;
    new_value.it_interval = ts1; //间隔计时

    if (timerfd_settime(timerfd, 1, &new_value, NULL) == -1)
    {
        perror("timerfd_settime fail");
        return -1;
    }
    printf("timer started\n");
    return 0;
}

static void stopTimer()
{
    if (timerfd == -1)
        return;

    struct itimerspec new_value = {};
    new_value.it_value.tv_sec = 0;
    new_value.it_value.tv_nsec = 0;

    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;

    if (timerfd_settime(timerfd, 0, &new_value, NULL) == -1)
    {
        printf("timerfd_settime fail\n");
    }
    printf("stop timer!\n");

}
static void recvTimerSignal()
{
    uint64_t exp = 0;
    int ret = read(timerfd, &exp, sizeof(uint64_t));
    if (ret != sizeof(uint64_t))
    {
        perror("read timerfd fail");
    }
    if(getDevicePowerState()) //无任务时4min一次
        Reply(HIGHTEMP, gRunParam.m_state.runId,-2);

}
static void recvProcessDataSignal()
{
	//P2MDatafd 信号
	int s = read(P2MDatafd, (void*) &fdsig, sizeof(struct signalfd_siginfo));
	if (s != sizeof(fdsig))
	{
		printf("read failed.\n");
		return;
	}
	int data_length = kfifo_read_pack(&gP2MFifoData, &gDatabuf, DATA_FIFO_SIZE);	//收到信号，从fifo中读取到数组
	if (data_length <= 0)
	{
		printf("read fifo failed.\n");
		return;
	}
	else
	{
		send_data(&gDatabuf);
	}

}
static void recvRtStateSignal()
{
	int s = read(R2MStatefd, (void*) &fdsig, sizeof(struct signalfd_siginfo));
	if (s != sizeof(fdsig))
	{
		printf("read failed.\n");
		return;
	}
	int data_length = kfifo_read_pack(&gR2MFifoState, &gStatebuf, STATE_FIFO_SIZE);	//收到信号，从fifo中读取到数组
	if (data_length <= 0)
		return;
	if (data_length != sizeof(int)) //收到的是rt指令,需要转发
	{
		sprintf(gLogbuf, "收到rt状态信号，但无法解析，长度为%d\n", data_length);
		writelog(gLogbuf);
		printf(gLogbuf);
		return;
	}
	int i = 0;
	memcpy(&i, &gStatebuf, data_length);
	if (i == DAQSTARTED)
	{
		gRunParam.m_state.isSampling = true;
		Reply(DAQSTARTED, gRunParam.m_state.runId,-2);
		writelog("设备开始采样\n");

	}
	else if (i == DAQSTOPED)
	{
		writelog("设备停止采样\n");
		Reply(DAQSTOPED, gRunParam.m_state.runId,-2);
		gRunParam.m_state.isSampling = false;
		gRunParam.m_state.isCountZero = false;
	}
	else if (i == DAQPIPE_FULL) //需要发送给上位机异常rt开始采集或停止
	{
		writelog("设备采样线程往process fifo写入时爆满\n");
		gRunParam.m_state.isSampling = false;
		//报异常
		Reply(DAQPIPE_FULL, gRunParam.m_state.runId,-2);
	}
	else if (i == DAQSAMPLEFALSE) //需要发送给上位机异常rt开始采集或停止
	{
		writelog("单次采样失败，超时\n");
		//报异常
		Reply(DAQSAMPLEFALSE, gRunParam.m_state.runId,-2);
	}
	else if (i == DEVICEDAQSTOPED) //需要发送给上位机异常rt开始采集或停止
	{
		writelog("轮询触发信号超时导致采集停止\n");
		//报异常
		Reply(DEVICEDAQSTOPED,gRunParam.m_state.runId,-2);
		Reply(DAQSTOPED, gRunParam.m_state.runId,-2); //用于上位机刷新按钮
		gRunParam.m_state.isSampling = false;

	}
	else if(UPDATESUCCESS ==i)
	{
		Reply(UPDATESUCCESS, gRunParam.m_state.runId,-2);
		writelog("更新参数成功\n");
	}
	else if(UPDATEFAIL ==i)
	{
		Reply(UPDATEFAIL, gRunParam.m_state.runId,-2);
		writelog("更新参数失败\n");
	}
	else
	{
		printf("rt线程发出未知信号 %d\n", i);
		writelog("rt线程发出未知信号 \n");
	}
}

static void recvSaveStateSignal()
{
	int s = read(S2MStatefd, (void*) &fdsig, sizeof(struct signalfd_siginfo));
	if (s != sizeof(fdsig))
	{
		printf("read failed.\n");
		return;
	}
	char saveStatebuf[sizeof(DiskSpace)];
    int data_length = kfifo_read_pack(&gS2MFifoState, saveStatebuf, sizeof(saveStatebuf));	//收到信号，从fifo中读取到数组
	if (data_length <= 0)
		return;
	if (data_length == sizeof(DiskSpace))
    {
        memcpy(&gRunParam.space, saveStatebuf, sizeof(gRunParam.space));
        return;
    }
    if (data_length != sizeof(int)) //收到的是rt指令,需要转发
	{
		sprintf(gLogbuf, "收到save状态信号，但无法解析，长度为%d\n", data_length);
		writelog(gLogbuf);
		printf(gLogbuf);
		return;
	}
	int i = 0;
	memcpy(&i, saveStatebuf, data_length);
	if (i == SAVESTARTED)
	{
		gRunParam.m_state.isSaving = true;
		writelog("设备开始存储\n");
		Reply(SAVESTARTED, gRunParam.m_state.runId,-2);
	}
	else if (i == SAVESTOPED)
	{
		gRunParam.m_state.isSaving = false;
		writelog("设备停止存储\n");
		Reply(SAVESTOPED,gRunParam.m_state.runId,-2);
	}
	else if (i == DISKFULL)				//需要发送给上位机异常rt开始采集或停止
	{
		writelog("设备存储线写入时磁盘已满\n");
		//报异常
		Reply(DISKFULL, gRunParam.m_state.runId,-2);
		Reply(SAVESTOPED, gRunParam.m_state.runId,-2);
	}

	else
	{
		writelog("save线程发出未知信号\n");
	}
}
static void recvProcessStateSignal()
{
	int s = read(P2MStatefd, (void*) &fdsig, sizeof(struct signalfd_siginfo));
	if (s != sizeof(fdsig))
	{
		printf("read failed.\n");
		return;
	}
	int data_length = kfifo_read_pack(&gP2MFifoState, &gStatebuf, STATE_FIFO_SIZE);	//收到信号，从fifo中读取到数组
	if (data_length <= 0)
		return;
	if (data_length == sizeof(int)) //收到的是rt指令,需要转发
	{
		int i = 0;
		memcpy(&i, &gStatebuf, data_length);
		if (i == UPDATEBCOFF)
		{
			writeJsonFileChannel();
			Reply(DAWRITESUCCESS,  gRunParam.m_state.runId,-2);
			writelog("已经更新调零参数！\n");
		}
		else if (i == ZEROADJUSTRESULT)
		{
			writeJsonFileChannel();
			Reply(ZEROADJUSTRESULT,  gRunParam.m_state.runId,-2);
			writelog("已经更新调零参数！\n");
		}
		else if(i==SAVEPIPE_FULL)
		{
			printf("存储线程爆满!\n");
			writelog("存储线程爆满!\n");
			Reply(SAVEPIPE_FULL,  gRunParam.m_state.runId,-2);
		}
	}
	else
	{
		sprintf(gLogbuf, "收到process状态信号，但无法解析，长度为%d\n", data_length);
		writelog(gLogbuf);
		printf(gLogbuf);
		return;
	}

}
static void recvProcessLogSignal()
{
	int s = read(P2MLogfd, (void*) &fdsig, sizeof(struct signalfd_siginfo));
	if (s != sizeof(fdsig))
	{
		printf("read failed.\n");
		return;
	}
	int data_length = kfifo_read_pack(&gP2MFifoLog, &gLogbuf, LOG_FIFO_SIZE);	//收到信号，从fifo中读取到数组
	if (data_length <= 0)
		return;

	writelog(gLogbuf);
	printf(gLogbuf);
}
extern struct JsonDeviceParam jParam;
void updateDeviceChannels()
{
    //先不考虑换卡的情况
    //直接超已存在的板卡，不存在的赋予默认值
    //更新最终KB
    for (int i = 0; i < gRunParam.m_chnldefs.m_nAnalogChnls; i++)
    {
        ChnlParams*lchannel= gRunParam.m_chnldefs.m_AnalogChnlParam[i];
        if(i<jParam.m_chnldefs.m_nAnalogChnls)
        {
            ChnlParams*rchannel= jParam.m_chnldefs.m_AnalogChnlParam[i];
            lchannel->bCoff=rchannel->bCoff;
            lchannel->kCoff=rchannel->kCoff;
            lchannel->bCalib=rchannel->bCalib;
            lchannel->kCalib=rchannel->kCalib;
            lchannel->fSensitivity=rchannel->fSensitivity;
        }
        else
        {
            lchannel->bCoff=0;
            lchannel->kCoff=1;
            lchannel->bCalib=0;
            lchannel->kCalib=1;
        }
        lchannel->bFinally=(lchannel->bCalib*lchannel->kCoff+lchannel->bCoff)/lchannel->fSensitivity;
        lchannel->kFinally=(lchannel->kCoff*lchannel->kCalib)/lchannel->fSensitivity;
    }
    for (int i = 0; i < gRunParam.m_chnldefs.m_nDigitalChnls; i++)
    {
        ChnlParams*lchannel= gRunParam.m_chnldefs.m_DigitalChnlParam[i];
        if(i<jParam.m_chnldefs.m_nDigitalChnls)
        {
            ChnlParams*rchannel= jParam.m_chnldefs.m_DigitalChnlParam[i];
            lchannel->bCoff=rchannel->bCoff;
            lchannel->kCoff=rchannel->kCoff;
            lchannel->bCalib=rchannel->bCalib;
            lchannel->kCalib=rchannel->kCalib;
            lchannel->fSensitivity=rchannel->fSensitivity;
        }
        else
        {
            lchannel->bCoff=0;
            lchannel->kCoff=1;
            lchannel->bCalib=0;
            lchannel->kCalib=1;
        }
        lchannel->bFinally=(lchannel->bCalib*lchannel->kCoff+lchannel->bCoff)/lchannel->fSensitivity;
        lchannel->kFinally=(lchannel->kCoff*lchannel->kCalib)/lchannel->fSensitivity;
    }
    for (int i = 0; i < gRunParam.m_chnldefs.m_nIOChnls; i++)
    {
        ChnlParams*lchannel= gRunParam.m_chnldefs.m_IOChnlParam[i];
        if(i<jParam.m_chnldefs.m_nIOChnls)
        {
            ChnlParams*rchannel= jParam.m_chnldefs.m_IOChnlParam[i];
            lchannel->bCoff=rchannel->bCoff;
            lchannel->kCoff=rchannel->kCoff;
            lchannel->bCalib=rchannel->bCalib;
            lchannel->kCalib=rchannel->kCalib;
            lchannel->fSensitivity=rchannel->fSensitivity;
        }
        else
        {
            lchannel->bCoff=0;
            lchannel->kCoff=1;
            lchannel->bCalib=0;
            lchannel->kCalib=1;
        }
        lchannel->bFinally=(lchannel->bCalib*lchannel->kCoff+lchannel->bCoff)/lchannel->fSensitivity;
        lchannel->kFinally=(lchannel->kCoff*lchannel->kCalib)/lchannel->fSensitivity;
    }
}
void main_threadrun()
{
    if (main_init() != 0) //主线程需要创建epoll队列，监听rt状态信号、save状态信号、tcp信号、timer信号
    {
        printf("main_init fail\n");
        writelog("主线程初始化失败，可能因为TCP端口被占用!\n");
        exit(0);
    }
	if (initFifo() != 0)
	{
		return;
	}
	//给数据结构体清空
	InitDataMsg();
	//读取真正的板卡和通道数量
	readDeviceCards();
	readDeviceChannels();
	//合并通道
	updateDeviceChannels();
	//读取本地文件的板卡和通道数量
	//读取上位机下发板卡和通道数量

	//addVirtualCards();
   // readDeviceChannels();

//    const char *c = "5*5";
//    int error = 0;
//    double result = te_interp(c, &error);
//    if (error) {
//        printf("解析错误，错误代码：%d\n", error);
//    } else {
//        printf("计算结果：%f\n", result);
//    }


	//必须先初始化本地状态
//	if (readJsonFileChannel() != 0)
//	{
//		printf("readJsonFileChannel fail\n");
//		return;
//	}

	printf("main_thread ready\n");
	thread_wait_start();	//主线程开启
	printf("main_thread go\n");
	setTimertime(60);//一分钟一次
	main1();


	while (run_untill_shutDown)
	{
		int waitnum = epoll_wait(mainEpollfd, wait_event, 10 + 1, 2000);
		for (int i = 0; i < waitnum; i++)
		{
			if (wait_event[i].events & EPOLLERR)
			{
				end0 = GetTimeStampU();
				writelog("收到epoll error消息 !\n");
				do_error(&wait_event[i]);
				allTime = GetTimeStampU() - end0;
				printf(" error speed :%lld us  \n", allTime);
			}
			else if (wait_event[i].events & EPOLLIN)
			{
				//P2MDatafd 信号  rt数据信号
				if (P2MDatafd == wait_event[i].data.fd)
				{
					recvProcessDataSignal();
					continue;
				}
				//R2MStatefd 信号 rt状态信号
				else if (R2MStatefd == wait_event[i].data.fd)
				{
					recvRtStateSignal();
					continue;
				}
				//P2MStatefd 信号 save状态信号
				else if (S2MStatefd == wait_event[i].data.fd)
				{
					recvSaveStateSignal();
					continue;
				}
				//P2MStatefd 信号 save状态信号
				else if (P2MStatefd == wait_event[i].data.fd)
				{
					recvProcessStateSignal();
					continue;
				}
				//P2MLogfd 信号 save状态信号
				else if (P2MLogfd == wait_event[i].data.fd)
				{
					recvProcessLogSignal();
					continue;
				}
                //TIMERfd 信号 PLAN状态信号
                else if (timerfd == wait_event[i].data.fd)
                {
                    recvTimerSignal();
                    continue;
                }
				else
				{
					do_recieve(&wait_event[i]);
				}
			}
			else
			{
				sprintf(gLogbuf, "收到未知消息%d\n", wait_event[i].events);
				writelog(gLogbuf);
				int s = read(wait_event[i].data.fd, (void*) &fdsig, sizeof(struct signalfd_siginfo));
				if (s != sizeof(fdsig))
				{
					writelog("read epoll failed.\n");
					continue;
				}
			}
		}

	}
	stopTimer();
	uninitFifo();
}


