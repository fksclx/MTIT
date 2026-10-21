/*
 * jsonFileHelper.c
 *
 *  Created on: 20260806
 *      Author: jw
 */

#include "jsonFileReadHelper.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>
#include "common.h"
#include "cjson/cJSON.h"

struct JsonDeviceParam jParam;
extern char iffDeviceName[256];

//读本地设备参数
void readJsonFileFactoryInfo()
{
    FactoryInfo *faInfo = &jParam.factory;
    char filename[150];
    memset(filename, 0x00, sizeof(filename));
    sprintf(filename, "/home/admin/mnt/factoryInfo.txt");
    FILE *filePtr = fopen(filename, "r");
    if (NULL == filePtr)
    {
        printf("Fail to open factoryInfo.txt in JsonFileFactoryInfo!\n");
        return;
    }
    int64_t filelen = FileLength(filePtr);
    char *filestr = (char*) malloc(filelen + 1);
    int len = fread(filestr, 1, filelen, filePtr);
    printf("JsonFileFactoryInfo len: %d!\n", len);
    cJSON *cjson_info = cJSON_Parse(filestr);
    cJSON *cjson_factory_info = cJSON_GetObjectItem(cjson_info, "factoryInfo");
    if (NULL == cjson_factory_info)
        printf("cjson_card_info  NULLLLLL!\n");
    cJSON *cjson_name = cJSON_GetObjectItem(cjson_factory_info, "ProductName");
    cJSON *cjson_model = cJSON_GetObjectItem(cjson_factory_info, "Model");
    cJSON *cjson_sn = cJSON_GetObjectItem(cjson_factory_info, "S/N");
    cJSON *cjson_manu = cJSON_GetObjectItem(cjson_factory_info, "ManuFactured");
    char *char_name = cJSON_GetStringValue(cjson_name);
    char *char_model = cJSON_GetStringValue(cjson_model);
    char *char_sn = cJSON_GetStringValue(cjson_sn);
    char *char_manu = cJSON_GetStringValue(cjson_manu);
    strcpy(faInfo->m_productName, char_name);
    strcpy(faInfo->m_model, char_model);
    strcpy(faInfo->m_sn, char_sn);
    strcpy(faInfo->m_manuFactured, char_manu);
    strcpy(iffDeviceName,char_name);
    if (NULL != filestr)
    {
        free(filestr);
        filestr = NULL;
    }
    if (NULL != filePtr)
    {
        fclose(filePtr);
        filePtr = NULL;
    }
    if (NULL != cjson_info)
    {
        cJSON_Delete(cjson_info);
        cjson_info = NULL;
    }
}
/*
 * 读取本地板卡文件  目前没用
 */
