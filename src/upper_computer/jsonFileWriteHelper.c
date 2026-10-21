/*
 * jsonFileHelper.c
 *
 *  Created on: 20260806
 *      Author: jw
 */

#include "jsonFileWriteHelper.h"

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


extern struct RunningParam gRunParam;
extern char iffDeviceName[256];
/*
 * 更新本地param,目前不用
 */
void writeJsonFileChannel()
{
    // 创建跟对象
    cJSON *cjson_info = cJSON_CreateObject();
    cJSON *cjson_channel_info = cJSON_CreateObject();
    cJSON_AddItemToObject(cjson_info, "channelInfo", cjson_channel_info);
    cJSON *cjson_channel_array = cJSON_CreateArray();
    cJSON_AddItemToObject(cjson_channel_info, "channelArray", cjson_channel_array);

    for (int i = 0; i < gRunParam.m_chnldefs.m_nChnls; i++)
    {
        cJSON *cjson_channel = cJSON_CreateObject();
        ChnlParams *channelParam = gRunParam.m_chnldefs.m_chnlParam[i];

        cJSON_AddItemToObject(cjson_channel, "chnlIndex_device", cJSON_CreateNumber(channelParam->chnlIndex_device));
        cJSON_AddItemToObject(cjson_channel, "chnlIndex_card", cJSON_CreateNumber(channelParam->chnlIndex_card));
        cJSON_AddItemToObject(cjson_channel, "chnl_cardIndex", cJSON_CreateNumber(channelParam->chnl_cardIndex));
        cJSON_AddItemToObject(cjson_channel, "name", cJSON_CreateString(channelParam->strChnlName));
        cJSON_AddItemToObject(cjson_channel, "senserType", cJSON_CreateString(channelParam->strSenserType));
        cJSON_AddItemToObject(cjson_channel, "physicType", cJSON_CreateString(channelParam->strPhysicType));
        cJSON_AddItemToObject(cjson_channel, "range", cJSON_CreateString(channelParam->strRange));
        cJSON_AddItemToObject(cjson_channel, "kCoff", cJSON_CreateNumber(channelParam->kCoff));
        cJSON_AddItemToObject(cjson_channel, "bCoff", cJSON_CreateNumber(channelParam->bCoff));
        cJSON_AddItemToObject(cjson_channel, "daqEnable", cJSON_CreateNumber(channelParam->bDaqEnable));
        cJSON_AddItemToObject(cjson_channel, "saveEnable", cJSON_CreateNumber(channelParam->bStoreEnable));
        cJSON_AddItemToObject(cjson_channel, "limitEnable", cJSON_CreateNumber(channelParam->bLimitEnable));
        cJSON_AddItemToObject(cjson_channel, "unit", cJSON_CreateString(channelParam->strUnit));
        cJSON_AddItemToObject(cjson_channel, "samplingRate", cJSON_CreateNumber(channelParam->nSamplingRate));
        cJSON_AddItemToObject(cjson_channel, "sensitivity", cJSON_CreateNumber(channelParam->fSensitivity));
        cJSON_AddItemToObject(cjson_channel, "lowFilter", cJSON_CreateString(channelParam->strLowFilter));
        cJSON_AddItemToObject(cjson_channel, "BridgeMode", cJSON_CreateString(channelParam->strBridgeMode));
        cJSON_AddItemToObject(cjson_channel, "BridgeResistance", cJSON_CreateNumber(channelParam->fBridgeResistance));
        cJSON_AddItemToObject(cjson_channel, "ImpUnit", cJSON_CreateString(channelParam->strImpUnit));
        cJSON_AddItemToObject(cjson_channel, "ImpValue", cJSON_CreateNumber(channelParam->fImpValue));
        cJSON_AddItemToObject(cjson_channel, "TemperatureCompesate", cJSON_CreateNumber(channelParam->fTemperatureCompesateTemp));
        cJSON_AddItemToArray(cjson_channel_array, cjson_channel);
    }
    char *cjValue = cJSON_Print(cjson_info);
    char out[131072] = { 0 };
    int length = 131072;
    if (strlen(cjValue) < 131072)
        length = strlen(cjValue);
    memcpy(out, cjValue, length);

    if (NULL == cjValue)
    {
        writelog("通道JSON编写失败！\n");
        printf("通道JSON编写失败！\n");
        return ;
    }

    char filename[150];
    memset(filename, 0x00, sizeof(filename));
    sprintf(filename, "/home/admin/mnt/channelInfo.txt");
    FILE *filePtr = fopen(filename, "w");
    if (NULL == filePtr)
    {
        writelog("打开通道文件失败！\n");
        printf("Fail to open channelInfo Write.txt in JsonFileCalibration!\n");
        return ;
    }
    int ret = fputs(out, filePtr);
    if (ret == EOF)
    {
        writelog("写入通道文件失败！\n");
        printf("写入通道文件失败！\n");
        return ;
    }
    fflush(filePtr);
    fsync(fileno(filePtr));
    fclose(filePtr);
    filePtr = NULL;
    cJSON_free(cjValue);
    if (NULL != cjson_info)
    {
        cJSON_Delete(cjson_info);
        cjson_info = NULL;
    }
}
//更新下位机标定参数 目前不用
void writeJsonFileCalib()
{
    // 创建跟对象
    cJSON *cjson_info = cJSON_CreateObject();
    cJSON *cjson_channel_info = cJSON_CreateObject();
    cJSON_AddItemToObject(cjson_info, "cardInfo", cjson_channel_info);
    cJSON *cjson_card_array = cJSON_CreateArray();
    cJSON_AddItemToObject(cjson_channel_info, "cardArray", cjson_card_array);
    for (int i = 0; i < gRunParam.m_nAnalogCards; i++)
    {
        cJSON *cjson_card = cJSON_CreateObject();
        AnalogCardParams *cardParam = gRunParam.m_AnalogCardParams[i];

        cJSON_AddItemToObject(cjson_card, "Card_Name", cJSON_CreateString(cardParam->strCardName));
        cJSON_AddItemToObject(cjson_card, "Card_Index", cJSON_CreateNumber(cardParam->nCardIndex));
        cJSON_AddItemToObject(cjson_card, "Card_Slot", cJSON_CreateNumber(cardParam->nCardSlot));
        cJSON_AddItemToObject(cjson_card, "Card_Channels", cJSON_CreateNumber(cardParam->nCardChannel));
        cJSON_AddItemToObject(cjson_card, "Card_Model", cJSON_CreateString(cardParam->strCardModel));
        cJSON *cjson_acArray = cJSON_CreateArray();
        cJSON_AddItemToObject(cjson_card, "AC_Array", cjson_acArray);
        for (int j = 0; j < cardParam->nCardChannel; j++)
        {
            cJSON *object = cJSON_CreateObject();
            cJSON_AddItemToObject(object, "K", cJSON_CreateNumber(cardParam->k[0][j]));
            cJSON_AddItemToObject(object, "B", cJSON_CreateNumber(cardParam->b[0][j]));
            cJSON_AddItemToArray(cjson_acArray, object);
        }

        cJSON *cjson_icpArray = cJSON_CreateArray();
        for (int j = 0; j < cardParam->nCardChannel; j++)
        {
            cJSON *object = cJSON_CreateObject();
            cJSON_AddItemToObject(object, "K", cJSON_CreateNumber(cardParam->k[1][j]));
            cJSON_AddItemToObject(object, "B", cJSON_CreateNumber(cardParam->b[1][j]));
            cJSON_AddItemToArray(cjson_icpArray, object);
        }
        cJSON_AddItemToObject(cjson_card, "ICP_Array", cjson_icpArray);
        cJSON *cjson_dcArray = cJSON_CreateArray();
        for (int j = 0; j < cardParam->nCardChannel; j++)
        {
            cJSON *object = cJSON_CreateObject();
            cJSON_AddItemToObject(object, "K", cJSON_CreateNumber(cardParam->k[2][j]));
            cJSON_AddItemToObject(object, "B", cJSON_CreateNumber(cardParam->b[2][j]));
            cJSON_AddItemToArray(cjson_dcArray, object);
        }
        cJSON_AddItemToObject(cjson_card, "DC_Array", cjson_dcArray);



        cJSON *cjson_1Array = cJSON_CreateArray();
        for (int j = 0; j < cardParam->nCardChannel; j++)
        {
            cJSON *object = cJSON_CreateObject();
            cJSON_AddItemToObject(object, "K", cJSON_CreateNumber(cardParam->k[3][j]));
            cJSON_AddItemToObject(object, "B", cJSON_CreateNumber(cardParam->b[3][j]));
            cJSON_AddItemToArray(cjson_1Array, object);
        }
        cJSON_AddItemToObject(cjson_card, "Strain_Array", cjson_1Array);



        cJSON *cjson_2Array = cJSON_CreateArray();
        for (int j = 0; j < cardParam->nCardChannel; j++)
        {
            cJSON *object = cJSON_CreateObject();
            cJSON_AddItemToObject(object, "K", cJSON_CreateNumber(cardParam->k[4][j]));
            cJSON_AddItemToObject(object, "B", cJSON_CreateNumber(cardParam->b[4][j]));
            cJSON_AddItemToArray(cjson_2Array, object);
        }
        cJSON_AddItemToObject(cjson_card, "PT_100", cjson_2Array);

        cJSON *cjson_3Array = cJSON_CreateArray();
        for (int j = 0; j < cardParam->nCardChannel; j++)
        {
            cJSON *object = cJSON_CreateObject();
            cJSON_AddItemToObject(object, "K", cJSON_CreateNumber(cardParam->k[5][j]));
            cJSON_AddItemToObject(object, "B", cJSON_CreateNumber(cardParam->b[5][j]));
            cJSON_AddItemToArray(cjson_3Array, object);
        }
        cJSON_AddItemToObject(cjson_card, "Thermocouple_Array", cjson_3Array);

        cJSON_AddItemToArray(cjson_card_array, cjson_card);
    }
    char *cjValue = cJSON_Print(cjson_info);
    if (NULL == cjValue)
    {
        writelog("通道JSON编写失败！\n");
        return ;
    }
    char filename[150];
    memset(filename, 0x00, sizeof(filename));
    sprintf(filename, "/mnt/calibration.txt");
    FILE *filePtr = fopen(filename, "w");
    if (NULL == filePtr)
    {
        writelog("打开标定文件失败！\n");
        printf("Fail to open calibration Write.txt in JsonFileCalibration!\n");
        return ;
    }
    int ret = fputs(cjValue, filePtr);
    if (ret == EOF)
    {
        writelog("写入标定文件失败！\n");
        printf("写入标定文件失败！\n");
        return ;
    }
    fflush(filePtr);
    fsync(fileno(filePtr));
    fclose(filePtr);
    filePtr = NULL;
    cJSON_free(cjValue);
    if (NULL != cjson_info)
    {
        cJSON_Delete(cjson_info);
        cjson_info = NULL;
    }
}

