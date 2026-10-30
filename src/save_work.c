/*
 * save_work.c
 *
 *  Created on: 20230607
 *      Author: jw
 */

#include "save_work.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/epoll.h> //EPOLL
#include <sys/types.h>//读取文件夹
#include <dirent.h>//读取文件夹
#include "upper_computer/file_reader.h"
#include "upper_computer/common.h"
#include "upper_computer/protoHelper.h"
#include "kfifo.h"
#include "log.h"
#include "threadcreate.h"
#include <unistd.h>
#include <fcntl.h>

extern char normalSaveRoad[256];
extern char dirname[100];
extern char iffDeviceName[256];
static int nIndexData;
extern pthread_t thr_main;  //主线程
extern struct kfifo gM2SFifoState, gP2SFifoData;
extern struct RunningParam gRunParam;
extern bool run_untill_shutDown;

struct kfifo gS2MFifoState;  //SAVE发送状态MAIN读取
static char *m_s2mFifoStateBuffer = NULL; //状态数组
static IffFile g_IffFile; //iff全局

static int saveEpollfd;
static int P2SDatafd = -1;
static int M2SCMDfd = -1;
static int P2SCMDfd = -1;

static struct signalfd_siginfo fdsig;
static struct epoll_event wait_event[10];
static char gDatabuf[FRAME_MAX_SIZE];
static char gStatebuf[STATE_MAX_SIZE];
static bool isSaving = false;

static char* dataPtr[MAX_CHANNEL];
static int framePoints[MAX_CHANNEL];


extern int LOOP_TIME_NEED; //ms值，单次凑齐读取100毫秒数据

/* Publish a snapshot; only the main thread updates gRunParam.space. */
static void notifyDiskSpace()
{
    if (updateSaveLastLen(isSaving ? dirname : normalSaveRoad, &g_IffFile) != 0)
    {
        perror("query save disk space");
        return;
    }
    /* Reserve room for start/stop/error status packets. */
    if ((gS2MFifoState.mask + 1 - (gS2MFifoState.in - gS2MFifoState.out)) < sizeof(DiskSpace) + sizeof(unsigned int) + 3 * (sizeof(int) + sizeof(unsigned int)))
        return;
    if (kfifo_write_pack(&gS2MFifoState, &g_IffFile.space,
                         sizeof(g_IffFile.space)) == sizeof(g_IffFile.space))
        pthread_kill(thr_main, sigStateS2M);
}

static int initEpoll()
{
    saveEpollfd = epoll_create1(0);	//创建epoll实例
    if (-1 == saveEpollfd)
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
    sigaddset(&sigintMask, sigDataP2S);	//在set指向的信号集中加入处理线程给主线程数据信号
    int ret = sigprocmask(SIG_BLOCK, &sigintMask, NULL);	//结果集是当前集合参数集的并集,把信号集添加进去，避免触发这些信号的默认处理方式
    if (ret == -1)
        printf("sigprocmask failed in init.\n");
    P2SDatafd = signalfd(-1, &sigintMask, 0);	//signalfd可以将信号抽象为一个文件描述符，当有信号发生时可以对其read，这样可以将信号的监听放到select、poll、epoll等监听队列中
    if (P2SDatafd == -1)
        printf("P2MDatafd failed in init.\n");

    struct epoll_event sigintEvent;
    sigintEvent.data.fd = P2SDatafd;
    sigintEvent.events = EPOLLIN;
    epoll_ctl(saveEpollfd, EPOLL_CTL_ADD, P2SDatafd, &sigintEvent);
    return 0;
}
//初始化信号SIGUSR2
static int initPipeSignal2()
{
    //使得epoll监听到SIGUSR1
    sigset_t sigintMask;	//将信号加入sigset_t
    sigemptyset(&sigintMask);	//初始化，清空信号集里所有信号
    sigaddset(&sigintMask, sigCmdM2S);	//在set指向的信号集中加入RT线程给主线程状态信号
    int ret = sigprocmask(SIG_BLOCK, &sigintMask, NULL);	//结果集是当前集合参数集的并集,把信号集添加进去，避免触发这些信号的默认处理方式
    if (ret == -1)
        printf("sigprocmask failed in init.\n");
    M2SCMDfd = signalfd(-1, &sigintMask, 0);	//signalfd可以将信号抽象为一个文件描述符，当有信号发生时可以对其read，这样可以将信号的监听放到select、poll、epoll等监听队列中
    if (M2SCMDfd == -1)
        printf("P2MDatafd failed in init.\n");

    struct epoll_event sigintEvent;
    sigintEvent.data.fd = M2SCMDfd;
    sigintEvent.events = EPOLLIN;
    epoll_ctl(saveEpollfd, EPOLL_CTL_ADD, M2SCMDfd, &sigintEvent);
    return 0;
}
//初始化信号SIGUSR3
static int initPipeSignal3()
{
    //使得epoll监听到SIGUSR3
    sigset_t sigintMask;	//将信号加入sigset_t
    sigemptyset(&sigintMask);	//初始化，清空信号集里所有信号
    sigaddset(&sigintMask, sigCmdP2S);	//在set指向的信号集中加入RT线程给主线程状态信号
    int ret = sigprocmask(SIG_BLOCK, &sigintMask, NULL);	//结果集是当前集合参数集的并集,把信号集添加进去，避免触发这些信号的默认处理方式
    if (ret == -1)
        printf("sigprocmask failed in init.\n");
    P2SCMDfd = signalfd(-1, &sigintMask, 0);	//signalfd可以将信号抽象为一个文件描述符，当有信号发生时可以对其read，这样可以将信号的监听放到select、poll、epoll等监听队列中
    if (P2SCMDfd == -1)
        printf("P2SCMDfd failed in init.\n");

    struct epoll_event sigintEvent;
    sigintEvent.data.fd = P2SCMDfd;
    sigintEvent.events = EPOLLIN;
    epoll_ctl(saveEpollfd, EPOLL_CTL_ADD, P2SCMDfd, &sigintEvent);
    return 0;
}