void readJsonFileCard() //读取板卡信息
{
    char filename[150];
    memset(filename, 0x00, sizeof(filename));
    sprintf(filename, "/home/admin/mnt/cardInfo.txt");
    FILE *filePtr = fopen(filename, "r");
    if (NULL == filePtr)
    {
        printf("Fail to open cardinfo.txt in readJsonFileCard!\n");
        return;
    }
    int64_t filelen = FileLength(filePtr);
    char *filestr = (char*) malloc(filelen + 1);
    int len = fread(filestr, 1, filelen, filePtr);
    printf("JsonFileCard len: %d!\n", len);
    cJSON *cjson_info = cJSON_Parse(filestr);
    cJSON *cjson_card_info = cJSON_GetObjectItem(cjson_info, "cardInfo");
    if (NULL == cjson_card_info)
        printf("cjson_card_info  NULLLLLL!\n");
    cJSON *cjson_card_array = cJSON_GetObjectItem(cjson_card_info, "cardArray");
    if (NULL == cjson_card_array)
        printf("cjson_card_array  NULLLLLL!\n");
    int array_size = cJSON_GetArraySize(cjson_card_array);
    printf("card array_size_card: %d\n", array_size);
    int nAnalogCards = 0;
    int nDIOCards = 0;
    int nDigitalCards = 0;
    for (int i = 0; i < array_size; i++)
    {
        //这里需要开辟不同类型的板卡
        cJSON *cjson_card = cJSON_GetArrayItem(cjson_card_array, i);
        cJSON *cjson_model = cJSON_GetObjectItem(cjson_card, "Card_Model");
        char *char_model = cJSON_GetStringValue(cjson_model);
        if (strstr(char_model, "AD") != NULL)
        {
            nAnalogCards++;
        }
        else if (strstr(char_model, "IO") != NULL)
        {
            nDIOCards++;
        }
        else if (strstr(char_model, "DA") != NULL)
        {
            nDigitalCards++;
        }
        else
        {
            printf("unknown card type!!!!\n");
        }
    }
    jParam.m_nAnalogCards = nAnalogCards;
    if (nAnalogCards > 0)
    {
        jParam.m_AnalogCardParams = malloc(nAnalogCards * sizeof(AnalogCardParams*)); //开辟指针空间
        memset(jParam.m_AnalogCardParams, 0x00, nAnalogCards * sizeof(AnalogCardParams*));
    }

    jParam.m_nDigitalCards = nDigitalCards;
    if (nDigitalCards > 0)
    {
        jParam.m_DigitalCardParams = malloc(nDigitalCards * sizeof(DigitalCardParams*)); //开辟指针空间
        memset(jParam.m_DigitalCardParams, 0x00, nDigitalCards * sizeof(DigitalCardParams*));
    }

    jParam.m_nIOCards = nDIOCards;
    if (nDigitalCards > 0)
    {
        jParam.m_IOCardParams = malloc(nDIOCards * sizeof(IOCardParams*)); //开辟指针空间
        memset(jParam.m_IOCardParams, 0x00, nDIOCards * sizeof(IOCardParams*));
    }
    nDIOCards =0;
    nAnalogCards = 0;
    nDigitalCards = 0;
    for (int i = 0; i < array_size; i++)
    {
        cJSON *cjson_card = cJSON_GetArrayItem(cjson_card_array, i);
        cJSON *cjson_name = cJSON_GetObjectItem(cjson_card, "Card_Name");
        cJSON *cjson_index = cJSON_GetObjectItem(cjson_card, "Card_Index");
        cJSON *cjson_slot = cJSON_GetObjectItem(cjson_card, "Card_Slot");
        cJSON *cjson_channels = cJSON_GetObjectItem(cjson_card, "Card_Channels");
        cJSON *cjson_model = cJSON_GetObjectItem(cjson_card, "Card_Model");

        char *char_name = cJSON_GetStringValue(cjson_name);
        char *char_model = cJSON_GetStringValue(cjson_model);
        int slot = cJSON_GetNumberValue(cjson_slot);
        int index = cJSON_GetNumberValue(cjson_index);
        int channels = cJSON_GetNumberValue(cjson_channels);

        if (strstr(char_model, "AD") != NULL)
        {
            jParam.m_AnalogCardParams[nAnalogCards] = malloc(sizeof(AnalogCardParams));
            memset(jParam.m_AnalogCardParams[nAnalogCards], 0x00, sizeof(AnalogCardParams));
            jParam.m_AnalogCardParams[nAnalogCards]->nCardChannel = channels;
            jParam.m_AnalogCardParams[nAnalogCards]->nCardSlot = slot;
            jParam.m_AnalogCardParams[nAnalogCards]->nCardIndex = index;
            strcpy(jParam.m_AnalogCardParams[nAnalogCards]->strCardModel, char_model);
            strcpy(jParam.m_AnalogCardParams[nAnalogCards]->strCardName, char_name);
            nAnalogCards++;
        }
        else if (strstr(char_model, "DA") != NULL)
        {
            jParam.m_DigitalCardParams[nDigitalCards] = malloc(sizeof(DigitalCardParams));
            memset(jParam.m_DigitalCardParams[nDigitalCards], 0x00, sizeof(DigitalCardParams));
            jParam.m_DigitalCardParams[nDigitalCards]->nCardChannel = channels;
            jParam.m_DigitalCardParams[nDigitalCards]->nCardSlot = slot;
            jParam.m_DigitalCardParams[nDigitalCards]->nCardIndex = index;
            strcpy(jParam.m_DigitalCardParams[nDigitalCards]->strCardModel, char_model);
            strcpy(jParam.m_DigitalCardParams[nDigitalCards]->strCardName, char_name);
            nDigitalCards++;
        }
        else if (strstr(char_model, "IO") != NULL)
        {
            jParam.m_IOCardParams[nDIOCards] = malloc(sizeof(IOCardParams));
            memset(jParam.m_IOCardParams[nDIOCards], 0x00, sizeof(IOCardParams));
            jParam.m_IOCardParams[nDIOCards]->nCardChannel = channels;
            jParam.m_IOCardParams[nDIOCards]->nCardSlot = slot;
            jParam.m_IOCardParams[nDIOCards]->nCardIndex = index;
            strcpy(jParam.m_IOCardParams[nDIOCards]->strCardModel, char_model);
            strcpy(jParam.m_IOCardParams[nDIOCards]->strCardName, char_name);
            nDIOCards++;
        }
        else
        {
            printf("unknown card type!!!!\n");
        }
    }

    if (NULL != filestr)
    {
        free(filestr);
        filestr = NULL;
    }
    if (NULL != filePtr)
    {
        fclose(filePtr);
        filePtr = NULL;
    }
    if (NULL != cjson_info)
    {
        cJSON_Delete(cjson_info);
        cjson_info = NULL;
    }
}
/*
 * 读取本地通道信息文件，目前不需要
 */
