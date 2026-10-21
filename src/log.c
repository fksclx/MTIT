/*
 * log日志记录类
 * 单线程使用，多线程不安全
 * 主函数初始化一次即可
 */
#include "log.h"
#include <stdio.h>//文件操作类
#include <sys/types.h>
#include "upper_computer/common.h"
#include "hardware.h"
extern char currentLogFileRoad[256]; //本次上电记录路径
char logroad[256]; //log文件路径
static char cur_time[32]; //时间字符串
static FILE * logfp; //log文件指针

#include <sys/types.h>//读取文件夹
#include <dirent.h>//读取文件夹
/*
 * 初始化log文件
 */
int initLog()
{
	DIR *dir;
    memset(logroad, 0, sizeof(logroad));
    strcpy(logroad, currentLogFileRoad);
	int currentNumber=0;
	int nIndexLog=0;

	//找到根目录如无则创建log文件夹
	if ((dir = opendir(logroad)) == 0)	//无法打开则创建
	{
		printf("open %s failed!\n", logroad);
		mkdir(logroad,0755);
		if (opendir(logroad) == 0)	//无法打开则创建
			return -1;
	}

	//先遍历递增子文件夹
	struct dirent *stdir;
	while ((stdir = readdir(dir)) != NULL)
	{
		if (strcmp(stdir->d_name, ".") == 0 || strcmp(stdir->d_name, "..") == 0)
		{
			continue; // 跳过当前目录和父目录
		}
		if (stdir->d_type != 8)							//判定是否为文件夹则输出
		{
			currentNumber = atoi(stdir->d_name);
			if (currentNumber > nIndexLog)
				nIndexLog = currentNumber;
		}
	}
	closedir(dir);		//关闭目录
	char smaxNumber[100];
	nIndexLog++;		//需要自加1
	sprintf(smaxNumber, "%d", nIndexLog);
	strcat(logroad, smaxNumber);
	strcat(logroad, "/");

	//找到根目录如无则创建log文件夹
	if ((dir = opendir(logroad)) == 0)	//无法打开则创建
	{
		printf("open %s failed!\n", logroad);
		mkdir(logroad,0755);
		if (opendir(logroad) == 0)	//无法打开则创建
			return -1;
	}
	closedir(dir);		//关闭目录
	char fileroad[256];
	memset(fileroad,0,sizeof(fileroad));
    strcat(fileroad, logroad);

    //char cur_time[48];
    //memset(cur_time, 0x00, sizeof(cur_time));
   // GetTimeStamp(cur_time);
   // strcat(fileroad, cur_time);
    strcat(fileroad, "log.txt"); //获取log路径
    if ((logfp = fopen(fileroad, "a")) == NULL) //新建或者添加
    {
        printf("log文件创建失败! %s\n",fileroad);
        return -1;
    }
    writelog("日志初始化成功\n");
    return 0;
}
/*
 * 初始化log文件
 */
static int syncTimes = 0;
void writelog(const char* logdata)
{
    if (logfp == NULL)
    {
        return;
    }
    memset(cur_time, 0x00, sizeof(cur_time));
    GetTimeStampMs(cur_time); //获取当前时间，多线程安全用_r
    fprintf(logfp, "[%s]  %s", cur_time, logdata);
    fflush(logfp);
    syncTimes++;
    if (syncTimes == 2)
    {
        syncTimes = 0;
        int fd = fileno(logfp);
        if (fd != -1)
        {
            fdatasync(fd);
        }
    }
}
/*
 * 关闭log文件
 */
void uinitLog()
{
    if (logfp == NULL)
        return;
    int fd = fileno(logfp);
    if (fd != -1)
    {
        fsync(fd);
    }
    fclose(logfp);
    logfp = NULL;
}
int clearLog()
{
    uinitLog();
    delete_directory(currentLogFileRoad,false);
    return initLog();
}
