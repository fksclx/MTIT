#include "protoHelper.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <math.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>
#include "common.h"
#include "cjson/cJSON.h"

ReplyMsg g_replyMsg;
DataMsg g_datamsg;
ReplyStrm Replystr = REPLY_STRM__INIT;
DataStrm datastr = DATA_STRM__INIT;

DeviceParamsStrm g_deviceParamStr = DEVICE_PARAMS_STRM__INIT;
CalibParamsStrms g_calibParamStr = CALIB_PARAMS_STRMS__INIT;
DeviceStateStrm g_deviceStateStr = DEVICE_STATE_STRM__INIT;
HostCmdProtoInfo g_hostCmdProtoInfo;


int LOOP_TIME_NEED=100;

extern struct RunningParam gRunParam;


static int s_nMsgNo = 0;
static int s_nReplyNo = 0;
static int s_nDataNo = 0;

extern char iffDeviceName[256];


//更新全部通道数据，上位机发下来的
void UpdateChnlParamsStrm(ChnlParams *pchnldef, ChnlParamsStrm *lchnldef)
{
    if (!pchnldef || !lchnldef)
        return;


    strcpy(pchnldef->strChnlName, lchnldef->strchnlname);

    strcpy(pchnldef->strPhysicType, lchnldef->strphysictype);
    strcpy(pchnldef->strSenserType, lchnldef->strsensertype);
    strcpy(pchnldef->strRange, lchnldef->strrange);

    pchnldef->nSamplingRate = atoi(lchnldef->strsamplingrate);
    printf("抄写%s:采样率%d\n",pchnldef->strChnlName,pchnldef->nSamplingRate);

    pchnldef->nFrameCount = (pchnldef->nSamplingRate * LOOP_TIME_NEED) / 1000;

    pchnldef->nFrameSize = pchnldef->nFrameCount * sizeof(float);

    pchnldef->kCoff= lchnldef->kcoff;
    pchnldef->bCoff= lchnldef->bcoff;
    //    //先标定 成对应calib电压
    //    if (strstr(pchnldef->strSenserType, "AC") != NULL)
    //    {
    //        pchnldef->kCalib = gRunParam.m_AnalogCardParams[pchnldef->chnl_cardIndex]->k[0][pchnldef->chnlIndex_card]; //下位机先算
    //        pchnldef->bCalib = gRunParam.m_AnalogCardParams[pchnldef->chnl_cardIndex]->b[0][pchnldef->chnlIndex_card]; //下位机先算
    //    }
    //    else if (strstr(pchnldef->strSenserType, "ICP") != NULL)
    //    {
    //        pchnldef->kCalib = gRunParam.m_AnalogCardParams[pchnldef->chnl_cardIndex]->k[1][pchnldef->chnlIndex_card]; //下位机先算
    //        pchnldef->bCalib = gRunParam.m_AnalogCardParams[pchnldef->chnl_cardIndex]->b[1][pchnldef->chnlIndex_card]; //下位机先算
    //    }
    //    else if (strstr(pchnldef->strSenserType, "DC") != NULL)
    //    {
    //        pchnldef->kCalib = gRunParam.m_AnalogCardParams[pchnldef->chnl_cardIndex]->k[2][pchnldef->chnlIndex_card]; //下位机先算
    //        pchnldef->bCalib = gRunParam.m_AnalogCardParams[pchnldef->chnl_cardIndex]->b[2][pchnldef->chnlIndex_card]; //下位机先算
    //    }
    //    else if (strstr(pchnldef->strSenserType,"应变") !=NULL)
    //    {
    //        pchnldef->kCalib = gRunParam.m_AnalogCardParams[pchnldef->chnl_cardIndex]->k[3][pchnldef->chnlIndex_card]; //下位机先算
    //        pchnldef->bCalib = gRunParam.m_AnalogCardParams[pchnldef->chnl_cardIndex]->b[3][pchnldef->chnlIndex_card]; //下位机先算
    //    }
    //    else if (strstr(pchnldef->strSenserType,"PT") !=NULL)
    //    {
    //        pchnldef->kCalib = gRunParam.m_AnalogCardParams[pchnldef->chnl_cardIndex]->k[4][pchnldef->chnlIndex_card]; //下位机先算
    //        pchnldef->bCalib = gRunParam.m_AnalogCardParams[pchnldef->chnl_cardIndex]->b[4][pchnldef->chnlIndex_card]; //下位机先算
    //    }
    //    else if (strstr(pchnldef->strSenserType,"热电偶") !=NULL)
    //    {
    //        pchnldef->kCalib = gRunParam.m_AnalogCardParams[pchnldef->chnl_cardIndex]->k[5][pchnldef->chnlIndex_card]; //下位机先算
    //        pchnldef->bCalib = gRunParam.m_AnalogCardParams[pchnldef->chnl_cardIndex]->b[5][pchnldef->chnlIndex_card]; //下位机先算
    //    }
    //    else if (strstr(pchnldef->strSenserType, "温度") != NULL)
    //    {
    //        pchnldef->kCalib = gRunParam.m_AnalogCardParams[pchnldef->chnl_cardIndex]->k[1][pchnldef->chnlIndex_card]; //下位机先算
    //        pchnldef->bCalib = gRunParam.m_AnalogCardParams[pchnldef->chnl_cardIndex]->b[1][pchnldef->chnlIndex_card]; //下位机先算
    //    }
    //    else
    //    {
    //        pchnldef->kCalib = 10000; //下位机先算
    //        pchnldef->bCalib = 0; //下位机先算
    //    }

    //    pchnldef->kFinally =  pchnldef->kCalib;
    //    pchnldef->bFinally =  pchnldef->bCalib;

    pchnldef->kFinally =  -1;
    pchnldef->bFinally =  0;

    //再算灵敏度
    pchnldef->fSensitivity = lchnldef->fsensitivity;
    if (((pchnldef->fSensitivity - 0) < 0.000001) || ((pchnldef->fSensitivity - 0) > -0.000001))
        pchnldef->fSensitivity_t = 1.f / lchnldef->fsensitivity; //用倒数避免出发
    else
        pchnldef->fSensitivity_t = 0.f;
    pchnldef->kFinally *= pchnldef->fSensitivity_t;
    pchnldef->bFinally *=  pchnldef->fSensitivity_t;



    pchnldef->kFinally *= pchnldef->kCoff;
    pchnldef->bFinally *= pchnldef->kCoff;

    pchnldef->bFinally += pchnldef->bCoff;


    strcpy(pchnldef->strLowFilter, lchnldef->strlowfilter);

    strcpy(pchnldef->strBridgeMode, lchnldef->strbridgemode);

    float value;

    if(strcmp(lchnldef->strbridgeresistance,"1kohm")==0)
        pchnldef->fBridgeResistance = 1000;
    else if(strcmp(lchnldef->strbridgeresistance,"350ohm")==0)
        pchnldef->fBridgeResistance = 350;
    else if(strcmp(lchnldef->strbridgeresistance,"120ohm")==0)
        pchnldef->fBridgeResistance = 120;


    strcpy(pchnldef->strImpUnit, lchnldef->strimpunit);
    value = strtof(lchnldef->strimpvalue, NULL);
    pchnldef->fImpValue = value;
    strcpy(pchnldef->strImpValue, lchnldef->strimpvalue);


    //  printf("pchnldef->shuntCalCorr %f\n",pchnldef->shuntCalCorr);
    //  printf("pchnldef->shuntCalStandardStrain %f %f\n",pchnldef->shuntCalStandardStrain,vs);

    strcpy(pchnldef->strUnit, lchnldef->strunit);

    pchnldef->bDaqEnable = lchnldef->bdaqenable;
    pchnldef->bStoreEnable = lchnldef->bstoreenable;
    pchnldef->bLimitEnable = lchnldef->blimitenable;

    //  printf("update :::%s  %s   %s   %s %s \n ", pchnldef->strRange, pchnldef->strImpUnit,pchnldef->strBridgeResistance,pchnldef->strBridgeMode,pchnldef->strLowFilter);

}
void CleanLoops()
{
    if (gRunParam.m_loopParam != NULL) {
        // 释放每个结构体
        for (int i = 0; i < gRunParam.m_nLoops; i++)
        {
            if (gRunParam.m_loopParam[i] != NULL) {
                free(gRunParam.m_loopParam[i]);
                gRunParam.m_loopParam[i] = NULL; // 防止悬空指针
            }
        }
        // 释放指针数组
        free(gRunParam.m_loopParam);
        gRunParam.m_loopParam = NULL; // 防止悬空指针
    }
    gRunParam.m_nLoops = 0; // 重置循环数量
}
//收到上位机新param
//传参会被析构，不能直接等
int UpdateRunParam(CMDStrm *CmdMsg)
{
    if (CmdMsg == NULL)
        return -1;

    gRunParam.m_state.runId = CmdMsg->ncmdno;
    if (CmdMsg->has_daqstarttime)
        gRunParam.nStartDaqTime = CmdMsg->daqstarttime;
    if (CmdMsg->has_savestarttime)
        gRunParam.nStartSaveTime = CmdMsg->savestarttime;
    if (CmdMsg->has_savestartdelay)
        gRunParam.nSaveTimeDelay = CmdMsg->savestartdelay;
    else if (CmdMsg->has_savestarttime && gRunParam.nStartDaqTime > 0)
        gRunParam.nSaveTimeDelay = (CmdMsg->savestarttime - gRunParam.nStartDaqTime) / 1000;
    if (CmdMsg->strsavefilename != NULL)
        strcpy(gRunParam.strSaveName, CmdMsg->strsavefilename);

    //虚拟通道
    if (CmdMsg->virtual_channels!= NULL)
    {
        gRunParam.m_chnldefs.m_nVirtualChnls=CmdMsg->n_virtual_channels;
    }
    readDeviceChannels();
    if (CmdMsg->virtual_channels!= NULL)
    {
        for (size_t i = 0; i < CmdMsg->n_virtual_channels; i++)
        {
            ChnlParams *pchnldef =gRunParam.m_chnldefs.m_VirtualChnlParam[i];
            VirtualChannelStrm *lchnldef = CmdMsg->virtual_channels[i];
            if (lchnldef == NULL || pchnldef == NULL)
            {
                continue;
            }
            if(pchnldef->chnlIndex_device!=lchnldef->id)
            {
                printf("虚拟通道号对不上！本机:%d 上位机:%d\n",pchnldef->chnlIndex_device,lchnldef->id);
            }
            strcpy(pchnldef->strChnlName,lchnldef->name);
            strcpy(pchnldef->vparams.formula_text,lchnldef->formula_text);
            pchnldef->vparams.n_count = lchnldef->n_operands;
            for(size_t y=0;y<lchnldef->n_operands;y++)
            {
                pchnldef->vparams.vars[y].name = (char *)malloc(10 * sizeof(char)); // 为名称分配内存
                strcpy(pchnldef->vparams.vars[y].name,lchnldef->operands[y]->symbol);
                if(lchnldef->operands[y]->chnl_index_device >= gRunParam.m_chnldefs.m_nChnls)
                {
                    printf("虚拟通道号超出实际范围！\n");
                    continue;
                }
                printf("实际通道号%d \n",lchnldef->operands[y]->chnl_index_device);
                pchnldef->vparams.vars[y].address=&gRunParam.m_chnldefs.m_chnlParam[lchnldef->operands[y]->chnl_index_device]->fCurrentValue;
            }
        }
    }

    if (CmdMsg->params != NULL)
    {
        if(gRunParam.m_chnldefs.m_nChnls<CmdMsg->params->n_chnldef)
        {
            printf("下发参数数量不对！%d %d\n",gRunParam.m_chnldefs.m_nChnls,CmdMsg->params->n_chnldef);
            return -1;
        }
        for (size_t i = 0; i < CmdMsg->params->n_chnldef; i++)
        {
            ChnlParams *pchnldef =gRunParam.m_chnldefs.m_chnlParam[i];
            ChnlParamsStrm *lchnldef = CmdMsg->params->chnldef[i];
            if (lchnldef == NULL || pchnldef == NULL)
            {
                continue;
            }
            UpdateChnlParamsStrm(pchnldef, lchnldef);
        }
        writeJsonFileChannel(); //写本地通道参数
    }
    refreshCardSampleRate();
    CleanLoops();
    if (CmdMsg->loop_params != NULL)
    {
        //1.清空历史loop
        //2.创建新的loop
        //3.拷贝全部参数
        gRunParam.m_nLoops=CmdMsg->n_loop_params;
        printf("有loop 个数:%d\n",gRunParam.m_nLoops);
        gRunParam.m_loopParam = (struct LoopParamInfo **)malloc(gRunParam.m_nLoops * sizeof(struct LoopParamInfo *));
        for(int i=0;i<gRunParam.m_nLoops;i++)
        {
            LoopParamsStrm *loopdef = CmdMsg->loop_params[i];
            gRunParam.m_loopParam[i] = (struct LoopParamInfo *)malloc(sizeof(struct LoopParamInfo));
            LoopParamInfo *ploopdef = gRunParam.m_loopParam[i];
            ploopdef->active_curve_index=loopdef->active_curve_index;
            strcpy(ploopdef->algorithm,loopdef->algorithm);
            ploopdef->manual_mode=loopdef->manual_mode;
            ploopdef->manual_output=(float)loopdef->manual_output_value;

            ploopdef->pid_p=loopdef->pid_p;
            ploopdef->pid_i=loopdef->pid_i;
            ploopdef->pid_d=loopdef->pid_d;
            ploopdef->param1=loopdef->param1;
            ploopdef->param2=loopdef->param2;
            ploopdef->param3=loopdef->param3;
            ploopdef->param4=loopdef->param4;

            ploopdef->error_chnl=loopdef->error_chnl;
            ploopdef->other_chnl=loopdef->qr_chnl;

            ploopdef->condition_coeff=loopdef->condition_coeff;
            ploopdef->curve1_coeff=loopdef->curve1_coeff;
            ploopdef->curve2_coeff=loopdef->curve2_coeff;

            ploopdef->voltage_limit=loopdef->voltage_limit;
            ploopdef->voltage_lowLimit=loopdef->voltage_low_limit;

            ploopdef->loop_index=loopdef->loop_index;
            ploopdef->enabled=loopdef->enabled;
            ploopdef->output_chnlIndex=loopdef->output_chnl;
            ploopdef->target_chnl=loopdef->target_chnl;//注意改成抄写！
            ploopdef-> feedback_chnl=loopdef->feedback_chnl;

            ploopdef->n_input_chnls=loopdef->n_acquire_chnls;
            for(int y=0;y<ploopdef->n_input_chnls;y++)
            {
                ploopdef->intput_chnlsIndex[y]=loopdef->acquire_chnls[y];
            }
            //放置曲线
            ploopdef-> n_curve1=loopdef->n_curve1;
            for(int y=0;y<ploopdef->n_curve1;y++)
            {
                ploopdef->curve1[y].index=loopdef->curve1[y]->index;
                ploopdef->curve1[y].t=loopdef->curve1[y]->t;
                ploopdef->curve1[y].y=loopdef->curve1[y]->y;
                strcpy(ploopdef->curve1[y].unit,loopdef->curve1[y]->unit);
            }
            ploopdef-> n_curve2=loopdef->n_curve2;
            for(int y=0;y<ploopdef->n_curve2;y++)
            {
                ploopdef->curve2[y].index=loopdef->curve2[y]->index;
                ploopdef->curve2[y].t=loopdef->curve2[y]->t;
                ploopdef->curve2[y].y=loopdef->curve2[y]->y;
                strcpy(ploopdef->curve2[y].unit,loopdef->curve2[y]->unit);
            }

            ploopdef-> di_bool=loopdef->di_bool;
            // printf("回路参数：采集通道1:%d,采集通道2:%d,采集通道3:%d,采集通道4:%d,反馈通道:%d,目标通道:%d,误差通道:%d,热流通道:%d\n")
            printf("回路参数：采集通道个数1:%d,反馈通道:%d,目标通道:%d,误差通道:%d,热流通道:%d\n",ploopdef->n_input_chnls,ploopdef-> feedback_chnl,ploopdef->target_chnl,ploopdef->error_chnl,ploopdef->other_chnl);

        }
    }
    //  apply_calibs_to_cards(CmdMsg->calibs);
    return 0;
}

