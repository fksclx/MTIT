/*
 * common.c
 *
 *  Created on: 20250503
 *      Author: jw
 */

#include "common.h"
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <string.h>
#include <stdio.h>
#include<stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/statfs.h>
#include <unistd.h>
#include <arpa/inet.h>
/*
 * 返回当前时间的时间戳（微秒级），并将时间格式化为字符串（如果cur_time非NULL）
 * @param cur_time: 用于存储格式化时间字符串的缓冲区，建议长度至少20字节
 * @return 微秒级时间戳
 */
uint64_t GetTimeStampNoSec()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
    // 一天的秒数
    const uint64_t SECONDS_PER_DAY = 86400;

    // 取当天已经过去的秒数
    uint64_t seconds_today = tv.tv_sec % SECONDS_PER_DAY;

    // 当前时间戳减去当天已经过去的秒数，即为当天0点0分0秒对应的秒数
    uint64_t zero_sec = tv.tv_sec - seconds_today;

    // 转成微秒时间戳
    uint64_t ret = zero_sec * 1000000;
	return ret;
}

uint64_t GetTimeStamp(char *cur_time)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	if (NULL != cur_time)
	{
		uint64_t sec = tv.tv_sec;
        // 获取毫秒值
        int millis = tv.tv_usec / 1000;
		struct tm cur_tm; //
		localtime_r((time_t*) &sec, &cur_tm);
		sprintf(cur_time,"%04d-%02d-%02d_%02d_%02d_%02d_%03d", cur_tm.tm_year + 1900, cur_tm.tm_mon + 1, cur_tm.tm_mday, cur_tm.tm_hour, cur_tm.tm_min,
				cur_tm.tm_sec,millis);
	}
	uint64_t ret = (uint64_t) tv.tv_sec * 1000000 + tv.tv_usec;
	return ret;
}
/*
 * 获取当前时间，返回格式化字符串，精确到毫秒
 * @param cur_time: 用于存储格式化时间字符串的缓冲区
 */
void GetTimeStampMs(char *cur_time)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);

	if (NULL != cur_time)
	{
		uint64_t sec = tv.tv_sec;
		// 获取毫秒值
		long millis = tv.tv_usec / 1000;

		struct tm cur_tm; //
		localtime_r((time_t*) &sec, &cur_tm);
		sprintf(cur_time, "%d年%02d月%02d日%02d:%02d:%02d.%03ld", cur_tm.tm_year + 1900, cur_tm.tm_mon + 1, cur_tm.tm_mday, cur_tm.tm_hour, cur_tm.tm_min,
				cur_tm.tm_sec, millis);
	}
}
/*
 * 将毫秒级时间戳转换为格式化时间字符串：yyyy年MM月dd日HH:mm:ss.mmm
 * @param timestamp_ms: 毫秒时间戳
 * @param cur_time: 用于存储格式化时间字符串的缓冲区
 */
void ConvertTimestampToFormat(int64_t timestamp_ms, char *cur_time)
{
    if (cur_time != NULL) {
        // 分解为秒和毫秒
        time_t sec = timestamp_ms / 1000;
        int millis = timestamp_ms % 1000;

        struct tm cur_tm;
        localtime_r(&sec, &cur_tm);  // 注意这里要传time_t*

        // 格式化为 yyyy-MM-dd_hh_mm_ss_zzz
        // 例：2024-06-13_14_30_22_123
        sprintf(cur_time, "%04d-%02d-%02d_%02d_%02d_%02d_%03d",
                 cur_tm.tm_year + 1900,
                 cur_tm.tm_mon + 1,
                 cur_tm.tm_mday,
                 cur_tm.tm_hour,
                 cur_tm.tm_min,
                 cur_tm.tm_sec,
                 millis);
    }
}
/*
 * 返回微妙值
 */

uint64_t GetTimeStampU()
{
	uint64_t t = GetTimeStamp(NULL);
	return t;
}
/*
 * 睡眠指定的毫秒数
 * @param msec: 毫秒数
 */
void sleep_ms(unsigned int msec)
{
	struct timeval tval;
	tval.tv_sec = msec / 1000;
	tval.tv_usec = (msec % 1000)*1000;
	// select等待指定时间实现毫秒级睡眠
	select(0, NULL, NULL, NULL, &tval);
}
/*
 * 获取本机mac
 */
int get_local_mac(const char *eth_inf, char *mac)
{
	struct ifreq ifr;
	int sd;

	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("get %s mac address socket creat error\n", eth_inf);
		return -1;
	}

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, eth_inf, sizeof(ifr.ifr_name) - 1);

	if (ioctl(sd, SIOCGIFHWADDR, &ifr) < 0)
	{
		printf("get %s mac address error\n", eth_inf);
		close(sd);
		return -1;
	}

	sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", (unsigned char) ifr.ifr_hwaddr.sa_data[0], (unsigned char) ifr.ifr_hwaddr.sa_data[1],
			(unsigned char) ifr.ifr_hwaddr.sa_data[2], (unsigned char) ifr.ifr_hwaddr.sa_data[3], (unsigned char) ifr.ifr_hwaddr.sa_data[4],
			(unsigned char) ifr.ifr_hwaddr.sa_data[5]);

	close(sd);
	return 0;
}
/*
 * 获取本机ip
 */
