#pragma once

#define MAX_CHANNEL 256

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../tinyexpr.h"


typedef struct DiskSpace{
    uint64_t total_bytes;
    uint64_t used_bytes;
    int64_t available_bytes;
} DiskSpace;

typedef struct FactoryInfo
{
    char m_productName[64];
    char m_model[64];
    char m_sn[32];
    char m_manuFactured[64];
} FactoryInfo;

typedef struct VirtualChnlParams
{
    int n_count;//个数
    char formula_text[256];//虚拟公式
    te_variable vars[100];
    te_expr *expr;
}VirtualChnlParams;
typedef struct ChnlParams
{
	int chnlIndex_device; //udr默认0-64 通道在设备中的index
	int chnlIndex_card; //默认0-8索引 通道所属板卡
	int chnl_cardIndex; //默认0-8张板卡 通道所属板卡
	char strChnlName[128];
	char strSenserType[64]; // 0: Ac 1: ICP 2:DC 3: PT  4:digtalIO
	char strPhysicType[64];                    // 物理量类型
    double fCurrentValue;                    //报警判断
    float kCoff;                    //上位机下发的传感器k
    float bCoff;                    //上位机下发的传感器b
    float kCalib;                    //下位机先算板卡标定
    float bCalib;                    //下位机先算板卡标定
	float kFinally;                    //计算板卡标定完的电压值
	float bFinally;                    //计算板卡标定完的电压值
	float fSensitivity;                //灵敏度？
	float fSensitivity_t;                    //用倒数避免出发
	char strRange[64];
	float fGain;
	int bDaqEnable;
	int bStoreEnable;
	int bLimitEnable;                    //超限判断
	int nSamplingRate;                    //采样率
	int nFilter;                    //采样率
	int nJumpCount;                    //采样率
	int nFrameCount;                    //采样点数
	int nFrameSize;                    //采样大小
	char strUnit[16];                    //单位
	char strLowFilter[64];
	char strBridgeMode[64];//桥路 全桥 半桥 1/4桥
	float fBridgeResistance;//电桥电阻 120 350 1000
	float fImpValue;//激励电压
	char strImpValue[64];
	char strImpUnit[64];//激励电压单位
	int bZero;//是否参与调零
	float fTemperatureCompesateTemp;//温度补偿
	float fTemperatureCompesateVoltage;//温度补偿
    int  dioActionType;  // 类型 0=输入 1=输出
    int  dioTriggerHigh;  // 触发 1=真 0=假
    char strDioAction[128];  // 动作
    VirtualChnlParams vparams;
} ChnlParams;

typedef struct CurvePointInfo
{
    int index;
    float t;
    float y;
    char unit[64];
} CurvePointInfo;
typedef struct AlgorithmConstants
{
    double tThreshold ;
    float duInit ;
    float outputInit ;
    float duDivP ;
    float duDivI ;
    float duDiffCoeff ;
    float pv1LowLimit;
    float hwOffset ;
    float hwCoeff;
}AlgorithmConstants;
typedef struct AlgorithmFrameOutputs
{
    double pv1Out ;
    double pv2Out ;
    double pv3Out ;
    double sv1Out ;
    double sv2Out ;
    double d01 ;
    double d02 ;
    double error ;
    double du ;
    double output;
    double hw;
    double qr ;

    double cycle1Out ;
    double cycle2Out ;
    double cycle3Out ;
    double cycle4Out;
    double cycle5Out;
    double cycle6Out ;
    double cycle7Out ;
    double cycle8Out ;
}AlgorithmFrameOutputs;
// PID 控制器结构体
typedef struct PID_Controller{
    float kp;                // 比例系数
    float ki;                // 积分系数
    float kd;                // 微分系数
    float integral;          // 积分值
    float prev_error;        // 上一次误差值
    float prev_prev_error;   //上上次误差
    float integral_limit;    // 积分限制
    float output_limit_min;  // 输出下限
    float output_limit_max;  // 输出上限
    float param1;
    float param2;
    float param3;
    float param4;
    float prev_output;//上次输出
} PID_Controller;

// 假设的函数类型定义
typedef float (*AlgorithmExcute)(int loopIndex);
typedef struct RtLoopParamInfo
{
    int enabled;
    int output_chnlIndex;
    int n_input_chnls;
    int intput_chnlsIndex[16];
    int feedback_chnl;//反馈通道
    int target_chnl;//反馈通道
    int error_chnl;
    int other_chnl;

    char algorithm[128];
    AlgorithmExcute algorithmExcute; // 每个 loop 对应的函数
    int n_curve1;
    CurvePointInfo curve1[1024];
    int n_curve2;
    CurvePointInfo curve2[1024];
    int active_curve_index;//当前控制曲线名称 curve1/curve2
    int manual_mode;//0自动控制 1手动控制
    float manual_output;//手动输出
    PID_Controller pidControl;
    AlgorithmConstants algorithmConstans;
    AlgorithmFrameOutputs frameOutputs;
    int n_curveIndex1;
    int n_curveIndex2;
    float currentValue1;
    float currentValue2;
    float currentValue3;
    float currentValue4;
    float targetValue1;
    float targetValue2;
    float finalTargetValue;
    float feedbackValue;
    bool di_bool;

} RtLoopParamInfo;
typedef struct LoopParamInfo
{
    int loop_index;
    int enabled;
    int output_chnlIndex;
    int n_input_chnls;
    int intput_chnlsIndex[16];
    int feedback_chnl;//反馈通道
    int target_chnl;//反馈通道
    int error_chnl;
    int other_chnl;
    char algorithm[128];
    float pid_p;
    float pid_i;
    float pid_d;
    float voltage_limit;
    float voltage_lowLimit;
    int n_curve1;
    CurvePointInfo curve1[1024];
    int n_curve2;
    CurvePointInfo curve2[1024];
    int active_curve_index;//当前控制曲线名称 curve1/curve2
    float param1;
    float param2;
    float param3;
    float param4;
    float condition_coeff;
    float curve1_coeff;
    float curve2_coeff;
    int manual_mode;//0自动控制 1手动控制
    float manual_output;//手动输出
    bool di_bool;
} LoopParamInfo;