int UpdateHotRunParam(CMDStrm *CmdMsg)
{
    if (CmdMsg == NULL)
        return -1;
    if (CmdMsg->hot_loop_params != NULL)
    {
        printf("有loop 个数:%d %d\n",gRunParam.m_nLoops,CmdMsg->n_hot_loop_params);
        for(int i=0;i<gRunParam.m_nLoops;i++)
        {
            HotLoopParamsStrm *loopdef = CmdMsg->hot_loop_params[i];
            LoopParamInfo *ploopdef = gRunParam.m_loopParam[i];

            ploopdef->active_curve_index=loopdef->active_curve_index;
            ploopdef->manual_mode=loopdef->manual_mode;
            ploopdef->manual_output=(float)loopdef->manual_output_value;
            ploopdef->pid_p=loopdef->pid_p;
            ploopdef->pid_i=loopdef->pid_i;
            ploopdef->pid_d=loopdef->pid_d;
            ploopdef->param1=loopdef->param1;
            ploopdef->param2=loopdef->param2;
            ploopdef->param3=loopdef->param3;
            ploopdef->param4=loopdef->param4;
            ploopdef-> feedback_chnl=loopdef->feedback_chnl;
            ploopdef-> di_bool=loopdef->di_bool;
        }
    }
    //  apply_calibs_to_cards(CmdMsg->calibs);
    return 0;
}
int UpdateLgorithm(CMDStrm *CmdMsg)
{
    if (CmdMsg == NULL)
        return;
    if (CmdMsg->algorithm_library != NULL)
    {
        int count = CmdMsg->n_algorithm_library;
        for(int i=0;i<count;i++)
        {
            AlgorithmDeployStrm* strm = CmdMsg->algorithm_library[i];
            AlgorithmConstants *ploopdef = &gRunParam.algorithmConstans[i];
            ploopdef->duInit=strm->du_init;
            ploopdef->tThreshold=strm->t_threshold;
            ploopdef->outputInit=strm->output_init;
            ploopdef->duDivP=strm->du_div_p;
            ploopdef->duDivI=strm->du_div_i;
            ploopdef->duDiffCoeff=strm->du_diff_coeff;
            ploopdef->pv1LowLimit=strm->pv1_out;
            ploopdef->hwOffset=strm->hw_offset;
            ploopdef->hwCoeff=strm->hw_coeff;
            printf("算法名称:%s\n",strm->algorithm_name);
        }
    }
    return 0;
}
//删除通道参数
void DeleteChnlParams(ChnlParamsStrm *lchnldef)
{
    free(lchnldef->strchnlname);
    free(lchnldef->strsensertype);
    free(lchnldef->strphysictype);
    free(lchnldef->strrange);
    free(lchnldef->strunit);
    free(lchnldef->strlowfilter);
    free(lchnldef->strbridgemode);
    free(lchnldef->strbridgeresistance);
    free(lchnldef->strsamplingrate);
    free(lchnldef->strimpvalue);
    free(lchnldef->strimpunit);
    free(lchnldef->strdioaction);
    free(lchnldef);
    lchnldef = NULL;
}