extern char logroad[256]; //本次上电记录路径
static FILE * logfp; //log文件指针
/*
 * 写过程日志文件
 */
void writeHotUpdateInfo()
{
    if (!logfp)
    {
        printf("未打开文件\n");
        return;
    }
    cJSON* root = cJSON_CreateObject();
    cJSON* arr_loop = cJSON_CreateArray();
    for(int loopIndex=0;loopIndex<gRunParam.m_nLoops;loopIndex++)
    {
        LoopParamInfo *lp =gRunParam.m_loopParam[loopIndex];
        cJSON* grp_basic = cJSON_CreateObject();
        cJSON_AddNumberToObject(grp_basic, "loop_index", lp->loop_index);
        cJSON_AddNumberToObject(grp_basic, "enabled", lp->enabled);
        cJSON_AddNumberToObject(grp_basic, "feedback_chnl", lp->feedback_chnl);
        cJSON_AddNumberToObject(grp_basic, "p", lp->pid_p);
        cJSON_AddNumberToObject(grp_basic, "i", lp->pid_i);
        cJSON_AddNumberToObject(grp_basic, "d", lp->pid_d);
        cJSON_AddNumberToObject(grp_basic, "param1", lp->param1);
        cJSON_AddNumberToObject(grp_basic, "param2", lp->param2);
        cJSON_AddNumberToObject(grp_basic, "param3", lp->param3);
        cJSON_AddNumberToObject(grp_basic, "param4", lp->param4);
        cJSON_AddNumberToObject(grp_basic, "active_curve_index", lp->active_curve_index);
        cJSON_AddNumberToObject(grp_basic, "manual_mode", lp->manual_mode);
        cJSON_AddNumberToObject(grp_basic, "manual_output", lp->manual_output);
        cJSON_AddNumberToObject(grp_basic, "di_bool", lp->di_bool);
        cJSON_AddItemToArray(arr_loop,grp_basic);
    }
    cJSON_AddItemToObject(root, "newLoop", arr_loop);


    char *cjValue = cJSON_Print(root);
    char out[131072] = { 0 };
    int length = 131072;
    if (strlen(cjValue) < 131072)
        length = strlen(cjValue);
    memcpy(out, cjValue, length);
    if (NULL == cjValue)
    {
        writelog("信息JSON编写失败！\n");
        printf("信息JSON编写失败！\n");
    }
    int ret = fputs(out, logfp);
    if (ret == EOF)
    {
        writelog("写入通道文件失败！\n");
        printf("写入通道文件失败！\n");
        return -1;
    }
    fflush(logfp);
    fsync(fileno(logfp));
    cJSON_free(cjValue);
    if (NULL != root)
    {
        cJSON_Delete(root);
    }

}
/*
 * 续写过程日志文件
 */
