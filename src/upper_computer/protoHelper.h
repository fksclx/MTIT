#pragma once
#include "Daq_V0.7.pb-c.h"
#include "structDefine.h"



#define PROTOCTYPE "DYCP"
#define PROTOCVER 01
enum MSGTYPE
{
    MSG_UNDEFINE, MSG_CMD, MSG_REPLY, MSG_DATA
};

enum DATATYPE
{
    DATA_UNDEFINE, DATA_SAMPLING
};

typedef struct Header
{
    char m_nProtocolType[4];
    uint32_t m_nVersion;
    uint32_t m_nMsgType;
    uint32_t m_nMsgNo;
    uint32_t m_nTypeNo;
} Header;

typedef struct DataProperties
{
    unsigned int nDataNo;
    unsigned int nDataType;
    unsigned int nDeviceID;
    unsigned int nDaqTime;
    unsigned int nChnls;
} DataProperties;

typedef struct DataBody
{
    unsigned int len;
    float *data;
} DataBody;
typedef struct ReplyMsg
{
    Header m_header;
    ReplyStrm *m_pReplyStrm;
} ReplyMsg;

typedef struct DataMsg
{
    Header m_header;
    struct DataProperties *m_prop;
    struct DataBody *m_body;
    DataStrm *m_pDataStrm;
} DataMsg;

typedef struct ProtoChnlIndexInfo
{
    int chnlIndex_device;
    int chnlIndex_card;
    int chnl_cardIndex;
} ProtoChnlIndexInfo;



typedef struct ProtoAlgorithmDeployInfo
{
    char algorithm_name[128];
    char template_id[128];
    int target_loop_index;
    double t_threshold;
    double du_init;
    double output_init;
    double du_div_p;
    double du_div_i;
    double du_diff_coeff;
} ProtoAlgorithmDeployInfo;

typedef struct ProtoCalibParamInfo
{
    int chnlIndex_device;
    int chnlIndex_card;
    int chnl_cardIndex;
    char strChnlName[128];
    float AC_kCalib;
    float AC_bCalib;
    float DC_kCalib;
    float DC_bCalib;
    float ICP_kCalib;
    float ICP_bCalib;
} ProtoCalibParamInfo;

typedef struct HostCmdProtoInfo
{
    uint32_t nCMDNo;
    uint32_t nCMDType;
    uint32_t nMasterID;
    uint32_t nDeviceID;
    int64_t nExcuteTime;

    int n_zeroChannel;
    int32_t *zeroChannel;
    int n_shuntChannel;
    int32_t *shuntChannel;
    int n_averageChannel;
    int32_t *averageChannel;
    int64_t syncTime;
    int64_t planStartTime;
    int64_t planStopTime;
    int64_t preSaveTime;
    int n_calibs;
    ProtoCalibParamInfo *calibs;
    char strSaveFileName[256];
    int64_t daqStartTime;
    int64_t saveStartTime;
    int64_t saveStartDelay;
    int localClock;
    char OperatingMode[128];
    int n_loops;
    LoopParamInfo *loops;
    ProtoAlgorithmDeployInfo algorithm_deploy;
    int n_algorithms;
    ProtoAlgorithmDeployInfo *algorithms;
    char active_template_id[128];
    int update_param_mode;
} HostCmdProtoInfo;

extern HostCmdProtoInfo g_hostCmdProtoInfo;

void ClearHostCmdProtoInfo(HostCmdProtoInfo *info);
int UpdateRunParam(CMDStrm *CmdMsg);
int UpdateHotRunParam(CMDStrm *CmdMsg);