//解压缩头部
int headerDecode(Header *head, uint8_t *buf, int len)
{
    if (len < sizeof(Header))
    {
        return -1;
    }
    Header *pHeader = (Header*) buf;
    strncpy(head->m_nProtocolType, pHeader->m_nProtocolType, 4);
    head->m_nVersion = pHeader->m_nVersion;
    head->m_nMsgType = pHeader->m_nMsgType;
    head->m_nMsgNo = pHeader->m_nMsgNo;
    head->m_nTypeNo = pHeader->m_nTypeNo;
    return sizeof(Header);
}
//打包回复
int ReplyEncode(int replytype, int cmdNo, uint8_t *buf)
{
    g_replyMsg.m_pReplyStrm = &Replystr;
    strncpy(g_replyMsg.m_header.m_nProtocolType, PROTOCTYPE, 4);
    g_replyMsg.m_header.m_nVersion = PROTOCVER;
    g_replyMsg.m_header.m_nMsgType = MSG_REPLY;
    g_replyMsg.m_header.m_nMsgNo = ++s_nMsgNo;
    g_replyMsg.m_header.m_nTypeNo = ++s_nReplyNo;
    g_replyMsg.m_pReplyStrm->ncmdno = cmdNo;
    g_replyMsg.m_pReplyStrm->nreplyno = s_nReplyNo;
    g_replyMsg.m_pReplyStrm->nreplytype = replytype;
    g_replyMsg.m_pReplyStrm->ndeviceid = 0;
    g_replyMsg.m_pReplyStrm->nreplytime = (uint32_t) GetTimeStampU() * 0.000001;

    if (DEVICEPARAM == replytype || UPDATEFAIL == replytype || UPDATESUCCESS == replytype)
    {
        //初始化和更新后自动更新这个结构体
        g_replyMsg.m_pReplyStrm->params = &g_deviceParamStr;

    }
    else
    {
        g_replyMsg.m_pReplyStrm->params = NULL;
    }

    if (DEVICESTATE == replytype) //设备状态
    {
        g_deviceStateStr.daqstate = gRunParam.m_state.isSampling;
        g_deviceStateStr.savestate = gRunParam.m_state.isSaving;
        g_replyMsg.m_pReplyStrm->state = &g_deviceStateStr;
    }
    else if (UPDATESUCCESS== replytype)
    {
        printf("UPDATESUCCESS reply\n");
        g_deviceStateStr.planstate = 1;
        g_replyMsg.m_pReplyStrm->state = &g_deviceStateStr;
    }
    else if (UPDATEFAIL == replytype) //设备状态
    {
        printf("UPDATEFAIL reply\n");
        g_deviceStateStr.planstate = 0;
        g_replyMsg.m_pReplyStrm->state = &g_deviceStateStr;
    }
    else if (OCCUPY_REPLY == replytype) //设备状态
    {
        g_deviceStateStr.planstate = gRunParam.m_state.occupy_reply;
        g_replyMsg.m_pReplyStrm->state = &g_deviceStateStr;
    }
    else if (SYNCTIMEFAIL == replytype)
    {
        g_deviceStateStr.synctime = GetTimeStampU() / 1000;
        g_replyMsg.m_pReplyStrm->state = &g_deviceStateStr;
    }
    else if(SWITCHMODEFAIL == replytype || SWITCHMODESUCESS == replytype)
    {

    }
    else if (CHANGEELICSUCCESS == replytype || CHANGEELICFAIL == replytype)
    {
    }
    else if (ELEICSTATSUCCESS == replytype || ELEICSTATFAIL == replytype)
    {
        g_replyMsg.m_pReplyStrm->state = &g_deviceStateStr;

    }
    else
    {
        g_replyMsg.m_pReplyStrm->state = NULL;
    }

    g_replyMsg.m_pReplyStrm->n_chnlvalue = 0;
    g_replyMsg.m_pReplyStrm->chnlvalue = NULL;

    memcpy(buf, &(g_replyMsg.m_header), sizeof(g_replyMsg.m_header));
    int temlen = reply_strm__pack(g_replyMsg.m_pReplyStrm, (uint8_t*) (buf + sizeof(struct Header)));
    return temlen + sizeof(struct Header);
}
void CopyChnlParamsToData(ChnlParamsStrm **des, ChnlParams *src)
{
    if (!des || !src)
    {
        printf("CopyChnlParamsToData ERROR!\n");
        return;
    }
    if (*des)
    {
        chnl_params_strm__free_unpacked(*des, NULL);
        *des = NULL;
    }
    *des = (ChnlParamsStrm*) malloc(sizeof(ChnlParamsStrm));
    chnl_params_strm__init(*des);

    ChnlParamsStrm *lchnldef = *des;
    ChnlParams *pchnldef = src;

    lchnldef->chnlindex_device = pchnldef->chnlIndex_device;
    lchnldef->chnlindex_card = pchnldef->chnlIndex_card;
    lchnldef->chnl_cardindex = pchnldef->chnl_cardIndex;

    lchnldef->strchnlname = (char*) malloc(128);
    strncpy(lchnldef->strchnlname, pchnldef->strChnlName, strlen(pchnldef->strChnlName));
    lchnldef->strchnlname[strlen(pchnldef->strChnlName)] = '\0';

    lchnldef->strsensertype = (char*) malloc(64);
    strncpy(lchnldef->strsensertype, pchnldef->strSenserType, strlen(pchnldef->strSenserType));
    lchnldef->strsensertype[strlen(pchnldef->strSenserType)] = '\0';

    lchnldef->strphysictype = (char*) malloc(64);
    strncpy(lchnldef->strphysictype, pchnldef->strPhysicType, strlen(pchnldef->strPhysicType));
    lchnldef->strphysictype[strlen(pchnldef->strPhysicType)] = '\0';

    lchnldef->strlowfilter = (char*) malloc(64);
    strncpy(lchnldef->strlowfilter, pchnldef->strLowFilter, strlen(pchnldef->strLowFilter));
    lchnldef->strlowfilter[strlen(pchnldef->strLowFilter)] = '\0';

    lchnldef->strbridgemode = (char*) malloc(64);
    strncpy(lchnldef->strbridgemode, pchnldef->strBridgeMode, strlen(pchnldef->strBridgeMode));
    lchnldef->strbridgemode[strlen(pchnldef->strBridgeMode)] = '\0';

    lchnldef->strbridgeresistance = (char*) malloc(64);
    memset(lchnldef->strbridgeresistance, 0, 64);


    if( pchnldef->fBridgeResistance == 1000)
        strcpy(lchnldef->strbridgeresistance,"1kohm");
    else if( pchnldef->fBridgeResistance == 350)
        strcpy(lchnldef->strbridgeresistance,"350ohm");
    else  if( pchnldef->fBridgeResistance == 120)
        strcpy(lchnldef->strbridgeresistance,"120ohm");


    lchnldef->strimpunit = (char*) malloc(64);
    strncpy(lchnldef->strimpunit, pchnldef->strImpUnit, strlen(pchnldef->strImpUnit));
    lchnldef->strimpunit[strlen(pchnldef->strImpUnit)] = '\0';

    lchnldef->strimpvalue = (char*) malloc(64);
    memset(lchnldef->strimpvalue, 0, 64);
    sprintf(lchnldef->strimpvalue, "%g", pchnldef->fImpValue);

    //lchnldef->ftemperaturecompesate=pchnldef->fTemperatureCompesateTemp;

    lchnldef->kcoff = pchnldef->kCoff;
    lchnldef->bcoff = pchnldef->bCoff;
    lchnldef->fsensitivity = pchnldef->fSensitivity;

    lchnldef->strrange = (char*) malloc(64);
    strncpy(lchnldef->strrange, pchnldef->strRange, strlen(pchnldef->strRange));
    lchnldef->strrange[strlen(pchnldef->strRange)] = '\0';

    lchnldef->strunit = (char*) malloc(16);
    strncpy(lchnldef->strunit, pchnldef->strUnit, strlen(pchnldef->strUnit));
    lchnldef->strunit[strlen(pchnldef->strUnit)] = '\0';

    lchnldef->bdaqenable = pchnldef->bDaqEnable;
    lchnldef->bstoreenable = pchnldef->bStoreEnable;
    lchnldef->blimitenable = pchnldef->bLimitEnable;

    lchnldef->strsamplingrate = (char*) malloc(64);
    memset(lchnldef->strsamplingrate, 0, 64);
    sprintf(lchnldef->strsamplingrate, "%d", pchnldef->nSamplingRate);

}
/*******************************************************************************************************************/
/*
 * 初始化datamsg,用于给上位机传递实时通道数据
 */