static int initFifo()
{
    int ret = 0;
    m_s2mFifoStateBuffer = (char*) malloc(STATE_FIFO_SIZE);
    if (m_s2mFifoStateBuffer == NULL)
    {
        printf("m_p2mFifoStateBuffer malloc fail\n");
        return -1;
    }
    ret = kfifo_init(&gS2MFifoState, m_s2mFifoStateBuffer, STATE_FIFO_SIZE, 1);
    if (ret != 0)
    {
        printf("gS2MFifoState init fail\n");
        return -1;
    }
    return 0;
}
static void uninitFifo()
{
    kfifo_free(&gS2MFifoState);
    printf("save fifoBuffer free success\n");
}
static int initSave()
{
    if (initFifo() != 0)
    {
        return -1;
    }
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
    //初始化信号3
    if (initPipeSignal3() != 0)
    {
        printf("initProcessSignal3 fail\n");
        return -1;
    }
    return 0;
}
/*
 * 存储异常
 */
static void SaveError()
{
    printf("磁盘空间已满，无法开始存储\n"); //写日志
    int i = DISKFULL;
    kfifo_write_pack(&gS2MFifoState, &i, sizeof(i));
    pthread_kill(thr_main, sigStateS2M); //发出给主线程的信号
}
/*
 * 开始存储
 */
static int SaveStart()
{
    isSaving = false;
    InitIffFile(&g_IffFile); //先初始化
    UpdateFile(&gRunParam, &g_IffFile); //根据通道开辟空间
    int i = IffFileNewDir(gRunParam.strSaveName, iffDeviceName,gRunParam.nStartSaveTime, &g_IffFile); //新时间存储文件夹
    if (i == 0)
    {
        isSaving = true;
        notifyDiskSpace();
        int i = SAVESTARTED;
        kfifo_write_pack(&gS2MFifoState, &i, sizeof(i));
        pthread_kill(thr_main, sigStateS2M); //发出给主线程的信号
    }
    else
    {
        SaveError();
        SaveStop();
        return -1;
    }
    return i;
}

static int isCurrentSavingPath(const char *path)
{
    if (!isSaving)
    {
        return 0;
    }

    int len = strlen(path);

    if (strncmp(gRunParam.strSaveName, path, len) != 0)
    {
        return 0;
    }

    if (gRunParam.strSaveName[len] == '\0' || gRunParam.strSaveName[len] == '/')
    {
        return 1;
    }

    return 0;
}

int clearSaveData()
{
    char fileroad[256];
    DIR *dir;
    struct dirent *stdir;
    int ret = 0;



    if ((dir = opendir(normalSaveRoad)) == 0)
    {
        printf("open %s failed!\n", normalSaveRoad);
        ret = -1;
        return;
    }

    while ((stdir = readdir(dir)) != NULL)
    {
        if (strcmp(stdir->d_name, ".") == 0 || strcmp(stdir->d_name, "..") == 0)
        {
            continue;
        }

        memset(fileroad, 0, sizeof(fileroad));
        sprintf(fileroad, "%s%s", normalSaveRoad, stdir->d_name);

        if (isCurrentSavingPath(fileroad))
        {
            continue;
        }

        if (stdir->d_type != 8)
        {
            printf("%s\n", fileroad);
            delete_directory(fileroad, true);
        }
    }

    closedir(dir);


    return ret;
}
/*
 * 结束存储
 */
static void SaveStop()
{
    gRunParam.nSaveTimeDelay=0;
    DeinitIffFile(&g_IffFile); //每次都反初始化释放
    syncCMD();
    notifyDiskSpace();
    isSaving = false;

    int b = SAVESTOPED;
    kfifo_write_pack(&gS2MFifoState, &b, sizeof(b));
    pthread_kill(thr_main, sigStateS2M); //发出给主线程的信号
    printf("save stop close \n");
}
//用于计算拷贝到数组的位置，不是采集帧协议的位置
static void channelPtrMove()
{

    int frameSize = 0;
    //模拟板卡
    for (int i = 0; i < gRunParam.m_chnldefs.m_nChnls; i++)
    {
        dataPtr[i] = &gDatabuf[MAX_CHANNEL * 4 + MAX_CHANNEL * 8 + frameSize]; //2*4 = 8字节时间 +64*2 =136字节 单通道点数
        frameSize += gRunParam.m_chnldefs.m_chnlParam[i]->nFrameSize;
    }
}
/*
 * 存储
 */
