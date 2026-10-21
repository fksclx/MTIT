#include "file_reader.h"

#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>

#include  "common.h"
char dirname[100];
uint64_t curfilesavelen = 0; //本次文件已用字节数
uint64_t testfilesavelen = 0; //单次试验存储大小
int64_t testfilesaveLastlen = 0; //可用的总存储空间
uint64_t FILE_MAX_LEN = 512 * 1024 * 1024;
uint64_t DISK_MIN_LEN = 128 * 1024 * 1024; //磁盘剩余空间
extern int LOOP_TIME_NEED; //和硬件采样，一次如果采不了125ms的数，就得分多次采集  循环次数

/*
 初始化iff文件，计算磁盘剩余空间，后续不再新拿
 */
void InitIffFile(IffFile *g_iffFile)
{
    g_iffFile->m_filePtr = NULL;
    g_iffFile->m_pchaInfo = NULL;
    g_iffFile->m_pLastSecDataHeaderAddr = NULL;
}
/*
 析构iff文件，关闭，后续不再新拿
 */
void DeinitIffFile(IffFile *g_iffFile)
{
    IffFileClose(g_iffFile);
    if (g_iffFile->m_pchaInfo != NULL)
    {
        free(g_iffFile->m_pchaInfo);
        g_iffFile->m_pchaInfo = NULL;
    }
    if (g_iffFile->m_pLastSecDataHeaderAddr != NULL)
    {
        free(g_iffFile->m_pLastSecDataHeaderAddr);
        g_iffFile->m_pLastSecDataHeaderAddr = NULL;
    }
}
/*
 *关闭iff文件，只需要更新文件头
 */
void IffFileClose(IffFile *g_iffFile)
{
    if (NULL != g_iffFile->m_filePtr)
    {
        IffFileSaveHeader(g_iffFile);
        int fd = fileno(g_iffFile->m_filePtr);
        if (fd != -1)
        {
            ftruncate(fd, g_iffFile->m_curOffset);
            fsync(fd);
        }
        fclose(g_iffFile->m_filePtr);
        g_iffFile->m_filePtr = NULL;
    }
}
/*
 新iff文件，需要在此之前update channel，m_isSaving=1
 */
bool IffFileNewFile(IffFile *g_iffFile)
{
    IffFileClose(g_iffFile);

    char filename[150];
    char cur_time[30];
    memset(cur_time, 0x00, sizeof(cur_time));
    memset(filename, 0x00, sizeof(filename));

    strcat(filename, dirname);
    GetTimeStamp(cur_time);
    strcat(filename, cur_time);
    strcat(filename, ".iff");

    if (NULL == (g_iffFile->m_filePtr = fopen(filename, "w+")))
    {
        printf("Fail to open IFF file in IffFileNewFile!\n");
        return false;
    }

    int fd = fileno(g_iffFile->m_filePtr);
    if (fd != -1)
        ftruncate(fd, FILE_MAX_LEN);
    //写通道头
    for (int i = 0; i < g_iffFile->m_channelHeader.channelCount; ++i)
    {
        //jw040102
        g_iffFile->m_pchaInfo[i].pointstampBegin = g_iffFile->m_pchaInfo[i].pointstampBegin + g_iffFile->m_pchaInfo[i].pointCount;
        g_iffFile->m_pchaInfo[i].pointCount = 0;
        //
        g_iffFile->m_pchaInfo[i].pointstampEnd = g_iffFile->m_pchaInfo[i].pointCount;
        g_iffFile->m_pchaInfo[i].firstSectionHeaderAddress = 0;
        g_iffFile->m_pchaInfo[i].lastSectionHeaderAddress = 0;
        g_iffFile->m_pLastSecDataHeaderAddr[i] = 0;
    }
    IffFileSaveHeader(g_iffFile);
    g_iffFile->m_curOffset = g_iffFile->m_fileHeader.firstSectionHeaderAddress;
    curfilesavelen = 0;
    return true;
}