void InitDataMsg()
{
    g_datamsg.m_pDataStrm = &datastr;
    strncpy(g_datamsg.m_header.m_nProtocolType, PROTOCTYPE, 4);
    g_datamsg.m_header.m_nVersion = PROTOCVER;
    g_datamsg.m_header.m_nMsgType = MSG_DATA;
    datastr.ndatano = s_nDataNo;
    datastr.ndatatype = DATA_SAMPLING;
    datastr.ndeviceid = 0;
    datastr.nchnls = 0;
    datastr.n_chnldef = 0;
    datastr.chnldef = NULL;
    datastr.points.data = NULL;
    datastr.points.len = 0;

    datastr.n_ndataformat = 0;
    datastr.ndataformat = NULL;
}
/*
 * 析构datamsg，每次变采样要用
 */
void DeInitDataMsg()
{
    if (datastr.ndaqtime != NULL)
    {
        free(datastr.ndaqtime);
        datastr.n_ndaqtime = 0;
        datastr.ndaqtime = NULL;
    }
    if (datastr.nframesize != NULL)
    {
        free(datastr.nframesize);
        datastr.n_nframesize = 0;
        datastr.nframesize = NULL;
    }
    if (datastr.ndataformat != NULL)
    {
        free(datastr.ndataformat);
        datastr.n_ndataformat = 0;
        datastr.ndataformat = NULL;
    }
    if (datastr.chnldef != NULL)
    {
        for (int i = 0; i < datastr.n_chnldef; ++i)
        {
            DeleteChnlParams(datastr.chnldef[i]);
        }
        free(datastr.chnldef);
        datastr.n_chnldef = 0;
        datastr.chnldef = NULL;
    }
    if (datastr.points.data != NULL)
    {
        free(datastr.points.data);
        datastr.points.len = 0;
        datastr.points.data = NULL;
    }
}
/*
 * 更新传递数据包信息，每次更新通道参数后需要修改
 */