int get_local_ip(const char *eth_inf, char *ip)
{
	int sd;
	struct sockaddr_in sin;
	struct ifreq ifr;

	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == sd)
	{
		perror("socket");
		return -1;
	}

	strncpy(ifr.ifr_name, eth_inf, IFNAMSIZ);
	ifr.ifr_name[IFNAMSIZ - 1] = 0;

	// if error: No such device
	if (ioctl(sd, SIOCGIFADDR, &ifr) < 0)
	{
		perror("ioctl");
		close(sd);
		return -1;
	}

	memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
	sprintf(ip, "%s", inet_ntoa(sin.sin_addr));

	close(sd);
	return 0;
}

int GetMacIpInfo(char* mac, char* ip)
{
	//get all the information of interface
	int sockfd;
	struct ifconf ifconf;
	struct ifreq *ifreq;
	char buf[512];

	ifconf.ifc_len = 512;
	ifconf.ifc_buf = buf;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket");
		exit(1);
	}
	ioctl(sockfd, SIOCGIFCONF, &ifconf);

	ifreq = (struct ifreq*) ifconf.ifc_buf;
	int num_if = ifconf.ifc_len / sizeof(struct ifreq);
	for (int i = 0; i < num_if; i++)
	{
		if (ifreq->ifr_flags == AF_INET)
		{
			char mactemp[64];
			char iptemp[64];
			get_local_mac(ifreq->ifr_name, mactemp);
			get_local_ip(ifreq->ifr_name, iptemp);

			if (strncmp(iptemp, "192", 3) == 0)
			{
				strcpy(mac, mactemp);
				strcpy(ip, iptemp);
				break;
			}

			ifreq++;
		}
	}

	return 0;
}

int64_t FileLength(FILE *fp)
{
	int32_t n;
	fpos_t fpos;
	fgetpos(fp, &fpos);
	fseek(fp, 0, SEEK_END);
	n = ftell(fp);
	fsetpos(fp, &fpos);
	return n;
}

int mkdirs(char *muldir)
{
	int iRetVal = 0;
	char tempStr[512] = { 0 };

	if (!muldir)
	{
		return 0;
	}

	strcpy(tempStr, muldir);
	for (int i = 1; i < strlen(tempStr); i++)
	{
		if (tempStr[i] == '/')
		{
			tempStr[i] = '\0';
			if (access(tempStr, 0) != 0)
			{
				mkdir(tempStr, 0777);
			}
			tempStr[i] = '/';
		}
	}

	if (strlen(tempStr) > 0 && access(tempStr, 0) != 0)
	{
		mkdir(tempStr, 0777);
	}

	return 1;
}

int remove_dir(const char *dir)
{
	char cur_dir[] = ".";
	char up_dir[] = "..";
	char dir_name[256];
	DIR *dirp;
	struct dirent *dp;
	struct stat dir_stat;

	if (0 != access(dir, F_OK))
	{
		return -1;
	}

	if (0 > stat(dir, &dir_stat))
	{
		perror("get directory stat error");
		return -1;
	}

	if (S_ISREG(dir_stat.st_mode))
	{
		remove(dir);
	}
	else if (S_ISDIR(dir_stat.st_mode))
	{
		dirp = opendir(dir);
		while ((dp = readdir(dirp)) != NULL)
		{
			if ((0 == strcmp(cur_dir, dp->d_name)) || (0 == strcmp(up_dir, dp->d_name)))
			{
				continue;
			}

			sprintf(dir_name, "%s/%s", dir, dp->d_name);
			remove_dir(dir_name);
		}
		closedir(dirp);
		rmdir(dir);
	}
	else
	{
		perror("unknow file type!");
	}

	return 0;
}

//获取路径下可用空间 单位字节
void GetDiskInfo(const char *lpdiskpath)
{
    struct statfs diskInfo;
    statfs(lpdiskpath, &diskInfo);
    uint64_t blocksize = diskInfo.f_bsize;                   // 每个block里包含的字节数
    uint64_t totalsize = blocksize * diskInfo.f_blocks; // 总的字节数，f_blocks为block的数目
    uint64_t freeDisk = diskInfo.f_bfree * blocksize;       // 剩余空间的大小
    uint64_t availableDisk = diskInfo.f_bavail * blocksize; // 可用空间大小
    double g_totalsize = totalsize /GB;
    double g_freeSise = freeDisk /MB;
    double g_availabelSize = availableDisk /KB;

    char logdata[100]; //日志
    sprintf(logdata, "磁盘路径:%s 可用空间 = %f MB\n", lpdiskpath, g_freeSise);
    printf(logdata);
    writelog(logdata);
}
//获取路径下可用空间 单位字节
int64_t SKY_GetDiskFreeZone(const char *lpdiskpath)
{
	struct statfs diskInfo;
	statfs(lpdiskpath, &diskInfo);
	uint64_t blocksize = diskInfo.f_bsize;                   // 每个block里包含的字节数
	//	uint64_t totalsize = blocksize * diskInfo.f_blocks; // 总的字节数，f_blocks为block的数目
	//	printf("Total_size = %lld B = %f KB = %f MB = %f GB\n", totalsize, totalsize / KB, totalsize / MB, totalsize / GB);
	//	uint64_t freeDisk = diskInfo.f_bfree * blocksize;       // 剩余空间的大小
	uint64_t availableDisk = diskInfo.f_bavail * blocksize; // 可用空间大小
	//printf("Disk_available = %f MB = %f GB\n", availableDisk / MB, availableDisk / GB);
	return availableDisk;
}