void readJsonFileChannel()
{
    char filename[150];
    memset(filename, 0x00, sizeof(filename));
    sprintf(filename, "/home/admin/mnt/channelInfo.txt");
    FILE *filePtr = fopen(filename, "r");
    if (NULL == filePtr)
    {
        printf("Fail to open channelInfo.txt in readJsonFileChannel!\n");
        return;
    }
    int64_t filelen = FileLength(filePtr);
    char *filestr = (char*) malloc(filelen + 1);
    int len = fread(filestr, 1, filelen, filePtr);
    printf("JsonFileChannelInfo len: %d!\n", len);
    cJSON *cjson_info = cJSON_Parse(filestr);
    cJSON *cjson_channel_info = cJSON_GetObjectItem(cjson_info, "channelInfo");
    if (NULL == cjson_channel_info)
        printf("cjson_channelInfo  NULLLLLL!\n");
    cJSON *cjson_channel_array = cJSON_GetObjectItem(cjson_channel_info, "channelArray");
    if (NULL == cjson_channel_array)
        printf("cjson_channelInfo  NULLLLLL!\n");
    int array_size = cJSON_GetArraySize(cjson_channel_array);
    printf("channel array_size: %d\n", array_size);

    int channelCount = 0;
    for (int i = 0; i < jParam.m_nAnalogCards; i++)
    {
        channelCount += jParam.m_AnalogCardParams[i]->nCardChannel;
    }
    jParam.m_chnldefs.m_nAnalogChnls = channelCount;
    jParam.m_chnldefs.m_AnalogChnlParam = malloc(channelCount * sizeof(ChnlParams*));
    memset(jParam.m_chnldefs.m_AnalogChnlParam, 0x00, channelCount * sizeof(ChnlParams*));

    channelCount = 0;
    for (int i = 0; i < jParam.m_nDigitalCards; i++)
    {
        channelCount += jParam.m_DigitalCardParams[i]->nCardChannel;
    }
    jParam.m_chnldefs.m_nDigitalChnls = channelCount;
    jParam.m_chnldefs.m_DigitalChnlParam = malloc(channelCount * sizeof(ChnlParams*));
    memset(jParam.m_chnldefs.m_DigitalChnlParam, 0x00, channelCount * sizeof(ChnlParams*));

//    channelCount = 0;
//    for (int i = 0; i < jParam.m_nVirtualCards; i++)
//    {
//        channelCount += jParam.m_VirtualCardParams[i]->nCardChannel;
//    }
//    jParam.m_chnldefs.m_nVirtualChnls = channelCount;
//    jParam.m_chnldefs.m_VirtualChnlParam = malloc(channelCount * sizeof(ChnlParams*));
//    memset(jParam.m_chnldefs.m_VirtualChnlParam, 0x00, channelCount * sizeof(ChnlParams*));

    channelCount = 0;
    for (int i = 0; i < jParam.m_nIOCards; i++)
    {
        channelCount += jParam.m_IOCardParams[i]->nCardChannel;
    }
    jParam.m_chnldefs.m_nIOChnls = channelCount;
    jParam.m_chnldefs.m_IOChnlParam = malloc(channelCount * sizeof(ChnlParams*));
    memset(jParam.m_chnldefs.m_IOChnlParam, 0x00, channelCount * sizeof(ChnlParams*));


    printf("channel Count %d %d %d \n", jParam.m_chnldefs.m_nAnalogChnls , jParam.m_chnldefs.m_nDigitalChnls, jParam.m_chnldefs.m_nIOChnls);
    for (int i = 0; i < array_size; i++)
    {
        ChnlParams *channelParam;
        if(i<jParam.m_chnldefs.m_nAnalogChnls)
        {
            jParam.m_chnldefs.m_AnalogChnlParam[i] = malloc(sizeof(ChnlParams));
            memset(jParam.m_chnldefs.m_AnalogChnlParam[i], 0, sizeof(ChnlParams)); //初始化为0
            channelParam = jParam.m_chnldefs.m_AnalogChnlParam[i];
        }
        else if(i<jParam.m_chnldefs.m_nAnalogChnls+jParam.m_chnldefs.m_nDigitalChnls)
        {
            jParam.m_chnldefs.m_DigitalChnlParam[i-jParam.m_chnldefs.m_nAnalogChnls] = malloc(sizeof(ChnlParams));
            memset(jParam.m_chnldefs.m_DigitalChnlParam[i-jParam.m_chnldefs.m_nAnalogChnls], 0, sizeof(ChnlParams)); //初始化为0
            channelParam = jParam.m_chnldefs.m_DigitalChnlParam[i-jParam.m_chnldefs.m_nAnalogChnls];
        }
        else if(i<jParam.m_chnldefs.m_nAnalogChnls+jParam.m_chnldefs.m_nDigitalChnls+jParam.m_chnldefs.m_nIOChnls)
        {
            jParam.m_chnldefs.m_IOChnlParam[i-jParam.m_chnldefs.m_nAnalogChnls-jParam.m_chnldefs.m_nDigitalChnls] = malloc(sizeof(ChnlParams));
            memset(jParam.m_chnldefs.m_IOChnlParam[i-jParam.m_chnldefs.m_nAnalogChnls-jParam.m_chnldefs.m_nDigitalChnls], 0, sizeof(ChnlParams)); //初始化为0
            channelParam = jParam.m_chnldefs.m_IOChnlParam[i-jParam.m_chnldefs.m_nAnalogChnls-jParam.m_chnldefs.m_nDigitalChnls];
        }
        else
        {
            continue;
        }


        cJSON *cjson_channel = cJSON_GetArrayItem(cjson_channel_array, i);
        cJSON *cjson_channel_Dindex = cJSON_GetObjectItem(cjson_channel, "chnlIndex_device");
        cJSON *cjson_channel_Cindex = cJSON_GetObjectItem(cjson_channel, "chnlIndex_card");
        cJSON *cjson_card = cJSON_GetObjectItem(cjson_channel, "chnl_cardIndex");

        cJSON *cjson_name = cJSON_GetObjectItem(cjson_channel, "name");
        cJSON *cjson_type = cJSON_GetObjectItem(cjson_channel, "senserType");
        cJSON *cjson_ntype = cJSON_GetObjectItem(cjson_channel, "physicType");
        cJSON *cjson_rindex = cJSON_GetObjectItem(cjson_channel, "range");

        cJSON *cjson_uplimit = cJSON_GetObjectItem(cjson_channel, "upLimit");
        cJSON *cjson_lowlimit = cJSON_GetObjectItem(cjson_channel, "lowLimit");
        cJSON *cjson_kcoff = cJSON_GetObjectItem(cjson_channel, "kCoff");
        cJSON *cjson_bcoff = cJSON_GetObjectItem(cjson_channel, "bCoff");
        cJSON *cjson_daq = cJSON_GetObjectItem(cjson_channel, "daqEnable");
        cJSON *cjson_save = cJSON_GetObjectItem(cjson_channel, "saveEnable");
        cJSON *cjson_limit = cJSON_GetObjectItem(cjson_channel, "limitEnable");
        cJSON *cjson_unit = cJSON_GetObjectItem(cjson_channel, "unit");
        cJSON *cjson_samplingRate = cJSON_GetObjectItem(cjson_channel, "samplingRate");
        cJSON *cjson_sensitivity = cJSON_GetObjectItem(cjson_channel, "sensitivity");
        cJSON *cjson_temperatrue = cJSON_GetObjectItem(cjson_channel, "TemperatureCompesate");
        cJSON *cjson_LowFilter = cJSON_GetObjectItem(cjson_channel, "lowFilter");
        cJSON *cjson_BridgeMode = cJSON_GetObjectItem(cjson_channel, "BridgeMode");
        cJSON *cjson_BridgeResistance = cJSON_GetObjectItem(cjson_channel, "BridgeResistance");
        cJSON *cjson_ImpUnit = cJSON_GetObjectItem(cjson_channel, "ImpUnit");
        cJSON *cjson_ImpValue = cJSON_GetObjectItem(cjson_channel, "ImpValue");
        cJSON *cjson_ZeroCoeff = cJSON_GetObjectItem(cjson_channel, "ZeroCoeff");

        if(cjson_temperatrue!=NULL)
        {
            channelParam->fTemperatureCompesateTemp=(float)cJSON_GetNumberValue(cjson_temperatrue);
        }
        else
        {
            channelParam->fTemperatureCompesateTemp=26.5;
        }

        int channel_index = (int) cJSON_GetNumberValue(cjson_channel_Dindex);
        int card_index = (int) cJSON_GetNumberValue(cjson_channel_Cindex);
        int card = (int) cJSON_GetNumberValue(cjson_card);
        char *name = cJSON_GetStringValue(cjson_name);
        char *type = cJSON_GetStringValue(cjson_type);
        char *physictype = cJSON_GetStringValue(cjson_ntype);
        char *range = cJSON_GetStringValue(cjson_rindex);

        char *unit = cJSON_GetStringValue(cjson_unit);
        int samplingRate = (int) cJSON_GetNumberValue(cjson_samplingRate);
        float sensitivity = (float) cJSON_GetNumberValue(cjson_sensitivity);

        float uplimit = (float) cJSON_GetNumberValue(cjson_uplimit);
        float lowlimit = (float) cJSON_GetNumberValue(cjson_lowlimit);
        float kcoff = (float) cJSON_GetNumberValue(cjson_kcoff);
        float bcoff = (float) cJSON_GetNumberValue(cjson_bcoff);

        int daq = (int) cJSON_GetNumberValue(cjson_daq);
        int save = (int) cJSON_GetNumberValue(cjson_save);
        int limit = (int) cJSON_GetNumberValue(cjson_limit);


        if (cjson_LowFilter != NULL)
        {
            strcpy(channelParam->strLowFilter, cJSON_GetStringValue(cjson_LowFilter)); //放大倍数和量程是一个意思吗
        }
        else
        {
            strcpy(channelParam->strLowFilter, "直通"); //放大倍数和量程是一个意思吗
        }

        if (cjson_BridgeMode != NULL)
        {
            strcpy(channelParam->strBridgeMode, cJSON_GetStringValue(cjson_BridgeMode));
        }
        else
        {
            strcpy(channelParam->strBridgeMode, "全桥"); //全桥 1/4桥
        }

        if (cjson_BridgeResistance != NULL)
        {
            channelParam->fBridgeResistance=cJSON_GetNumberValue(cjson_BridgeResistance);
        }
        else
        {
            channelParam->fBridgeResistance=0; //电桥电阻
        }

        if (cjson_ImpUnit != NULL)
        {
            strcpy(channelParam->strImpUnit, cJSON_GetStringValue(cjson_ImpUnit)); //放大倍数和量程是一个意思吗
        }
        else
        {
            strcpy(channelParam->strImpUnit, "mv"); //激励电压单位
        }
        if (cjson_ImpValue != NULL)
        {
            channelParam->fImpValue= cJSON_GetNumberValue(cjson_ImpValue); //激励电压
        }
        else
        {
            channelParam->fImpValue=1; //激励电压
        }


        channelParam->chnlIndex_device = channel_index;
        channelParam->chnlIndex_card = card_index;
        channelParam->chnl_cardIndex = card;

        strcpy(channelParam->strChnlName, name);

        strcpy(channelParam->strPhysicType, physictype);
        strcpy(channelParam->strSenserType, type);
        strcpy(channelParam->strRange, range);

        channelParam->bDaqEnable = daq;
        channelParam->bStoreEnable = save;
        channelParam->bLimitEnable = limit;

        channelParam->kCoff = kcoff;
        channelParam->bCoff = bcoff;

        channelParam->fSensitivity = sensitivity;
        if (((channelParam->fSensitivity - 0) < 0.000001) || ((channelParam->fSensitivity - 0) > -0.000001))
            channelParam->fSensitivity_t = 1 / sensitivity; //用倒数避免出发
        else
            channelParam->fSensitivity_t = 0; //用倒数避免出发

        channelParam->nSamplingRate = samplingRate; //采样率

        if(i<jParam.m_chnldefs.m_nAnalogChnls)
        {
            if (strstr(type, "AC") != NULL)
            {
                channelParam->kCalib = jParam.m_AnalogCardParams[card]->k[0][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_AnalogCardParams[card]->b[0][card_index]; //下位机先算
            }
            else if (strstr(type, "ICP") != NULL)
            {

                channelParam->kCalib = jParam.m_AnalogCardParams[card]->k[1][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_AnalogCardParams[card]->b[1][card_index]; //下位机先算
            }
            else if (strstr(type, "DC") != NULL)
            {
                channelParam->kCalib = jParam.m_AnalogCardParams[card]->k[2][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_AnalogCardParams[card]->b[2][card_index]; //下位机先算
            }
            else if (strstr(type,"应变") !=NULL)
            {

                channelParam->kCalib = jParam.m_AnalogCardParams[card]->k[3][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_AnalogCardParams[card]->b[3][card_index]; //下位机先算
            }
            else if (strstr(type,"PT") !=NULL)
            {

                channelParam->kCalib = jParam.m_AnalogCardParams[card]->k[4][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_AnalogCardParams[card]->b[4][card_index]; //下位机先算
            }
            else if (strstr(type,"热电偶") !=NULL)
            {
                channelParam->kCalib = jParam.m_AnalogCardParams[card]->k[5][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_AnalogCardParams[card]->b[5][card_index]; //下位机先算
            }
            else
            {
                channelParam->kCalib = 10000; //下位机先算
                channelParam->bCalib = 0; //下位机先算
            }
        }
        else if(i<jParam.m_chnldefs.m_nAnalogChnls+jParam.m_chnldefs.m_nDigitalChnls)
        {
            if (strstr(type, "AC") != NULL)
            {
                channelParam->kCalib = jParam.m_DigitalCardParams[card-jParam.m_nAnalogCards]->k[0][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_DigitalCardParams[card-jParam.m_nAnalogCards]->b[0][card_index]; //下位机先算
            }
            else if (strstr(type, "ICP") != NULL)
            {

                channelParam->kCalib = jParam.m_DigitalCardParams[card-jParam.m_nAnalogCards]->k[1][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_DigitalCardParams[card-jParam.m_nAnalogCards]->b[1][card_index]; //下位机先算
            }
            else if (strstr(type, "DC") != NULL)
            {
                channelParam->kCalib = jParam.m_DigitalCardParams[card-jParam.m_nAnalogCards]->k[2][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_DigitalCardParams[card-jParam.m_nAnalogCards]->b[2][card_index]; //下位机先算
            }
            else if (strstr(type,"应变") !=NULL)
            {

                channelParam->kCalib = jParam.m_DigitalCardParams[card-jParam.m_nAnalogCards]->k[3][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_DigitalCardParams[card-jParam.m_nAnalogCards]->b[3][card_index]; //下位机先算
            }
            else if (strstr(type,"PT") !=NULL)
            {

                channelParam->kCalib = jParam.m_DigitalCardParams[card-jParam.m_nAnalogCards]->k[4][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_DigitalCardParams[card-jParam.m_nAnalogCards]->b[4][card_index]; //下位机先算
            }
            else if (strstr(type,"热电偶") !=NULL)
            {
                channelParam->kCalib = jParam.m_DigitalCardParams[card-jParam.m_nAnalogCards]->k[5][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_DigitalCardParams[card-jParam.m_nAnalogCards]->b[5][card_index]; //下位机先算
            }
            else
            {
                channelParam->kCalib = 10000; //下位机先算
                channelParam->bCalib = 0; //下位机先算
            }
        }
        else if(i<jParam.m_chnldefs.m_nAnalogChnls+jParam.m_chnldefs.m_nDigitalChnls+jParam.m_chnldefs.m_nIOChnls)
        {
            if (strstr(type, "AC") != NULL)
            {
                channelParam->kCalib = jParam.m_IOCardParams[card-jParam.m_nAnalogCards-jParam.m_nDigitalCards]->k[0][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_IOCardParams[card-jParam.m_nAnalogCards-jParam.m_nDigitalCards]->b[0][card_index]; //下位机先算
            }
            else if (strstr(type, "ICP") != NULL)
            {

                channelParam->kCalib = jParam.m_IOCardParams[card-jParam.m_nAnalogCards-jParam.m_nDigitalCards]->k[1][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_IOCardParams[card-jParam.m_nAnalogCards-jParam.m_nDigitalCards]->b[1][card_index]; //下位机先算
            }
            else if (strstr(type, "DC") != NULL)
            {
                channelParam->kCalib = jParam.m_IOCardParams[card-jParam.m_nAnalogCards-jParam.m_nDigitalCards]->k[2][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_IOCardParams[card-jParam.m_nAnalogCards-jParam.m_nDigitalCards]->b[2][card_index]; //下位机先算
            }
            else if (strstr(type,"应变") !=NULL)
            {

                channelParam->kCalib = jParam.m_IOCardParams[card-jParam.m_nAnalogCards-jParam.m_nDigitalCards]->k[3][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_IOCardParams[card-jParam.m_nAnalogCards-jParam.m_nDigitalCards]->b[3][card_index]; //下位机先算
            }
            else if (strstr(type,"PT") !=NULL)
            {

                channelParam->kCalib = jParam.m_IOCardParams[card-jParam.m_nAnalogCards-jParam.m_nDigitalCards]->k[4][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_IOCardParams[card-jParam.m_nAnalogCards-jParam.m_nDigitalCards]->b[4][card_index]; //下位机先算
            }
            else if (strstr(type,"热电偶") !=NULL)
            {
                channelParam->kCalib = jParam.m_IOCardParams[card-jParam.m_nAnalogCards-jParam.m_nDigitalCards]->k[5][card_index]; //下位机先算
                channelParam->bCalib = jParam.m_IOCardParams[card-jParam.m_nAnalogCards-jParam.m_nDigitalCards]->b[5][card_index]; //下位机先算
            }
            else
            {
                channelParam->kCalib = 10000; //下位机先算
                channelParam->bCalib = 0; //下位机先算
            }
        }
        else
        {
            channelParam->kCalib = 10000; //下位机先算
            channelParam->bCalib = 0; //下位机先算
        }
        channelParam->kFinally = channelParam->kCalib;
        channelParam->bFinally = channelParam->bCalib;

        channelParam->fGain = 1;//先给1


        channelParam->kFinally /= channelParam->fGain;
        channelParam->bFinally /= channelParam->fGain;

        strcpy(channelParam->strUnit, unit); //单位


        channelParam->kFinally *= channelParam->fSensitivity_t;
        channelParam->bFinally *= channelParam->fSensitivity_t;

        channelParam->kFinally *= channelParam->kCoff;
        channelParam->bFinally *= channelParam->kCoff;

        channelParam->bFinally += channelParam->bCoff;


    }

    if (NULL != filestr)
    {
        free(filestr);
        filestr = NULL;
    }
    if (NULL != filePtr)
    {
        fclose(filePtr);
        filePtr = NULL;
    }
    if (NULL != cjson_info)
    {
        cJSON_Delete(cjson_info);
        cjson_info = NULL;
    }
    printf("success readJsonFileChannel \n");
}

//读取标定
void readJsonFileCalib()
{
    char filename[150];
    memset(filename, 0x00, sizeof(filename));
    sprintf(filename, "/mnt/calibration.txt");
    FILE *filePtr = fopen(filename, "r");
    if (NULL == filePtr)
    {
        printf("Fail to open calibration.txt in readJsonFileCalib!\n");
        return;
    }
    int64_t filelen = FileLength(filePtr);
    char *filestr = (char*) malloc(filelen + 1);
    int len = fread(filestr, 1, filelen, filePtr);
    printf("JsonFileCalibration len: %d!\n", len);
    cJSON *cjson_info = cJSON_Parse(filestr);
    cJSON *cjson_card_info = cJSON_GetObjectItem(cjson_info, "cardInfo");
    if (NULL == cjson_card_info)
        printf("cjson_card_info  NULLLLLL!\n");
    cJSON *cjson_card_array = cJSON_GetObjectItem(cjson_card_info, "cardArray");
    if (NULL == cjson_card_array)
        printf("cjson_card_array  NULLLLLL!\n");
    int array_size = cJSON_GetArraySize(cjson_card_array);

    for (int i = 0; i < array_size && i < jParam.m_nAnalogCards; i++)
    {
        cJSON *cjson_card = cJSON_GetArrayItem(cjson_card_array, i);
        cJSON *cjson_AC_array = cJSON_GetObjectItem(cjson_card, "AC_Array");
        int AC_size = cJSON_GetArraySize(cjson_AC_array);
        for (int j = 0; j < AC_size; j++)
        {
            cJSON *cjson_kb = cJSON_GetArrayItem(cjson_AC_array, j);
            cJSON *cjson_k = cJSON_GetObjectItem(cjson_kb, "K");
            cJSON *cjson_b = cJSON_GetObjectItem(cjson_kb, "B");
            double char_k = cJSON_GetNumberValue(cjson_k);
            double char_b = cJSON_GetNumberValue(cjson_b);
            jParam.m_AnalogCardParams[i]->k[0][j] = char_k;
            jParam.m_AnalogCardParams[i]->b[0][j] = char_b;
        }

        cJSON *cjson_ICP_array = cJSON_GetObjectItem(cjson_card, "ICP_Array");
        int ICP_size = cJSON_GetArraySize(cjson_ICP_array);
        for (int j = 0; j < ICP_size; j++)
        {
            cJSON *cjson_kb = cJSON_GetArrayItem(cjson_ICP_array, j);
            cJSON *cjson_k = cJSON_GetObjectItem(cjson_kb, "K");
            cJSON *cjson_b = cJSON_GetObjectItem(cjson_kb, "B");
            double char_k = cJSON_GetNumberValue(cjson_k);
            double char_b = cJSON_GetNumberValue(cjson_b);
            jParam.m_AnalogCardParams[i]->k[1][j] = char_k;
            jParam.m_AnalogCardParams[i]->b[1][j] = char_b;
        }
        cJSON *cjson_DC_array = cJSON_GetObjectItem(cjson_card, "DC_Array");
        int DC_size = cJSON_GetArraySize(cjson_DC_array);
        for (int j = 0; j < DC_size; j++)
        {
            cJSON *cjson_kb = cJSON_GetArrayItem(cjson_DC_array, j);
            cJSON *cjson_k = cJSON_GetObjectItem(cjson_kb, "K");
            cJSON *cjson_b = cJSON_GetObjectItem(cjson_kb, "B");
            double char_k = cJSON_GetNumberValue(cjson_k);
            double char_b = cJSON_GetNumberValue(cjson_b);
            jParam.m_AnalogCardParams[i]->k[2][j] = char_k;
            jParam.m_AnalogCardParams[i]->b[2][j] = char_b;
        }


        cJSON *cjson_Strain_array = cJSON_GetObjectItem(cjson_card, "Strain_Array");
        int Strain_size = cJSON_GetArraySize(cjson_Strain_array);
        for (int j = 0; j < Strain_size; j++)
        {
            cJSON *cjson_kb = cJSON_GetArrayItem(cjson_Strain_array, j);
            cJSON *cjson_k = cJSON_GetObjectItem(cjson_kb, "K");
            cJSON *cjson_b = cJSON_GetObjectItem(cjson_kb, "B");
            double char_k = cJSON_GetNumberValue(cjson_k);
            double char_b = cJSON_GetNumberValue(cjson_b);
            jParam.m_AnalogCardParams[i]->k[3][j] = char_k;
            jParam.m_AnalogCardParams[i]->b[3][j] = char_b;
        }


        cJSON *PT_100 = cJSON_GetObjectItem(cjson_card, "PT_100");
        int PT_size = cJSON_GetArraySize(PT_100);
        for (int j = 0; j < PT_size; j++)
        {
            cJSON *cjson_kb = cJSON_GetArrayItem(PT_100, j);
            cJSON *cjson_k = cJSON_GetObjectItem(cjson_kb, "K");
            cJSON *cjson_b = cJSON_GetObjectItem(cjson_kb, "B");
            double char_k = cJSON_GetNumberValue(cjson_k);
            double char_b = cJSON_GetNumberValue(cjson_b);
            jParam.m_AnalogCardParams[i]->k[4][j] = char_k;
            jParam.m_AnalogCardParams[i]->b[4][j] = char_b;
        }

        cJSON *Thermocouple_Array = cJSON_GetObjectItem(cjson_card, "Thermocouple_Array");
        int Thermocouple_size = cJSON_GetArraySize(Thermocouple_Array);
        for (int j = 0; j < Thermocouple_size; j++)
        {
            cJSON *cjson_kb = cJSON_GetArrayItem(Thermocouple_Array, j);
            cJSON *cjson_k = cJSON_GetObjectItem(cjson_kb, "K");
            cJSON *cjson_b = cJSON_GetObjectItem(cjson_kb, "B");
            double char_k = cJSON_GetNumberValue(cjson_k);
            double char_b = cJSON_GetNumberValue(cjson_b);
            jParam.m_AnalogCardParams[i]->k[5][j] = char_k;
            jParam.m_AnalogCardParams[i]->b[5][j] = char_b;
        }
    }

    if (NULL != filestr)
    {
        free(filestr);
        filestr = NULL;
    }
    if (NULL != filePtr)
    {
        fclose(filePtr);
        filePtr = NULL;
    }
    if (NULL != cjson_info)
    {
        cJSON_Delete(cjson_info);
        cjson_info = NULL;
    }
}


/*
 * 打包出厂信息
 */
void hardInfoJsonstr(char*output)
{
    FactoryInfo *factoryInfo = &jParam.factory;
    cJSON *description = cJSON_CreateObject();

    cJSON_AddStringToObject(description, "ProductName", factoryInfo->m_productName);
    cJSON_AddStringToObject(description, "Version", "V1.0.0");
    cJSON_AddStringToObject(description, "Model", factoryInfo->m_model);
    cJSON_AddStringToObject(description, "SN", factoryInfo->m_sn);
    cJSON_AddStringToObject(description, "Manufactured", factoryInfo->m_manuFactured);


    char mac[64];
    char ip[64];
    char slotIndex[8];
    char channelNum[8];


    GetMacIpInfo(mac, ip);
    cJSON_AddStringToObject(description, "MacAddr", mac);
    cJSON_AddStringToObject(description, "IP_Addr", ip);

    cJSON *cjson_cardArray = cJSON_CreateArray();

    for (int i = 0; i < jParam.m_nAnalogCards; i++)
    {
        cJSON *cjson_card = cJSON_CreateObject();
        sprintf(slotIndex, "%d", jParam.m_AnalogCardParams[i]->nCardSlot);
        sprintf(channelNum, "%d", jParam.m_AnalogCardParams[i]->nCardChannel);
        cJSON_AddStringToObject(cjson_card, "SlotIndex", slotIndex);
        cJSON_AddStringToObject(cjson_card, "Model", jParam.m_AnalogCardParams[i]->strCardModel);
        cJSON_AddStringToObject(cjson_card, "ChannelNum", channelNum);
        cJSON_AddItemToArray(cjson_cardArray, cjson_card);
    }
    for (int i = 0; i < jParam.m_nDigitalCards; i++)
    {
        cJSON *cjson_card = cJSON_CreateObject();
        sprintf(slotIndex, "%d", jParam.m_DigitalCardParams[i]->nCardSlot);
        sprintf(channelNum, "%d", jParam.m_DigitalCardParams[i]->nCardChannel);
        cJSON_AddStringToObject(cjson_card, "SlotIndex", slotIndex);
        cJSON_AddStringToObject(cjson_card, "Model", jParam.m_DigitalCardParams[i]->strCardModel);
        cJSON_AddStringToObject(cjson_card, "ChannelNum", channelNum);
        cJSON_AddItemToArray(cjson_cardArray, cjson_card);
    }
    for (int i = 0; i < jParam.m_nIOCards; i++)
    {
        cJSON *cjson_card = cJSON_CreateObject();
        sprintf(slotIndex, "%d", jParam.m_IOCardParams[i]->nCardSlot);
        sprintf(channelNum, "%d", jParam.m_IOCardParams[i]->nCardChannel);
        cJSON_AddStringToObject(cjson_card, "SlotIndex", slotIndex);
        cJSON_AddStringToObject(cjson_card, "Model", jParam.m_IOCardParams[i]->strCardModel);
        cJSON_AddStringToObject(cjson_card, "ChannelNum", channelNum);
        cJSON_AddItemToArray(cjson_cardArray, cjson_card);
    }

    cJSON *cjson_cardInfo = cJSON_CreateObject();
    cJSON_AddItemToObject(cjson_cardInfo, "Value", cjson_cardArray);
    cJSON_AddItemToObject(description, "CardInfos", cjson_cardInfo);

//    if (jParam.m_state.isSampling )
//        cJSON_AddStringToObject(description, "Workstage", "工作中");
//    else
//        cJSON_AddStringToObject(description, "Workstage", "空闲");
//
//    if (jParam.m_state.isConnect)
//        cJSON_AddStringToObject(description, "Network", "已有连接");
//    else
//        cJSON_AddStringToObject(description, "Network", "无连接");

    cJSON *cjson_hardInfo = cJSON_CreateObject();
    cJSON_AddItemToObject(cjson_hardInfo, "Description", description);

    char *jsonstr = cJSON_Print(cjson_hardInfo);
    memcpy(output, jsonstr, strlen(jsonstr));
    cJSON_Delete(cjson_hardInfo);
    cJSON_free(jsonstr);
}