enum CMDTYPE
{
    UNDEFINECMD,        // 未知
    UPDATEPARAM,        // 参数更新(1)
    GETPARAM,           // 获取参数(2)
    GETSTATE,           // 获取状态(3)
    STARTDAQ,           //开始采集
    STOPDAQ,            //停止采集
    STARTSAVE,          //开始存储
    STOPSAVE,           //停止存储
    DELETEFILE,         //删除文件
    SYNCTIME,           //同步时间
    SENDPLAN,           //下发计划
    ECHANGELOCAL,       //切换地电
    ECHANGEREMOTE,      //切换弹电
    WRITESPI,           //写入spi
    CANCELPLAN,         //取消计划
    OPENLIMIT,          //下发超限开关
    CANCELLIMIT,        //下下发超限开关
    UPDATECALIB,        //更新标定参数
    GETCALIB,           //获取标定参数
    CLEARDATA,
    CCHANGELOCAL,       //切换到本地时钟
    CCHANGEEXTERNAL,    //切换到同步器时钟
    SHUNTERRORCALC,
    WIRERESISTANCECALC,
    ZEROADJUST,
    ZEROADJUSTRESET,
    WIRERESISTANCESET,
    WIRERESISTANCECALCRESET,
    OCCUPY_DEVICE,
    ResumeConnect,
    ResetOccupy,
    RESUMEDATASEND,
    UPDATESHUNTCALIB,
    GETSHUNTCALIB,
    AUTOSHUNTCALIB,
    UPDATEALGORITHM,
    TOTALINTERLOCK_OPEN, // 总闭锁开（lock=1）
    TOTALINTERLOCK_CLOSE, // 总闭锁关（lock=0）
    HOTUPDATEPARAM, // 试验中热更新回路参数（HotLoopParamsStrm）
    STARTTEST, // 开始试验（控制曲线输出，区别于 STARTDAQ）
    PAUSETEST, // 暂停试验（停止驱动输出，采集继续）
    STOPTEST, // 停止试验（停止驱动输出并切回 Random）
    DISKSPACE,//硬盘查询
    CMDCOUNT
};
enum REPLYTYPE
{
    CMDNOTVALID,
    DEVICESTATE,            //设备状态 首次发送
    DEVICEPARAM,            //设备参数（通道、设备）
    DAQSTARTED,             //采集开始
    DAQSTOPED,              //采集结束
    DEVICEDAQSTOPED,        //采集设备意外停止
    DAQPIPE_FULL,           //采集FIFO写满
    DAQSAMPLEFALSE,         //单次采集遇到错误
    SAVESTARTED,            //存储开始
    DISKFULL,               //存储已满
    SAVESTOPED,             //存储结束
    PLANACCEPTED,           //计划开始执行
    PLANUNACCEPTED,         //计划有误,已有计划或计划时间不对
    PLANSTARTED,            //计划开始执行
    PLANSTOPED,             //计划停止
    PLANCANCELED,           //计划取消
    PLANERROR,              //计划意外
    SYNCTIMESUCCESS,        //同步时间成功
    SYNCTIMEFAIL,           //同步时间失败，回复带当前时间
    DAWRITESUCCESS,         //修改DA指令成功
    DAWRITEFAIL,            //修改DA失败，原因为无对应da通道或未开始采集
    CHANGEELICSUCCESS,      //切电成功，需要回复当前设备状态
    CHANGEELICFAIL,         //切电失败
    ELEICSTATSUCCESS,       //查询切电结果成功
    ELEICSTATFAIL,          //查询切电结果失败
    UPDATESUCCESS,          //更新参数成功
    UPDATEFAIL,             //更新参数失败
    MAINPIPE_FULL,          //FIFO写满
    SAVEPIPE_FULL,          //FIFO写满
    CONNECTED,              //已连接
    UNCONNECTED,            //未连接
    LIMITOPENED,            //超限已开启
    LIMITCLOSED,            //超限已关闭
    LIMITTRIGGERED,         //超限已触发
    UPDATECALIBSUCESS,      //修改标定参数成功
    UPDATECALIBFAIL,        //修改标定参数失败
    CALIBINFO,              //标定参数
    UDPPIPE_FULL,           //udp爆满fifo
    UPDATEBCOFF,            //更新参数b
    CLEARDATASUCESS,        //清除下位机数据成功
    CLEARDATAFAIL,          //清除下位机数据失败
    LIMITALARM,             //超限判断触发
    CLOCKCHANGESUCESS,      //时钟切换成功
    CLOCKLOCKED,            //时钟锁定成功
    CLOCKLOCKFAILED,        //时钟锁定失败
    SWITCHMODESUCESS,       //切换模式成功
    SWITCHMODEFAIL,         //切换模式失败
    CLOSEHOST,              //断开连接
    SHUNTERRORRESULT,       //瞬态计算失败
    WIRERESISTANCERESULT,   //
    ZEROADJUSTRESULT,       //
    OCCUPY_REPLY,           //
    RESUMEDATASEND_REPLY,   //
    UPDATESHUNTCALIB_REPLY, //
    GETSHUNTCALIB_REPLY,    //
    AUTOSHUNTCALIB_REPLY,   //
    ALGORITHMUPDATESUCCESS, //算法库下发成功
    ALGORITHMUPDATEFAIL,    //算法库下发失败
    INTERLOCKAPPLIED,       //总闭锁已执行
    TESTSTOPPED,            //试验输出已停止（控制曲线执行完毕或下位机主动停止）
    HIGHTEMP,
    DISKSPACEREPLY,          //回复硬盘信息
    REPLYCMDCOUNT           //
};

void UpdateChnlParamsStrm(ChnlParams *pchnldef, ChnlParamsStrm *lchnldef);
void DeleteChnlParams(ChnlParamsStrm *lchnldef);
void CopyChnlParams(ChnlParamsStrm **des, ChnlParams *src);