void writeColdUpdateInfo()
{
    closeCurrentInfo();
    // 遍历目录，找出最大的数字序号
    int maxIndex = 0;
    DIR *dir = opendir(logroad);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            const char *name = entry->d_name;
            size_t len = strlen(name);

            // 只处理以 ".txt" 结尾的文件
            if (len <= 4 || strcmp(name + len - 4, ".txt") != 0) continue;
            int i = (int)len - 4;
            char numbuf[32];
            memcpy(numbuf, name, i);
            numbuf[i] = '\0';
            int val = atoi(numbuf);
            if (val > maxIndex)
                maxIndex = val;
        }
        closedir(dir);
    }

    // 计算新的文件名
    int newIndex = maxIndex + 1;
    char logFilePath[256];
    sprintf(logFilePath, "%s/%d.txt", logroad, newIndex);

    // 打开新文件（写模式，创建新文件）
    logfp = fopen(logFilePath, "w");
    if (!logfp)
    {
        printf("创建信息文件失败%s",logFilePath);
        // 打开失败，清理并返回
        logFilePath[0] = '\0';
        return;
    }
    printf("创建信息文件成功%s\n",logFilePath);

    cJSON* root = cJSON_CreateObject();

    cJSON* arr_chnl = cJSON_CreateArray();
    for(int chnlIndex=0;chnlIndex<gRunParam.m_chnldefs.m_nChnls;chnlIndex++)
    {
        ChnlParams *chnl =gRunParam.m_chnldefs.m_chnlParam[chnlIndex];
        cJSON* chnl_basic = cJSON_CreateObject();
        cJSON_AddNumberToObject(chnl_basic, "chnlIndex_device", chnl->chnlIndex_device);
        cJSON_AddNumberToObject(chnl_basic, "chnlIndex_card", chnl->chnlIndex_card);
        cJSON_AddNumberToObject(chnl_basic, "chnl_cardIndex", chnl->chnl_cardIndex);
        cJSON_AddStringToObject(chnl_basic, "strChnlName", chnl->strChnlName);
        cJSON_AddStringToObject(chnl_basic, "strSenserType", chnl->strSenserType);
        cJSON_AddStringToObject(chnl_basic, "strPhysicType", chnl->strPhysicType);
        cJSON_AddNumberToObject(chnl_basic, "fCurrentValue", chnl->fCurrentValue);
        cJSON_AddNumberToObject(chnl_basic, "kCoff", chnl->kCoff);
        cJSON_AddNumberToObject(chnl_basic, "bCoff", chnl->bCoff);
        cJSON_AddNumberToObject(chnl_basic, "kCalib", chnl->kCalib);
        cJSON_AddNumberToObject(chnl_basic, "bCalib", chnl->bCalib);
        cJSON_AddNumberToObject(chnl_basic, "fSensitivity", chnl->fSensitivity);
        cJSON_AddStringToObject(chnl_basic, "strRange", chnl->strRange);
        cJSON_AddNumberToObject(chnl_basic, "fGain", chnl->fGain);
        cJSON_AddNumberToObject(chnl_basic, "bDaqEnable", chnl->bDaqEnable);
        cJSON_AddNumberToObject(chnl_basic, "bStoreEnable", chnl->bStoreEnable);
        cJSON_AddNumberToObject(chnl_basic, "bLimitEnable", chnl->bLimitEnable);

        cJSON_AddNumberToObject(chnl_basic, "nSamplingRate", chnl->nSamplingRate);
        cJSON_AddNumberToObject(chnl_basic, "bStoreEnable", chnl->bStoreEnable);
        cJSON_AddNumberToObject(chnl_basic, "bLimitEnable", chnl->bLimitEnable);

        cJSON_AddItemToArray(arr_chnl,chnl_basic);
    }
    cJSON_AddItemToObject(root, "chnl", arr_chnl);
    cJSON* arr_loop = cJSON_CreateArray();
    for(int loopIndex=0;loopIndex<gRunParam.m_nLoops;loopIndex++)
    {
        LoopParamInfo *lp =gRunParam.m_loopParam[loopIndex];
        cJSON* grp_basic = cJSON_CreateObject();
        cJSON_AddNumberToObject(grp_basic, "loop_index", lp->loop_index);
        cJSON_AddNumberToObject(grp_basic, "enabled", lp->enabled);
        cJSON_AddNumberToObject(grp_basic, "output_chnlIndex", lp->output_chnlIndex);
        cJSON* arr_in = cJSON_CreateArray();
        for (int i = 0; i < lp->n_input_chnls; ++i)
        {
            cJSON_AddItemToArray(arr_in, cJSON_CreateNumber(lp->intput_chnlsIndex[i]));
        }
        cJSON_AddItemToObject(grp_basic, "intput_chnlsIndex", arr_in);

        cJSON_AddNumberToObject(grp_basic, "feedback_chnl", lp->feedback_chnl);
        cJSON_AddNumberToObject(grp_basic, "target_chnl", lp->target_chnl);
        cJSON_AddNumberToObject(grp_basic, "error_chnl", lp->error_chnl);
        cJSON_AddNumberToObject(grp_basic, "other_chnl", lp->other_chnl);
        cJSON_AddStringToObject(grp_basic, "algorithm", lp->algorithm);

        cJSON_AddNumberToObject(grp_basic, "p", lp->pid_p);
        cJSON_AddNumberToObject(grp_basic, "i", lp->pid_i);
        cJSON_AddNumberToObject(grp_basic, "d", lp->pid_d);
        cJSON_AddNumberToObject(grp_basic, "param1", lp->param1);
        cJSON_AddNumberToObject(grp_basic, "param2", lp->param2);
        cJSON_AddNumberToObject(grp_basic, "param3", lp->param3);
        cJSON_AddNumberToObject(grp_basic, "param4", lp->param4);
        cJSON_AddNumberToObject(grp_basic, "voltage_limit", lp->voltage_limit);
        cJSON_AddNumberToObject(grp_basic, "voltage_lowlimit", lp->voltage_lowLimit);

        cJSON* arr_curve1 = cJSON_CreateArray();
        for (int i = 0; i < lp->n_curve1; ++i)
        {
            cJSON* point = cJSON_CreateObject();
            cJSON_AddNumberToObject(point, "time", lp->curve1[i].t);
            cJSON_AddNumberToObject(point, "value", lp->curve1[i].y);
            cJSON_AddStringToObject(point, "unit", lp->curve1[i].unit);
            cJSON_AddItemToArray(arr_curve1,point);
        }
        cJSON_AddItemToObject(grp_basic, "curve1", arr_curve1);

        cJSON* arr_curve2 = cJSON_CreateArray();
        for (int i = 0; i < lp->n_curve2; ++i)
        {
            cJSON* point = cJSON_CreateObject();
            cJSON_AddNumberToObject(point, "time", lp->curve2[i].t);
            cJSON_AddNumberToObject(point, "value", lp->curve2[i].y);
            cJSON_AddStringToObject(point, "unit", lp->curve2[i].unit);
            cJSON_AddItemToArray(arr_curve2,point);
        }
        cJSON_AddItemToObject(grp_basic, "curve2", arr_curve2);

        cJSON_AddNumberToObject(grp_basic, "active_curve_index", lp->active_curve_index);

        cJSON_AddNumberToObject(grp_basic, "condition_coeff", lp->condition_coeff);
        cJSON_AddNumberToObject(grp_basic, "curve1_coeff", lp->curve1_coeff);
        cJSON_AddNumberToObject(grp_basic, "curve2_coeff", lp->curve2_coeff);

        cJSON_AddNumberToObject(grp_basic, "manual_mode", lp->manual_mode);
        cJSON_AddNumberToObject(grp_basic, "manual_output", lp->manual_output);
        cJSON_AddNumberToObject(grp_basic, "di_bool", lp->di_bool);

        cJSON_AddItemToArray(arr_loop,grp_basic);
    }
    cJSON_AddItemToObject(root, "loop", arr_loop);


    char *cjValue = cJSON_Print(root);
    char out[131072] = { 0 };
    int length = 131072;
    if (strlen(cjValue) < 131072)
        length = strlen(cjValue);
    memcpy(out, cjValue, length);

    if (NULL == cjValue)
    {
        writelog("信息JSON编写失败！\n");
        printf("信息JSON编写失败！\n");
        return ;
    }

    if (NULL == logfp)
    {
        writelog("打开通道文件失败！\n");
        printf("Fail to open channelInfo Write.txt in JsonFileCalibration!\n");
        return ;
    }
    int ret = fputs(out, logfp);
    if (ret == EOF)
    {
        writelog("写入通道文件失败！\n");
        printf("写入通道文件失败！\n");
        return ;
    }
    fflush(logfp);
    fsync(fileno(logfp));
    cJSON_free(cjValue);
    if (NULL != root)
    {
        cJSON_Delete(root);
    }

}
void closeCurrentInfo()
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