typedef struct AnalogCardParams
{
	int nCardIndex; //默认0-16
	int nCardSlot; //暂时没用
	int nCardChannel; //最大16
	char strCardName[128];
	char strCardModel[128];
	float k[9][64];                                //类型个数-通道个数
	float b[9][64];
	int nSamplingRate;                    //板卡最大采样率
} AnalogCardParams;
typedef struct DigitalCardParams
{
    int nCardIndex; //默认0-16
    int nCardSlot; //暂时没用
    int nCardChannel; //最大16
    char strCardName[128];
    char strCardModel[128];
    float k[9][64];                                //类型个数-通道个数
    float b[9][64];
    int nSamplingRate;                    //板卡最大采样率
} DigitalCardParams;
typedef struct IOCardParams
{
    int nCardIndex; //默认0-16
    int nCardSlot; //暂时没用
    int nCardChannel; //最大16
    char strCardName[128];
    char strCardModel[128];
    float k[9][64];                                //类型个数-通道个数
    float b[9][64];
    int nSamplingRate;                    //板卡最大采样率
} IOCardParams;
//typedef struct VirtualCardParams
//{
//    int nCardIndex; //默认0-16
//    int nCardSlot; //暂时没用
//    int nCardChannel; //最大16
//    char strCardName[128];
//    char strCardModel[128];
//    float k[9][64];                                //类型个数-通道个数
//    float b[9][64];
//    int nSamplingRate;                    //板卡最大采样率
//} VirtualCardParams;
struct s_workstate
{				//结构体类型定义最好在h文件，结构体变量定义最好在C文件
	bool isSampling;				//caiji
	bool isSaving;				//caiji
	bool isConnect;				//shangweiji
	bool isCountZero;          //调零
	int occupy_reply;
    int lock_state;//控制发送DIO
    int test_state;//0停止 1开始 2暂停
	int runId;
};
typedef struct DeviceParams
{
	int m_nAnalogChnls;
	struct ChnlParams **m_AnalogChnlParam;
    int m_nDigitalChnls;
    struct ChnlParams **m_DigitalChnlParam;
    int m_nTargetChnls;
    struct ChnlParams **m_TargetChnlParam;
    int m_nErrorChnls;
    struct ChnlParams **m_ErrorChnlParam;
    int m_nOtherChnls;
    struct ChnlParams **m_OtherChnlParam;

    int m_nVirtualChnls;
    struct ChnlParams **m_VirtualChnlParam;
    int m_nIOChnls;
    struct ChnlParams **m_IOChnlParam;

    int m_nChnls;
    struct ChnlParams **m_chnlParam;
} DeviceParams;

typedef struct RunningParam
{
	struct s_workstate m_state;				//工作状态
	int64_t nStartDaqTime;				//起始时间
	char strSaveName[256];//存储文件名称
	int64_t nStartSaveTime;//存储开始时间，用于存iff
	int64_t nSaveTimeDelay;//延迟开始存储对齐
	DeviceParams m_chnldefs;				//all channels of device , upper-computer send channel settings of all these channels;
	int m_nAnalogCards;
	struct AnalogCardParams **m_AnalogCardParams;
	int m_nDigitalCards;
	struct DigitalCardParams **m_DigitalCardParams;
    int m_nIOCards;
    struct IOCardParams **m_IOCardParams;

    int m_nLoops;
    struct LoopParamInfo **m_loopParam;

    DiskSpace space;
    AlgorithmConstants algorithmConstans[10];

} RunningParam;

typedef struct JsonDeviceParam
{
    FactoryInfo factory;
    DeviceParams m_chnldefs;                //全部通道all channels of device , upper-computer send channel settings of all these channels;
    int m_nAnalogCards;
    struct AnalogCardParams **m_AnalogCardParams;
    int m_nDigitalCards;
    struct DigitalCardParams **m_DigitalCardParams;
    int m_nIOCards;
    struct IOCardParams **m_IOCardParams;
} JsonDeviceParam;


typedef struct TempratureVoltage
{
	float Temprature_begin;
	float Temprature_end;
	float Voltage_begin;
	float Voltage_end;
	float k;
	float b;
} TempratureVoltage;

typedef struct TempratureParams
{
	char m_strName[256];                               //传感器类型名字
	int m_nTempratureDatas;                               //个数
	TempratureVoltage **m_tempratureData;                               //分度表内容
} TempratureParams;
typedef struct TempratureType
{
		int nCount;
		TempratureParams **m_params;
}TempratureType;


