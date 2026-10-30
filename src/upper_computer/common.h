/*
 * common.h
 *
 *  Created on: 20231221
 *      Author: JW
 */

#pragma once
#include <stdint.h>
#include <stdio.h>
#include <sys/signalfd.h> //信号
#include <signal.h>
#include "structDefine.h"

#define DATA_FIFO_SIZE 67108864//开辟FIFO大小 64MB  134217728
#define STATE_FIFO_SIZE 64 //开辟FIFO大小
#define LOG_FIFO_SIZE 1048576 //开辟FIFO大小
#define LOG_DATA_SIZE 1024
#define FRAME_MAX_SIZE 52428800  //50m 2097152大小用于固定内存单次FIFO发送存储帧长度
#define STATE_MAX_SIZE 4  //4b大小用于固定内存单次FIFO发送存状态长度
#define DISK_MIN_LEN (128ULL * 1024 * 1024) //磁盘剩余空间

// MIN - MAX 中间有30个可用的
#define sigDataR2P SIGRTMIN+9
#define sigDataP2M SIGRTMIN+10
#define sigDataP2S SIGRTMIN+11
#define sigStateR2M SIGRTMIN+12
#define sigStateP2M SIGRTMIN+13
#define sigStateS2M SIGRTMIN+14
#define sigCmdM2P SIGRTMIN+16
#define sigCmdM2S SIGRTMIN+17
#define sigLogP2M SIGRTMIN+18
#define sigLogR2M SIGRTMIN+20
#define sigCmdP2S SIGRTMIN+21
uint64_t GetTimeStampU();
uint64_t GetTimeStampNoSec();
uint64_t GetTimeStamp(char *cur_time);
void GetTimeStampMs(char *cur_time);
void ConvertTimestampToFormat(int64_t timestamp_ms, char *cur_time);
void sleep_ms(unsigned int secs);

//int get_mac(char* mac);

//char* get_local_ip();

int get_local_mac(const char *eth_inf, char *mac);

int get_local_ip(const char *eth_inf, char *ip);

int GetMacIpInfo(char* mac, char* ip);

int64_t FileLength(FILE *fp);
int mkdirs(char *muldir);
int remove_dir(const char *dir);
int SKY_GetDiskFreeZone(const char *lpdiskpath, DiskSpace *space); //获取路径下可用空间 单位字节