int IffFileNewDir(char *fileRoad,char* deviceName,int64_t time,IffFile *g_iffFile)
{
    char filename[256];
    char cur_time[64];
    memset(cur_time, 0x00, sizeof(cur_time));
    memset(filename, 0x00, sizeof(filename));

    GetTimeStamp(cur_time);
    strcat(filename, fileRoad);
    strcat(filename, "/");
    //mkdirs(filename);

    strcat(filename, deviceName);
    strcat(filename, "/");
    mkdirs(filename);

    strcpy(dirname, filename);
    updateSaveLastLen(dirname,g_iffFile);

    strcat(filename, cur_time);
    strcat(filename, ".iff");
    printf("filename:  %s\n", filename);
    if (NULL == (g_iffFile->m_filePtr = fopen(filename, "w+")))
    {
        printf("Fail to open IFF file!\n");
        return -1;
    }
    int fd = fileno(g_iffFile->m_filePtr);

    if (fd != -1)
        ftruncate(fd, FILE_MAX_LEN);

    strcpy(g_iffFile->m_fileHeader.token, "Incremental File Format");
    strcpy(g_iffFile->m_fileHeader.version, "1.0.0.7");
    g_iffFile->m_fileHeader.size = sizeof(g_iffFile->m_fileHeader);

    for (int i = 0; i < g_iffFile->m_channelHeader.channelCount; ++i)
    {
        g_iffFile->m_pchaInfo[i].timestart = time;
        g_iffFile->m_pchaInfo[i].pointstampBegin = 0;
        g_iffFile->m_pchaInfo[i].pointstampEnd = 0;
        g_iffFile->m_pchaInfo[i].sectionCount = 0;
        g_iffFile->m_pchaInfo[i].pointCount = 0;
        g_iffFile->m_pchaInfo[i].firstSectionHeaderAddress = 0;
        g_iffFile->m_pchaInfo[i].lastSectionHeaderAddress = 0;
        g_iffFile->m_pLastSecDataHeaderAddr[i] = 0;
    }

    IffFileSaveHeader(g_iffFile);
    g_iffFile->m_curOffset = g_iffFile->m_fileHeader.firstSectionHeaderAddress;
    curfilesavelen = 0;

    printf("succ to open IFF file!\n");
    return 0;
}

char* IffFileName(IffFile *g_iffFile)
{
    return g_iffFile->filename;
}
void setIffFileName(char *timestam, IffFile *g_iffFile)
{
    memset(g_iffFile->filename, 0x00, sizeof(g_iffFile->filename));
    strcpy(g_iffFile->filename, timestam);
}

int64_t beginSavetime = 0;
int64_t endSavetime = 0;
int32_t frameCount[MAX_CHANNEL];

//注意： g_iffFile->m_readOffset是以字节为单位进行的偏移， g_iffFile->m_singleFrameCount[i]是点数
bool IffFileSaveFrame(char* data, IffFile *g_iffFile)
{
    for (int i = 0; i < g_iffFile->m_channelHeader.channelCount; ++i)
    {
        //printf("%d %d %d \n",i,g_iffFile->m_readOffset[i], g_iffFile->m_singleFrameCount[i]);
        //	printf("%f %f %f \n",(float*)data[ g_iffFile->m_readOffset[i]+8],(float*)data[ g_iffFile->m_readOffset[i]+12],(float*)data[ g_iffFile->m_readOffset[i]+16]);
        writeSection(g_iffFile, i, data + g_iffFile->m_readOffset[i], g_iffFile->m_singleFrameCount[i]);
    }
    fflush(g_iffFile->m_filePtr);
    int fd = fileno(g_iffFile->m_filePtr);
    if (fd != -1)
    {
        fdatasync(fd);
    }

    curfilesavelen += g_iffFile->m_singleFrameLen; //单次文件存储大小
    testfilesavelen += g_iffFile->m_singleFrameLen; //单次试验存储大小
    if (testfilesaveLastlen <= testfilesavelen) //可用的总存储空间小于单次试验已经存入大小
    {
        g_iffFile->m_isDiskFull = 1;
        IffFileClose(g_iffFile);
        printf("IffFileSaveFrame testfilesavelen more than testfilesaveLastlen : %lld %lld\n", testfilesavelen, testfilesaveLastlen);
        return false;
    }
    if (FILE_MAX_LEN < curfilesavelen) //单次文件已存小于开辟文件大小
    {
        if (IffFileNewFile(g_iffFile))
        {
            printf("create a new file savelen: %lld\n", curfilesavelen);
            curfilesavelen = 0;
        }
        else
        {
            printf("create a new file false\n");
            return false;
        }
    }
    return true;
}