static void saveFrame()
{
    if (isSaving)
    {
        memcpy(&framePoints[0], &gDatabuf[0], MAX_CHANNEL * 4);
        int size=0;
        size=4;
        for (int i = 0; i < gRunParam.m_chnldefs.m_nChnls; i++)
        {
            if (framePoints[i] <= 0)
            {
                continue;
            }

            if( gRunParam.m_chnldefs.m_chnlParam[i]->bStoreEnable!=1)
            {
                continue;
            }
            char *data = dataPtr[i];
            IffFileSaveChannel(i,data, framePoints[i], &g_IffFile,size);
        }
        bool fileCompleted = false;
        if (!IffFileCheckSize(&g_IffFile, &fileCompleted))
        {
            SaveError();
            SaveStop();
            return;
        }
        if (fileCompleted)
            notifyDiskSpace();
    }
    return;
}

static int s;
void* save_run(void *arg)
{
    if (initSave() != 0)
    {
        return NULL;
    }
    printf("save_thread ready\n");
    thread_wait_start();	//主线程开启
    printf("save_thread go\n");
    notifyDiskSpace();
    while (run_untill_shutDown)
    {
        int waitnum = epoll_wait(saveEpollfd, wait_event, 10, 3000);
        for (int i = 0; i < waitnum; i++)
        {
            if (wait_event[i].events & EPOLLIN)
            {
                //savefd1 信号 数据信号
                if (P2SDatafd == wait_event[i].data.fd)
                {
                    recvSaveDataSignal();
                    continue;
                }
                //savefd2 状态 信号
                else if (M2SCMDfd == wait_event[i].data.fd)
                {
                    recvMainCMDSignal();
                    continue;
                }
                //savefd2 状态 信号
                else if (P2SCMDfd == wait_event[i].data.fd)
                {
                    s = read(P2SCMDfd, (void*) &fdsig, sizeof(struct signalfd_siginfo));
                    if (s != sizeof(fdsig))
                    {
                        printf("read failed.\n");
                        continue;
                    }
                    if (!isSaving)
                    {
                        channelPtrMove();
                        SaveStart();
                    }
                }
            }
        }

    }
    uninitFifo();
    return NULL;
}
static void recvMainCMDSignal()
{
    s = read(M2SCMDfd, (void*) &fdsig, sizeof(struct signalfd_siginfo));
    if (s != sizeof(fdsig))
    {
        printf("read failed.\n");
        return;
    }
    int data_length = kfifo_read_pack(&gM2SFifoState, &gStatebuf, sizeof(gStatebuf));	//收到信号，从fifo中读取到数组
    if (data_length <= 0)
        return;
    if (data_length != sizeof(int)) //收到的是rt指令,需要转发
    {
        char temp[256];
        sprintf(temp, "收到main发存储状态信号，但无法解析，长度为%d\n", data_length);
        printf(temp);
        return;
    }
    int i = 0;
    memcpy(&i, &gStatebuf, data_length);
    if (i == STARTSAVE)
    {
        if (!isSaving)
        {
            channelPtrMove();
            SaveStart();
        }
        else
        {
            int b = SAVESTARTED;
            kfifo_write_pack(&gS2MFifoState, &b, sizeof(b));
            pthread_kill(thr_main, sigStateS2M); //发出给主线程的信号
        }
    }
    else if (i == STOPSAVE)
    {
        if (isSaving)
        {
            SaveStop();
        }
        else
        {
            int b = SAVESTOPED;
            kfifo_write_pack(&gS2MFifoState, &b, sizeof(b));
            pthread_kill(thr_main, sigStateS2M); //发出给主线程的信号
        }
    }
    else if (i == CLEARDATA)
    {
        notifyDiskSpace();
    }
    else if (i == DISKSPACE)    //查询磁盘空间，更新剩余空间
    {
        notifyDiskSpace();
    }

}
static int64_t begin;
static int64_t end;
static void recvSaveDataSignal()
{
    s = read(P2SDatafd, (void*) &fdsig, sizeof(struct signalfd_siginfo));
    if (s != sizeof(fdsig))
    {
        printf("read failed.\n");
        return;
    }
    begin = GetTimeStampU();
    int data_length = kfifo_read_pack(&gP2SFifoData, &gDatabuf, DATA_FIFO_SIZE);	//收到信号，从fifo中读取到数组
    if (data_length <= 0)
    {
        printf("read fifo failed.\n");
        return;
    }
    saveFrame();
    end = GetTimeStampU() - begin;
    if (end > LOOP_TIME_NEED * 1100)
        printf(" save speed more than 1.1RT ms: %lld us fifo length:%d \n", end, gP2SFifoData.in - gP2SFifoData.out);

}