static int rx_data_len = 0;
void UpdateDataMsg()
{
    DeInitDataMsg(); //先析构
    g_datamsg.m_pDataStrm = &datastr;
    strncpy(g_datamsg.m_header.m_nProtocolType, PROTOCTYPE, 4);
    g_datamsg.m_header.m_nVersion = PROTOCVER;
    g_datamsg.m_header.m_nMsgType = MSG_DATA;
    s_nDataNo = 0;
    s_nMsgNo = 0;
    s_nReplyNo = 0;
    datastr.ndatano = s_nDataNo;
    datastr.ndatatype = DATA_SAMPLING;
    datastr.ndeviceid = 0;

    // int allChnl = gRunParam.m_chnldefs.m_nAnalogChnls+gRunParam.m_chnldefs.m_nDigitalChnls+gRunParam.m_chnldefs.m_nIOChnls;//双倍通道
    int allChnl = gRunParam.m_chnldefs.m_nChnls;
    datastr.nchnls = allChnl;
    datastr.n_chnldef = allChnl;
    datastr.chnldef = (ChnlParamsStrm**) malloc(allChnl * sizeof(ChnlParamsStrm*));
    memset(datastr.chnldef, 0x00,allChnl * sizeof(ChnlParamsStrm*));

    datastr.n_ndaqtime = allChnl;
    datastr.ndaqtime = (int64_t*) malloc(datastr.n_ndaqtime * sizeof(int64_t));
    memset(datastr.ndaqtime, 0x00, datastr.n_ndaqtime * sizeof(int64_t));

    datastr.n_nframesize = allChnl;
    datastr.nframesize = (int32_t*) malloc(datastr.n_nframesize * sizeof(int32_t));
    memset(datastr.nframesize, 0x00, datastr.n_nframesize * sizeof(int32_t));

    datastr.n_ndataformat = allChnl;
    datastr.ndataformat = (int32_t*) malloc(datastr.n_ndataformat * sizeof(int32_t));
    memset(datastr.ndataformat, 0x00, datastr.n_ndataformat * sizeof(int32_t));

    rx_data_len = 0;
    int chnlIndex=0;
    //注意，开辟了全部的通道

    for (int i = 0; i < gRunParam.m_chnldefs.m_nChnls; i++,chnlIndex++)
    {
        CopyChnlParamsToData(&datastr.chnldef[chnlIndex], gRunParam.m_chnldefs.m_chnlParam[i]);
        datastr.ndataformat[i] = 1;
        rx_data_len += gRunParam.m_chnldefs.m_chnlParam[i]->nFrameSize;
    }

    //jw
    datastr.points.data = (uint8_t*) malloc(rx_data_len);
    datastr.points.len = rx_data_len;
}
/*
 * 反馈给上位机 实时数据通道 打包数据流
 */