void IffFileSaveChannel(int i,char * data,int pointCount,IffFile *g_iffFile,int size)
{
    if(pointCount==0)
        return;
    writeSection(g_iffFile, i, data, pointCount);
    curfilesavelen += pointCount*size; //单次文件存储大小
    testfilesavelen += pointCount*size; //单次试验存储大小
}
bool IffFileCheckSize(IffFile *g_iffFile)
{
    fflush(g_iffFile->m_filePtr);
    int fd = fileno(g_iffFile->m_filePtr);
    if (fd != -1)
    {
        fdatasync(fd);
    }
    if (testfilesaveLastlen <= testfilesavelen) //可用的总存储空间小于单次试验已经存入大小
    {
        g_iffFile->m_isDiskFull = 1;
        IffFileClose(g_iffFile);
        printf("IffFileSaveFrame testfilesavelen more than testfilesaveLastlen : %lld %lld\n", testfilesavelen, testfilesaveLastlen);
        return false;
    }
    if (FILE_MAX_LEN < curfilesavelen) //单次文件已存小于开辟文件大小
    {
        if (IffFileNewFile(g_iffFile))
        {
            printf("create a new file savelen: %lld\n", curfilesavelen);
            curfilesavelen = 0;
        }
        else
        {
            printf("create a new file false\n");
            return false;
        }
    }
    return true;
}

bool IffFileSaveFile(char* data, int frameLength, int FP_File, IffFile *g_iffFile)
{
    int size = write(FP_File, data, frameLength);
    if (size != frameLength)
    {
        printf("写入失败 预计数据大小%d 实际写入大小 %d\n", frameLength, size);
        g_iffFile->m_isDiskFull = 1;
        IffFileClose(g_iffFile);
        printf("磁盘空间已满！IffFileSaveFile testfilesavelen more than testfilesaveLastlen : %lld %lld\n", testfilesavelen, testfilesaveLastlen);
        return false;
    }
    testfilesavelen += frameLength; //单次试验存储大小
    if (testfilesaveLastlen <= testfilesavelen) //可用的总存储空间小于单次试验已经存入大小
    {
        g_iffFile->m_isDiskFull = 1;
        IffFileClose(g_iffFile);
        printf("磁盘空间已满！IffFileSaveFile testfilesavelen more than testfilesaveLastlen : %lld %lld\n", testfilesavelen, testfilesaveLastlen);
        return false;
    }
    return true;
}
bool IffFileSaveHeader(IffFile *g_iffFile)
{
    FILE *_file = g_iffFile->m_filePtr;
    if (_file == NULL)
    {
        return false;
    }
    fseek(_file, 0, SEEK_SET);

    g_iffFile->m_fileHeader.firstSectionHeaderAddress = sizeof(FileHeader) + sizeof(ChannelInformationHeader)
	        										                                + sizeof(ChannelInformation) * g_iffFile->m_channelHeader.channelCount;
    // д���ļ�ͷ
    if (g_iffFile->m_pchaInfo == NULL)
    {
        g_iffFile->m_fileHeader.firstSectionHeaderAddress = 0;
    }

    if (fwrite(&g_iffFile->m_fileHeader, sizeof(FileHeader), 1, _file) == false)
    {
        return false;
    }
    // д��ͨ����Ϣͷ
    //g_iffFile->m_channelHeader.channelCount = (int)_channelInfos.size();
    if (fwrite(&g_iffFile->m_channelHeader, sizeof(ChannelInformationHeader), 1, _file) == false)
    {
        return false;
    }

    for (int i = 0; i < g_iffFile->m_channelHeader.channelCount; ++i)
    {
        fwrite(&g_iffFile->m_pchaInfo[i], sizeof(ChannelInformation), 1, _file);
        //        printf("chanel index: %d,first:%d,begin:%ld,end:%ld,count:%ld\n",\
        //        g_iffFile->m_pchaInfo[i].index,\
        //        g_iffFile->m_pchaInfo[i].firstSectionHeaderAddress,\
        //        g_iffFile->m_pchaInfo[i].timestampBegin,\
        //        g_iffFile->m_pchaInfo[i].timestampEnd,\
        //        g_iffFile->m_pchaInfo[i].sectionCount);
    }
    fflush(_file);
    return true;
}

