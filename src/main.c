#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <netdb.h> //TCP
#include <netinet/in.h> //TCP
#include "upper_computer/common.h"
#include "upper_computer/protoHelper.h"
#include <sys/sysinfo.h>

#include "rt_work.h" //RT采样线程
#include "process_work.h" //存储采样线程
#include "brocast_work.h"//UDP发现线程
#include "save_work.h"
#include "main_thread.h"
#include "net_work.h"
#include "log.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statfs.h>
#include <dirent.h>//读取文件夹
#include "threadcreate.h"//创建线程
#include <unistd.h> //包含cpu信息
#include <sys/resource.h>  //拿进程

#define rootRoad "/home/admin/mnt/"

#define MASTER_THREAD_PRIO 50  //主线程优先级
#define RT_THREAD_PRIO 99    //RT采集线程优先级
#define PROCESS_THREAD_PRIO 90    //RT采集线程优先级
#define UDPSEND_THREAD_PRIO 80  //发现线程优先级
#define SAVE_THREAD_PRIO 85  //发现线程优先级
#define BROCAST_THREAD_PRIO 29  //发现线程优先级

int nIndexTest; //当前试验索引
char currentRootRoad[256]; //软件根目录
char normalSaveRoad[256]; //本次上电数据记录路径
char iffDeviceName[256];
char currentLogFileRoad[256]; //本次上电日志记录路径

pthread_t thr_rt, thr_process, thr_save, thr_main, thr_bd;  //rt线程、处理线程、存储线程、主线程、发现线程、UDP发固存线程、UDP接收线程

FactoryInfo factoryInfo; //出厂信息

struct RunningParam gRunParam; //定义全局工作状态
bool run_untill_shutDown = true; //全部线程控制

/*
 * 退出函数
 */
static void fun_sig(int sig)
{
    run_untill_shutDown = false; //第一遍先停止所有线程
    printf("run_untill_shutDown \n");
    signal(sig, SIG_DFL); //第二遍再叫停
}

/*
 * 创建上电自增文件
 */
static int buildCurrentTestFile()
{
    DIR *dir;
    memset(currentRootRoad, 0, sizeof(currentRootRoad));
    strcpy(currentRootRoad, rootRoad);
    strcat(currentRootRoad, factoryInfo.m_productName);
    strcat(currentRootRoad, "/");
    //找到根目录如无则创建默认文件夹
    if ((dir = opendir(currentRootRoad)) == 0)	//无法打开则创建
    {
        printf("open %s failed!\n", currentRootRoad);
        mkdir(currentRootRoad,0755);
        if ((dir = opendir(currentRootRoad)) == 0)	//无法打开则创建
            return -1;
    }
    closedir(dir);

    memset(normalSaveRoad, 0, sizeof(normalSaveRoad));
    strcpy(normalSaveRoad, currentRootRoad);
    strcat(normalSaveRoad, "DATA/");
    //找到根目录如无则创建DATA文件夹
    if ( opendir(normalSaveRoad) == 0)	//无法打开则创建
    {
        printf("open %s failed!\n", normalSaveRoad);
        mkdir(normalSaveRoad,0755);
        if ( opendir(normalSaveRoad) == 0)	//无法打开则创建
            return -1;
    }
    closedir(dir);


    memset(currentLogFileRoad, 0, sizeof(currentLogFileRoad));
    strcpy(currentLogFileRoad, currentRootRoad);
    strcat(currentLogFileRoad, "LOG/");
    //找到根目录如无则创建log文件夹
    if ((dir = opendir(currentLogFileRoad)) == 0)	//无法打开则创建
    {
        printf("open %s failed!\n", currentLogFileRoad);
        mkdir(currentLogFileRoad,0755);
        if ((dir = opendir(currentLogFileRoad)) == 0)	//无法打开则创建
            return -1;
    }
    printf("%s \n",currentLogFileRoad);
    closedir(dir);		//关闭目录
    return 0;
}
static void setSysTime()
{
    //设置时区拿到正确时间
    setenv("TZ", "CST-8", 1); //设置东8区北京时间
    tzset(); //设置时区
}
static void setHighProperty()
{
    pid_t pid = getpid();
    // 设置较高的优先级
    if (setpriority(PRIO_PROCESS, pid, -10) == -1)
    {
        perror("setpriority");
    }
    // 检查优先级是否成功设置
    int priority = getpriority(PRIO_PROCESS, pid);
    if (priority == -1)
    {
        perror("getpriority");
    }
}
static void initRunParam()
{

    //参数初始化
    gRunParam.m_state.isSampling = false;
    gRunParam.m_state.isSaving = false;
    gRunParam.m_state.isConnect = false;
    gRunParam.m_state.lock_state = false;
    gRunParam.m_state.test_state=0;
}
/*
 * 初始化函数
 */
int init()
{
    //设置系统时间时区
    setSysTime();
    //设置当前进程优先级
    setHighProperty();
    //读取配置文件 初始化设备信息
    readJsonFileFactoryInfo();
    readJsonFileCard();
    readJsonFileCalib();
    readJsonFileChannel();
    //创建本次上电自加1文件文件夹
    if (buildCurrentTestFile() != 0)
    {
        //关键点位必须成功
        printf("初始化上电自加1文件失败!\n");
        return -1;
    }
    //初始化日志
    initLog();
    //获取磁盘空间
    GetDiskInfo(currentRootRoad);
    //参数初始化
    initRunParam();
    return 0;
}
int uinit()
{
    printf("uninit\n");
    deinit_tcp();
    thread_join(thr_rt);
    thread_join(thr_process);
    thread_join(thr_save);
    thread_join(thr_bd);
    uinitLog();
    thread_wait_destroy();	//释放
    printf("uninitfinish!\n");
    return 0;
}

int main(void)
{
    if (init() != 0)	//初始化必须成功
        exit(0);
    signal(SIGINT, fun_sig); //绑定退出信号

    int defualtThreadCount = 5; //rt线程、存储、处理和主线程、发现线程
    //所有调用pthread_barrier_wait的线程才能继续往后运行

    thread_wait_init(defualtThreadCount);

    //获取主线程
    thr_main = pthread_self();
    //设置主线程优先级
    thread_set_priority(thr_main, MASTER_THREAD_PRIO);

    thr_rt = thread_create(rt_run, &thr_rt, RT_THREAD_PRIO, 2);
    if (thr_rt == 0)
    {
        printf("rt_thread create fail\n");
        writelog("采集线程创建失败!\n");
        exit(0);	//rt创建失败必须退出了
    }
    thr_save = thread_create(save_run, &thr_save, SAVE_THREAD_PRIO, 2);
    if (thr_save == 0)
    {
        printf("save_thread create fail\n");
        writelog("存储线程创建失败!\n");
        exit(0);	//process创建失败必须退出了
    }

    thr_process = thread_create(process_run, &thr_process, PROCESS_THREAD_PRIO, 2);
    if (thr_process == 0)
    {
        printf("process_thread create fail\n");
        writelog("处理线程创建失败!\n");
        exit(0);	//process创建失败必须退出了
    }
    thr_bd = thread_create(brocast_run, &thr_bd, BROCAST_THREAD_PRIO, 0);
    if (thr_bd == 0)
    {
        printf("bd_thread create fail\n");
        writelog("发现线程创建失败!\n");
        exit(0);	//bd创建失败必须退出了
    }

    main_threadrun();	//主线程主函数启动
    uinit();	//退出程序

    return 0;
}