static int32_t frameCount[MAX_CHANNEL]; //点数
static int64_t frameIndex[MAX_CHANNEL]; //索引
int DataEncode(uint8_t *buf, char *src)
{
    memcpy(&frameCount[0], &src[0], MAX_CHANNEL * 4);
    memcpy(&frameIndex[0], &src[MAX_CHANNEL * 4], MAX_CHANNEL * 8);

    g_datamsg.m_pDataStrm = &datastr;
    g_datamsg.m_header.m_nMsgNo = ++s_nMsgNo;
    g_datamsg.m_header.m_nTypeNo = ++s_nDataNo;
    g_datamsg.m_pDataStrm->ndatano = gRunParam.m_state.runId;
    g_datamsg.m_pDataStrm->nstartdaqtime = gRunParam.nStartDaqTime;
    uint8_t *des = g_datamsg.m_pDataStrm->points.data;
    memset(des, 0, rx_data_len);
    uint32_t appendLen = 0;
    uint32_t appendCount = MAX_CHANNEL * 4 + MAX_CHANNEL * 8;

    //还有优化空间，process给出来直接打包好，作一个全长拷贝

    int chnlIndex=0;
    for (int i = 0; i < gRunParam.m_chnldefs.m_nChnls; i++)
    {
        ChnlParams *info = gRunParam.m_chnldefs.m_chnlParam[i];
        if (info->bDaqEnable == 0)
        {
            appendCount += info->nFrameSize; //跨越过去
            frameCount[chnlIndex] = 0;
            chnlIndex++;
            continue;
        }
        //这里先拷贝了全数据，最终记得扔掉数
        memcpy(des, &src[appendCount], info->nFrameSize);
        frameCount[chnlIndex] = info->nFrameCount;
        appendCount += info->nFrameSize; //用于找位置
        appendLen += info->nFrameSize; //总长度 也用nframesize
        des += info->nFrameSize; //位移也用nFrameSize 中间不满就不满
        chnlIndex++;
    }
    g_datamsg.m_pDataStrm->points.len = appendLen;
    memcpy(g_datamsg.m_pDataStrm->ndaqtime, &frameIndex[0], g_datamsg.m_pDataStrm->n_ndaqtime * 8);
    memcpy(g_datamsg.m_pDataStrm->nframesize, &frameCount[0], g_datamsg.m_pDataStrm->n_nframesize * 4);
    //printf("%lld %lld %lld \n",frameIndex[0],frameIndex[1],frameIndex[2]);
    int headlen = sizeof(Header);
    memcpy(buf, &g_datamsg.m_header, headlen);
    int temlen = data_strm__pack(g_datamsg.m_pDataStrm, (uint8_t*) (buf + headlen));
    return temlen + sizeof(struct Header);
}