bool writeSection(IffFile *pfile, int channelIndex, char *pdatas, int datasize)
{
    if (NULL == pfile || NULL == pfile->m_filePtr)
    {
        printf("fail to writeSection!\n");
        return false;
    }
    if (pfile->m_channelHeader.channelCount <= channelIndex || NULL == pdatas || 0 >= datasize)
    {
        printf("Fail to writeSection1! %d %d %d\n",pfile->m_channelHeader.channelCount ,channelIndex,datasize);
        return false;
    }

    //const int32_t curOffset = FileLength(pfile->m_filePtr);  //pfile->m_filePtr->_offset;

    int32_t curOffset = pfile->m_curOffset;

    struct ChannelInformation *pchaInfo = &(pfile->m_pchaInfo[channelIndex]);

    // ����ͨ����Ϣͷ
    // ����ͨ����Ϣͷ�ĵ�һ�����ݶ����ļ��е�ƫ��ֵ
    if (pchaInfo->firstSectionHeaderAddress == 0)
    {
        pchaInfo->firstSectionHeaderAddress = curOffset;
    }
    // ����ͨ����Ϣͷ�Ľ���ʱ��

    // д�����ݶ�ͷ
    const int headerSize = sizeof(struct SectionDataHeader);

    struct SectionDataHeader dataheader;

    /*debug byluo 20220614 index error*/
    //dataheader.channelIndex = channelIndex;
    dataheader.channelIndex = pchaInfo->index;
    /*debug byluo 20220614 index error*/

    pchaInfo->sectionCount++;

    dataheader.sectionIndex = pchaInfo->sectionCount;

    dataheader.valueCount = datasize;
    if (pchaInfo->DataForm == 4 || pchaInfo->DataForm == 8)
    {
        dataheader.valueSize = (sizeof(int8_t) * dataheader.valueCount);
    }
    else if (pchaInfo->DataForm == 64)
    {
        dataheader.valueSize = (sizeof(int16_t) * dataheader.valueCount);
    }
    else
    {
        dataheader.valueSize = (sizeof(float) * dataheader.valueCount);
    }
    dataheader.pointstamp = pchaInfo->pointstampBegin + pchaInfo->pointCount;

    pchaInfo->pointCount += datasize;
    pchaInfo->pointstampEnd = pchaInfo->pointstampBegin + pchaInfo->pointCount - 1;

    dataheader.previousSectionHeaderAddress = pfile->m_pLastSecDataHeaderAddr[channelIndex];
    dataheader.nextSectionHeaderAddress = 0;

    fwrite(&dataheader, headerSize, 1, pfile->m_filePtr);
    pfile->m_curOffset += headerSize;

    if (pchaInfo->DataForm == 4|| pchaInfo->DataForm == 8)
    {
        fwrite(pdatas, sizeof(int8_t), datasize, pfile->m_filePtr);
        pfile->m_curOffset += sizeof(int8_t) * datasize;
    }
    else if(pchaInfo->DataForm == 64)
    {
        fwrite(pdatas, sizeof(int16_t), datasize, pfile->m_filePtr);
        pfile->m_curOffset += sizeof(int16_t) * datasize;
    }
    else
    {
        fwrite(pdatas, sizeof(float), datasize, pfile->m_filePtr);
        pfile->m_curOffset += sizeof(float) * datasize;
    }

    const int32_t lastAddress = pfile->m_pLastSecDataHeaderAddr[channelIndex];
    if (lastAddress != 0)
    {
        const int32_t offset = pfile->m_pLastSecDataHeaderAddr[channelIndex] + headerSize - sizeof(dataheader.nextSectionHeaderAddress);

        fseek(pfile->m_filePtr, offset, 0);
        fwrite(&curOffset, sizeof(curOffset), 1, pfile->m_filePtr);
        fseek(pfile->m_filePtr, pfile->m_curOffset, SEEK_SET);
    }

    pfile->m_pLastSecDataHeaderAddr[channelIndex] = curOffset;
    pchaInfo->lastSectionHeaderAddress = curOffset;

    return true;
}

/*
 * 应该在每次开始存储前调用，变更rp后调用
 */
