#pragma once
#include "stdint.h"
#include <stdio.h>
#include <stdbool.h>
#include "structDefine.h"
#include "Daq_V0.7.pb-c.h"

typedef struct FileHeader
{
	char token[32];     // = "Incremental File Format";             // �ļ���ʽ��ʶ��
	char version[32];   // = "1.0.0.5";                           // �汾��
	int32_t fileType;   // = 0;                                 // ��ǰ�ļ�����
	int32_t size;       // = sizeof(FileHeader);                        // �ļ�ͷ��С
	int32_t address;    // = 0;                                 // ��ַ
	int32_t firstSectionHeaderAddress;  // = 0;                 // ��һ�����ݶε�ַ
} FileHeader;

//-------------------------------------------------------------------------
// ͨ����Ϣͷ
typedef struct ChannelInformationHeader
{
	int16_t channelCount;                               // ��ǰ�ļ��е�����������
	int16_t size;       // = sizeof(ChannelInformationHeader);      //
	int32_t address;    // = sizeof(FileHeader);                    // ���ļ��еĵ�ַ
} ChannelInformationHeader;

//-------------------------------------------------------------------------
typedef struct ChannelInformation
{
	char name[128];                                     // ͨ������
	int16_t index;                                      // ͨ��Index(�Զ��������䣬�����ļ��ڲ�����)
	int64_t timestart;   //真正的起始时间  都一样
	int64_t pointstampBegin;                             // 全程的开始的index
	int64_t pointstampEnd;                               //要改成全程的结束的index
	int64_t sectionCount;                               // section个数
	int64_t pointCount;                                 //总点数 全程累加的
	//    int32_t samplingFrequency;                          // ����Ƶ��
	double samplingFrequency;                          // 采样频率
	char physicType[32];                         // 物理量类型
	char unitX[32];                              // 通道单位
	char unitY[32];                              // 通道单位
	int32_t DataForm;							//0、1表示4字节 2表示double 4表示1字节
	char reserve[1020];                          //

	int32_t firstSectionHeaderAddress;                  // ���ļ��еĵ�ַ
	int32_t lastSectionHeaderAddress;
} ChannelInformation;

//-------------------------------------------------------------------------
// ���ݶ�ͷ
typedef struct SectionDataHeader
{
	int32_t channelIndex;                               // ͨ��Index
	int32_t sectionIndex;                               // ������
	int32_t dominNum;
	int32_t valueCount;                                 // ͨ�����ݵ����
	int64_t valueSize;                                  // ��������������ռ(�ڴ�)�ռ��С
	int64_t pointstamp;                                  // 每个section内第一个点的全程index
	int32_t previousSectionHeaderAddress;               // ��һ�����ļ��е�λ��(16EBѰַ�ռ�)
	int32_t nextSectionHeaderAddress;                   // ��һ�����ļ��е�λ��(16EBѰַ�ռ�)
} SectionDataHeader;

struct SectionAxisXHeader
{
	int64_t timestamp;          // ʱ���
	int32_t address;            // ���ļ��еĵ�ַ(16EBѰַ�ռ�)
};

typedef struct IffFile
{
	FILE* m_filePtr;
	struct FileHeader m_fileHeader;
	struct ChannelInformationHeader m_channelHeader;
	struct ChannelInformation *m_pchaInfo;
	int32_t *m_pLastSecDataHeaderAddr;

	uint32_t m_singleFrameLen;
	uint32_t m_singleFrameCount[MAX_CHANNEL];

	uint32_t m_isSaving;
	uint32_t m_isDiskFull;
	uint32_t m_isInited;
	uint32_t m_isQuit;

	char filename[256];
	uint32_t m_readOffset[MAX_CHANNEL];
	uint32_t m_curOffset;

} IffFile;

void InitIffFile(IffFile *g_iffFile);

void DeinitIffFile(IffFile *g_iffFile);

void IffFileClose(IffFile *g_iffFile);

bool IffFileNewFile(IffFile *g_iffFile);

bool IffFileSaveFrame(char* data, IffFile *g_iffFile);
bool IffFileSaveFile(char* data, int frameLength, int FP_File, IffFile *g_iffFile);
void setIffFileName(char* timestam, IffFile *g_iffFile);

bool IffFileSaveHeader(IffFile *g_iffFile);

bool writeSection(IffFile *pfile, int channelIndex, char *pdatas, int datasize);

char* IffFileName();

int IffFileNewDir(char *fileRoad,char* deviceName,int64_t time,IffFile *g_iffFile);

bool isIFFFileDiskFull(IffFile *g_iffFile);
bool IffFileCheckSize(IffFile *g_iffFile);
void IffFileSaveChannel(int i,char * data,int pointCount,IffFile *g_iffFile,int size);

void UpdateFile(RunningParam* rp, IffFile *g_iffFile);
void updateSaveLastLen(char*fileroad, IffFile *g_iffFile);