void UpdateFile(RunningParam *rp, IffFile *g_iffFile)
{
    if (NULL != g_iffFile->m_filePtr)
    {
        DeinitIffFile(g_iffFile);
        InitIffFile(g_iffFile);
    }

    strcpy(g_iffFile->m_fileHeader.token, "Incremental File Format");
    strcpy(g_iffFile->m_fileHeader.version, "1.0.0.7");
    g_iffFile->m_fileHeader.size = sizeof(g_iffFile->m_fileHeader);

    int allChnl = rp->m_chnldefs.m_nChnls;
    g_iffFile->m_channelHeader.channelCount = allChnl;
    g_iffFile->m_channelHeader.size = sizeof(struct ChannelInformationHeader);      //
    g_iffFile->m_channelHeader.address = sizeof(struct FileHeader);                    // ���ļ��еĵ�ַ

    g_iffFile->m_pchaInfo = (ChannelInformation*) malloc(sizeof(ChannelInformation) * (allChnl));
    memset(g_iffFile->m_pchaInfo, 0x00, sizeof(ChannelInformation) * (allChnl));
    g_iffFile->m_pLastSecDataHeaderAddr = (int32_t*) malloc(sizeof(int32_t) * (allChnl));
    memset(g_iffFile->m_pLastSecDataHeaderAddr, 0x00, sizeof(int32_t) * (allChnl));

    g_iffFile->m_singleFrameLen = sizeof(struct SectionDataHeader) * (allChnl);


    //模拟板卡
    int chnlIndex=0;
    for (int i = 0; i < rp->m_chnldefs.m_nChnls; i++)
    {
        g_iffFile->m_pchaInfo[chnlIndex].index = chnlIndex;
        memcpy(g_iffFile->m_pchaInfo[chnlIndex].name, rp->m_chnldefs.m_chnlParam[i]->strChnlName, strlen(rp->m_chnldefs.m_chnlParam[i]->strChnlName));
        g_iffFile->m_pchaInfo[chnlIndex].name[strlen(rp->m_chnldefs.m_chnlParam[i]->strChnlName)] = '\0';
        g_iffFile->m_pchaInfo[chnlIndex].samplingFrequency = (double) rp->m_chnldefs.m_chnlParam[i]->nSamplingRate;
        memcpy(g_iffFile->m_pchaInfo[chnlIndex].physicType, rp->m_chnldefs.m_chnlParam[i]->strPhysicType, strlen(rp->m_chnldefs.m_chnlParam[i]->strPhysicType));
        g_iffFile->m_pchaInfo[chnlIndex].physicType[strlen(rp->m_chnldefs.m_chnlParam[i]->strPhysicType)] = '\0';
        memcpy(g_iffFile->m_pchaInfo[chnlIndex].unitY, rp->m_chnldefs.m_chnlParam[i]->strUnit, strlen(rp->m_chnldefs.m_chnlParam[i]->strUnit));
        g_iffFile->m_pchaInfo[chnlIndex].unitY[strlen(rp->m_chnldefs.m_chnlParam[i]->strUnit)] = '\0';
        g_iffFile->m_singleFrameCount[chnlIndex] = rp->m_chnldefs.m_chnlParam[i]->nFrameCount;
        g_iffFile->m_pchaInfo[chnlIndex].DataForm = 1;
        g_iffFile->m_singleFrameLen += sizeof(float) * g_iffFile->m_singleFrameCount[chnlIndex];
        chnlIndex++;
    }
    IffFileSaveHeader(g_iffFile);
    testfilesavelen = 0; //单次试验存储大小

    int indexSave = 0;
    int readOffset = MAX_CHANNEL * 4 + MAX_CHANNEL * 8;   //隔开时间8字节+128*2字节

    for (int i = 0; i < rp->m_chnldefs.m_nChnls; i++)
    {
        if (rp->m_chnldefs.m_chnlParam[i]->bStoreEnable == 1)
        {
            g_iffFile->m_readOffset[indexSave] = readOffset;
            indexSave++;
        }
        readOffset += rp->m_chnldefs.m_chnlParam[i]->nFrameSize;
    }
}

bool isIFFFileDiskFull(IffFile *g_iffFile)
{
    return g_iffFile->m_isDiskFull == 1;
}
void updateSaveLastLen(char*fileroad, IffFile *g_iffFile)
{
    testfilesaveLastlen = SKY_GetDiskFreeZone(fileroad) - DISK_MIN_LEN; //可用的总存储空间

    if (testfilesaveLastlen <= 0)
    {
        g_iffFile->m_isDiskFull = 1;
        //printf("LastSaveSpaceNone:%f GB!\n", testfilesaveLastlen / GB);
    }
    else
    {
        g_iffFile->m_isDiskFull = 0;
        //	printf("LastSaveSpace:%f GB!\n", testfilesaveLastlen / GB);
    }
}
