#include <aio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>             // Strlen function
#include <sys/stat.h>           // Open() system call
#include <sys/types.h>          // Types for open()
#include <sys/mman.h>           // Mmap system call
#include <sys/ioctl.h>          // IOCTL system call
#include <unistd.h>             // Close() system call
#include <sys/time.h>           // Timing functions and definitions
#include <getopt.h>             // Option parsing
#include <errno.h>              // Error codes
#include <sys/syscall.h>
#include <sys/epoll.h> //EPOLL
#include <fcntl.h>  // 包含 open 函数的声明和相应的标志（如 O_RDWR 和 O_SYNC）
#include "upper_computer/common.h"
#include "upper_computer/protoHelper.h"
#include "rt_work.h"
#include "threadcreate.h"
#include <fcntl.h>  // 包含 open 函数的声明和相应的标志（如 O_RDWR 和 O_SYNC）
#include <unistd.h>
#include "hardware.h"
#include "tinyexpr.h"
#include <math.h>
static char cardData[FRAME_MAX_SIZE]; //板卡数据 //本地拷贝出来共享内存数据，加速用
static char dataBuf[FRAME_MAX_SIZE]; //存处理后数据
static char *dataPtr[MAX_CHANNEL]; //处理后数据多指针，databuf中指向不同通道的
static int32_t dataPtrIndex[MAX_CHANNEL]; //记录每个通道数据存储时的偏移（最后表示本次采集该通道的数据长度）
static int64_t dataDAQPoints[MAX_CHANNEL]; //打给上位机，让上位机知道数据丢没丢（数据：0，128，256，...）
static int64_t dataAllPoints[MAX_CHANNEL]; //总点数索引，从0开始


static int send_rxsize = 57352; //单次发送内存大小
static int channelCount = 0; //56 通道数

static int frameCount = MAX_CHANNEL; //单次采集总字节数

extern pthread_t thr_main;  //主线程
extern pthread_t thr_process;  //存储线程
extern struct kfifo gM2RFifoState;
struct kfifo gR2MFifoState;  //RT发送状态MAIN读取
struct kfifo gR2PFifoData;	//RT发送数据PROCESS读取
char *m_r2mFifoStateBuffer; //状态数组
char *m_r2pFifoDataBuffer; //数据数组
extern struct RunningParam gRunParam;
extern bool run_untill_shutDown;
static bool isSampling = false;
static int64_t begin;
static int64_t end;


static int allow_da=false;
static int allow_timeRun=false;//
static int test_status=0;//


extern struct pxi_sys_t g_pxi_system;
extern int LOOP_TIME_NEED; //ms值，单次凑齐读取100毫秒数据

static int n_rtLoops;
static RtLoopParamInfo rtLoop[1000];
/*
 * 采样，返回有效点数
 */

double adcode_to_mVoltage(uint32_t code)
{
    double voltage, voltage_per_code;
    int voltage_int;
    if (code & 0x800000)
        voltage_int = code | 0xFF000000;
    else
        voltage_int = code ;
    voltage_per_code = (((2.048 * 4.99 * 1*1000) / (double) (0x00FFFFFF + 1)));//* 1000 * 1000
    voltage = (1.0) * voltage_int * voltage_per_code;
    return voltage;
}
uint16_t mVoltage_to_adcode1(double voltage)
{
    // 电压范围的常量
    double max_value = (double)0xFFFF; // 修改为 uint16_t 的最大值 65535
    // 计算对应的代码
    int16_t code = (int16_t)(voltage *max_value / (2.048 * 4.99*2 * 1000.0));  // 将 uint32_t 更改为 uint16_t

    return (uint16_t)code;
}
double code_pre_uVoltage =  ((double)0x00FFFFFF + 1) /20000.0;
static int transChannel[16]={3,2,1,0,7,6,5,4,15,14,13,12,11,10,9,8};
extern volatile void *writeData;
extern volatile void *readData;
int temp=0;

#define PI 3.14159265358979323846
#define AMPLITUDE 2500.0  // 振幅，单位是 mV
#define FREQUENCY 0.2     // 正弦波频率，单位是 Hz
#define SAMPLES 100        // 100ms 对应的样本数
// 生成正弦波，并考虑第二个参数 second
extern int adc_fd;
float sineWave[SAMPLES];
uint16_t wData[SAMPLES];

static double d_mTime =0;//100MS

static void stopTest()
{
    allow_timeRun=false;
    d_mTime =0;//100MS
    for(int loopIndex=0;loopIndex<n_rtLoops;loopIndex++)
    {
        rtLoop[loopIndex].n_curveIndex1=0;
        rtLoop[loopIndex].n_curveIndex2=0;
    }
    test_status=0;
}
void generateSineWave(int second)
{
    for (int n = 0; n < SAMPLES; n++) {
        // 计算总时间 (second * 100ms + n * 2ms)
        float t = (second * 0.1) + (n / 1000.0); // second 以 100ms 为单位
        // 计算对应的正弦波电压
        sineWave[n] = AMPLITUDE * sin(2 * PI * FREQUENCY * t);
        //  sineWave[n] = 200;
        // 计算对应的正弦波电压并转换为16位整数
        wData[n] = (uint16_t)mVoltage_to_adcode1(sineWave[n]);

        //   printf("0x%x %.2f\n", wData[n],sineWave[n]);
    }
}
void generateSquareWave(int second)
{
    int samplesPerPeriod = 5000; // 周期总采样点数
    int halfPeriodSamples = samplesPerPeriod / 2;            // 半周期点数
    for (int n = 0; n < SAMPLES; n++)
    {
        // 计算当前时间（单位：秒）
        int t = (second * 100) + (n);// 1000 个样本为 1 秒


        // 判断当前样本属于正半周期还是负半周期
        if ((t  % samplesPerPeriod) < halfPeriodSamples)
        {
            sineWave[n] = AMPLITUDE; // 正半周期，幅值为正
        } else {
            sineWave[n] = -AMPLITUDE; // 负半周期，幅值为负
        }


        // 将电压值转换为 16 位整数
        wData[n] = (uint16_t)mVoltage_to_adcode1(sineWave[n]);

        // 可以打印调试信息
        //      printf("0x%x %.2f %d\n", wData[n], sineWave[n],n);
    }
}
void createLoopClosePIDSetPoints(int loopIndex)
{
    double t0 = d_mTime;
    int seg = rtLoop[loopIndex].n_curveIndex1;
    int curve_count=0;
    CurvePointInfo *info = NULL;
    curve_count = rtLoop[loopIndex].n_curve1;
    info = &rtLoop[loopIndex].curve1[0];
    while (seg < curve_count - 2 && t0 >= info[seg + 1].t)
    {
        seg++;
    }
    if (t0 <= info[0].t)
    {
        rtLoop[loopIndex].finalTargetValue = info[0].y;
    }
    else if (t0 >= info[curve_count - 1].t)
    {
        rtLoop[loopIndex].finalTargetValue = info[curve_count - 1].y;
        // 如果到达曲线末尾，发出停止试验消息
        stopTest(); // 这里调用停止试验的函数或逻辑
    }
    else
    {
        double t1 = info[seg].t;
        double y1 = info[seg].y;
        double t2 = info[seg + 1].t;
        double y2 = info[seg + 1].y;
        rtLoop[loopIndex].finalTargetValue = y1 + (t0 - t1) * (y2 - y1) / (t2 - t1);
    }
}

void createLoopOpenPIDSetPoints(int loopIndex)
{
    double t0 = d_mTime;
    int seg = rtLoop[loopIndex].n_curveIndex2;
    int curve_count=0;
    CurvePointInfo *info = NULL;
    curve_count = rtLoop[loopIndex].n_curve2;
    info = &rtLoop[loopIndex].curve2[0];
    while (seg < curve_count - 2 && t0 >= info[seg + 1].t)
    {
        seg++;
    }
    if (t0 <= info[0].t)
    {
        rtLoop[loopIndex].finalTargetValue = info[0].y;
    }
    else if (t0 >= info[curve_count - 1].t)
    {
        rtLoop[loopIndex].finalTargetValue = info[curve_count - 1].y;
        // 如果到达曲线末尾，发出停止试验消息
        stopTest(); // 这里调用停止试验的函数或逻辑
    }
    else
    {
        double t1 = info[seg].t;
        double y1 = info[seg].y;
        double t2 = info[seg + 1].t;
        double y2 = info[seg + 1].y;
        rtLoop[loopIndex].finalTargetValue = y1 + (t0 - t1) * (y2 - y1) / (t2 - t1);
    }
}
void createLoopHeatFluxPIDSetPoints(int loopIndex,int channel)
{
    double t0 = d_mTime;
    double targetValue1=0,targetValue2=0;
    int seg = 0;
    int curve_count=0;
    CurvePointInfo *info = NULL;
    {
        seg = rtLoop[loopIndex].n_curveIndex1;
        curve_count = rtLoop[loopIndex].n_curve1;
        info = &rtLoop[loopIndex].curve1[0];
        if(allow_timeRun)
        {
            while (seg < curve_count - 2 && t0 >= info[seg + 1].t)
            {
                seg++;
            }
            if (t0 <= info[0].t)
            {
                rtLoop[loopIndex].targetValue1 = info[0].y;
            }
            else if (t0 >= info[curve_count - 1].t)
            {
                rtLoop[loopIndex].targetValue1 = info[curve_count - 1].y;
                // 如果到达曲线末尾，发出停止试验消息
                stopTest(); // 这里调用停止试验的函数或逻辑
            }
            else
            {
                double t1 = info[seg].t;
                double y1 = info[seg].y;
                double t2 = info[seg + 1].t;
                double y2 = info[seg + 1].y;
                rtLoop[loopIndex].targetValue1 = y1 + (t0 - t1) * (y2 - y1) / (t2 - t1);
            }
        }
        //冻结
        else
        {
            targetValue1=rtLoop[loopIndex].targetValue1;
        }
        rtLoop[loopIndex].n_curveIndex1 = seg;
    }

    {
        seg = rtLoop[loopIndex].n_curveIndex2;
        curve_count = rtLoop[loopIndex].n_curve2;
        info = &rtLoop[loopIndex].curve2[0];
        if(allow_timeRun)
        {
            while (seg < curve_count - 2 && t0 >= info[seg + 1].t)
            {
                seg++;
            }
            if (t0 <= info[0].t)
            {
                rtLoop[loopIndex].targetValue2 = info[0].y;
            }
            else if (t0 >= info[curve_count - 1].t)
            {
                rtLoop[loopIndex].targetValue2 = info[curve_count - 1].y;
                // 如果到达曲线末尾，发出停止试验消息
                stopTest(); // 这里调用停止试验的函数或逻辑
            }
            else
            {
                double t1 = info[seg].t;
                double y1 = info[seg].y;
                double t2 = info[seg + 1].t;
                double y2 = info[seg + 1].y;
                rtLoop[loopIndex].targetValue2 = y1 + (t0 - t1) * (y2 - y1) / (t2 - t1);
            }
        }
        //冻结
        else
        {
            targetValue2=rtLoop[loopIndex].targetValue2;
        }
        rtLoop[loopIndex].n_curveIndex2 = seg;
    }

    AlgorithmConstants* algorithmConstans=&rtLoop[loopIndex].algorithmConstans;
    AlgorithmFrameOutputs *frameOutputs=&rtLoop[loopIndex].frameOutputs;
    PID_Controller *pid = &rtLoop[loopIndex].pidControl;
    if(channel ==3)
    {
        frameOutputs->pv3Out = rtLoop[loopIndex].currentValue3*(1-pid->param3)+frameOutputs->pv3Out*pid->param3;
        rtLoop[loopIndex].finalTargetValue = targetValue1*(1- (algorithmConstans->hwOffset+algorithmConstans->hwCoeff*(rtLoop[loopIndex].currentValue3+273))/targetValue2);
    }
    else if(channel ==1)
    {

        frameOutputs->pv1Out = rtLoop[loopIndex].currentValue1*(1-pid->param1)+frameOutputs->pv1Out*pid->param1;
        rtLoop[loopIndex].finalTargetValue = targetValue1*(1- (algorithmConstans->hwOffset+algorithmConstans->hwCoeff*(rtLoop[loopIndex].currentValue1+273))/targetValue2);
    }
}

//增量PID
float executeAlgorithmPID(int loopIndex)
{
    // 获取 PID 控制器
    PID_Controller *pid = &rtLoop[loopIndex].pidControl;
    //开环跟踪
    // if(rtLoop[loopIndex].active_curve_index == 1)
    if(rtLoop[loopIndex].di_bool)
    {
        //计算目标值
        createLoopOpenPIDSetPoints(loopIndex);
        //开环跟踪算法
        float open_loop_output = rtLoop[loopIndex].finalTargetValue;
        float error = (rtLoop[loopIndex].finalTargetValue - rtLoop[loopIndex].feedbackValue)
                                                    / gRunParam.m_chnldefs.m_chnlParam[rtLoop[loopIndex].feedback_chnl]->kCoff;
        // 应用输出限制
        if (open_loop_output > pid->output_limit_max)
        {
            open_loop_output = pid->output_limit_max;
        }
        else if(open_loop_output < pid->output_limit_min)
        {
            open_loop_output = pid->output_limit_min;
        }
        // 跟随算法
        // 更新历史误差
        pid->prev_prev_error = pid->prev_error; // e(k-2) = e(k-1)
        pid->prev_error = error;               // e(k-1) = e(k)
        pid->prev_output = open_loop_output;
        return open_loop_output;
    }
    //闭环算法
    //计算目标值
    createLoopClosePIDSetPoints(loopIndex);
    // 计算误差
    float error = (rtLoop[loopIndex].finalTargetValue - rtLoop[loopIndex].feedbackValue)
                                                / gRunParam.m_chnldefs.m_chnlParam[rtLoop[loopIndex].feedback_chnl]->kCoff;
    // 计算增量 Δu(k)
    float delta_output = pid->kp * (error - pid->prev_error) +
            pid->ki * error +
            pid->kd * (error - 2 * pid->prev_error + pid->prev_prev_error);
    // 计算总的输出值 u(k)
    float output_value = pid->prev_output + delta_output;
    // 应用输出限制
    if (output_value > pid->output_limit_max)
    {
        output_value = pid->output_limit_max;
    }
    else if
    (output_value < pid->output_limit_min)
    {
        output_value = pid->output_limit_min;
    }
    // 更新历史误差
    pid->prev_prev_error = pid->prev_error; // e(k-2) = e(k-1)
    pid->prev_error = error;               // e(k-1) = e(k)
    // 更新上次输出值
    pid->prev_output = output_value;
    // 返回当前输出值
    return output_value;
}
//热流
float executeAlgorithm1(int loopIndex)
{
    // 获取 PID 控制器
    PID_Controller *pid =&rtLoop[loopIndex].pidControl;
    AlgorithmConstants* algorithmConstans=&rtLoop[loopIndex].algorithmConstans;
    AlgorithmFrameOutputs *frameOutputs=&rtLoop[loopIndex].frameOutputs;
    //做滑动平均
    frameOutputs->pv1Out = rtLoop[loopIndex].currentValue1*(1-pid->param1)+frameOutputs->pv1Out*pid->param1;
    //开环跟踪
    if(rtLoop[loopIndex].di_bool)
    {
        //计算目标值
        createLoopOpenPIDSetPoints(loopIndex);
        //开环跟踪算法
        float open_loop_output = rtLoop[loopIndex].finalTargetValue;
        float error = (rtLoop[loopIndex].finalTargetValue - frameOutputs->pv1Out)
                                                    / gRunParam.m_chnldefs.m_chnlParam[rtLoop[loopIndex].intput_chnlsIndex[0]]->kCoff;
        // 应用输出限制
        if (open_loop_output > pid->output_limit_max)
        {
            open_loop_output = pid->output_limit_max;
        }
        else if(open_loop_output < pid->output_limit_min)
        {
            open_loop_output = pid->output_limit_min;
        }
        // 跟随算法
        // 更新历史误差
        pid->prev_prev_error = pid->prev_error; // e(k-2) = e(k-1)
        pid->prev_error = error;               // e(k-1) = e(k)
        // 更新上次输出值
        pid->prev_output = open_loop_output;
        return open_loop_output;
    }
    //闭环算法
    //计算目标值
    createLoopClosePIDSetPoints(loopIndex);
    // 计算误差
    float error = (rtLoop[loopIndex].finalTargetValue - frameOutputs->pv1Out)
                                                / gRunParam.m_chnldefs.m_chnlParam[rtLoop[loopIndex].intput_chnlsIndex[0]]->kCoff;
    float output_value = 0;
    //算法要求初始预加热
    if (d_mTime < algorithmConstans->tThreshold)
    {
        output_value = algorithmConstans->outputInit;
    }
    else
    {
        // 计算增量 Δu(k)
        float delta_output = pid->kp * (error - pid->prev_error) /algorithmConstans->duDivP+
                pid->ki * error /algorithmConstans->duDivI+
                pid->kd * (error - algorithmConstans->duDiffCoeff * pid->prev_error + pid->prev_prev_error);
        // 计算总的输出值 u(k)
        output_value = pid->prev_output + delta_output;
    }
    // 应用输出限制
    if (output_value > pid->output_limit_max)
    {
        output_value = pid->output_limit_max;
    }
    else if(output_value < pid->output_limit_min)
    {
        output_value = pid->output_limit_min;
    }
    // 更新历史误差
    pid->prev_prev_error = pid->prev_error; // e(k-2) = e(k-1)
    pid->prev_error = error;               // e(k-1) = e(k)
    // 更新上次输出值
    pid->prev_output = output_value;
    // 返回当前输出值
    return output_value;
}
//K偶
float executeAlgorithm2(int loopIndex)
{
    // 获取 PID 控制器
    PID_Controller *pid =&rtLoop[loopIndex].pidControl;
    AlgorithmConstants* algorithmConstans=&rtLoop[loopIndex].algorithmConstans;
    AlgorithmFrameOutputs *frameOutputs=&rtLoop[loopIndex].frameOutputs;
    //做滑动平均
    frameOutputs->pv1Out = rtLoop[loopIndex].currentValue1*(1-pid->param1)+frameOutputs->pv1Out*pid->param1;
    //开环跟踪
    if(rtLoop[loopIndex].di_bool)
    {
        //计算目标值
        createLoopOpenPIDSetPoints(loopIndex);
        //开环跟踪算法
        float open_loop_output = rtLoop[loopIndex].finalTargetValue;
        float error = (rtLoop[loopIndex].finalTargetValue - frameOutputs->pv1Out);
        // 应用输出限制
        if (open_loop_output > pid->output_limit_max)
        {
            open_loop_output = pid->output_limit_max;
        }
        else if(open_loop_output < pid->output_limit_min)
        {
            open_loop_output = pid->output_limit_min;
        }
        // 跟随算法
        // 更新历史误差
        pid->prev_prev_error = pid->prev_error; // e(k-2) = e(k-1)
        pid->prev_error = error;               // e(k-1) = e(k)
        // 更新上次输出值
        pid->prev_output = open_loop_output;
        return open_loop_output;
    }
    //闭环算法
    //计算目标值
    createLoopClosePIDSetPoints(loopIndex);
    // 计算误差
    float error = (rtLoop[loopIndex].finalTargetValue - frameOutputs->pv1Out);
    float output_value = 0;
    //算法要求初始预加热
    if (d_mTime < algorithmConstans->tThreshold)
    {
        output_value = algorithmConstans->outputInit;
    }
    else
    {
        // 计算增量 Δu(k)
        float delta_output = pid->kp * (error - pid->prev_error) /algorithmConstans->duDivP+
                pid->ki * error /algorithmConstans->duDivI+
                pid->kd * (error - algorithmConstans->duDiffCoeff * pid->prev_error + pid->prev_prev_error);
        // 计算总的输出值 u(k)
        output_value = pid->prev_output + delta_output;
    }
    // 应用输出限制
    if (output_value > pid->output_limit_max)
    {
        output_value = pid->output_limit_max;
    }
    else if(output_value < pid->output_limit_min)
    {
        output_value = pid->output_limit_min;
    }
    // 更新历史误差
    pid->prev_prev_error = pid->prev_error; // e(k-2) = e(k-1)
    pid->prev_error = error;               // e(k-1) = e(k)
    // 更新上次输出值
    pid->prev_output = output_value;
    // 返回当前输出值
    return output_value;
}
//点温仪
float executeAlgorithm3(int loopIndex)
{
    // 获取 PID 控制器
    PID_Controller *pid =&rtLoop[loopIndex].pidControl;
    AlgorithmConstants* algorithmConstans=&rtLoop[loopIndex].algorithmConstans;
    AlgorithmFrameOutputs *frameOutputs=&rtLoop[loopIndex].frameOutputs;
    frameOutputs->pv1Out = rtLoop[loopIndex].currentValue1*(1-pid->param1)+frameOutputs->pv1Out*pid->param1;

    //开环跟踪
    if(rtLoop[loopIndex].di_bool)
    {
        //计算目标值
        createLoopOpenPIDSetPoints(loopIndex);
        //开环跟踪算法
        float open_loop_output = rtLoop[loopIndex].finalTargetValue;
        float error = (rtLoop[loopIndex].finalTargetValue - frameOutputs->pv1Out);
        // 应用输出限制
        if (open_loop_output > pid->output_limit_max)
        {
            open_loop_output = pid->output_limit_max;
        }
        else if(open_loop_output < pid->output_limit_min)
        {
            open_loop_output = pid->output_limit_min;
        }
        // 跟随算法
        // 更新历史误差
        pid->prev_prev_error = pid->prev_error; // e(k-2) = e(k-1)
        pid->prev_error = error;               // e(k-1) = e(k)
        // 更新上次输出值
        pid->prev_output = open_loop_output;
        return open_loop_output;
    }
    createLoopClosePIDSetPoints(loopIndex);
    // 计算误差
    float error = rtLoop[loopIndex].finalTargetValue - frameOutputs->pv1Out;
    // 将误差转换为电压
    //error /= gRunParam.m_chnldefs.m_chnlParam[rtLoop[loopIndex].feedback_chnl]->kCoff;

    float output_value=0;
    if (frameOutputs->pv1Out < algorithmConstans->pv1LowLimit)
    {
        output_value = pid->param2;
        pid->prev_output = output_value;
    }
    else
    {
        // 计算增量 Δu(k)
        float delta_output = pid->kp * (error - pid->prev_error) /algorithmConstans->duDivP+
                pid->ki * error /algorithmConstans->duDivI+
                pid->kd * (error - algorithmConstans->duDiffCoeff * pid->prev_error + pid->prev_prev_error);
        // 计算总的输出值 u(k)
        output_value = pid->prev_output + delta_output;
    }
    if (d_mTime < algorithmConstans->tThreshold)
    {
        output_value = algorithmConstans->outputInit;
    }
    // 应用输出限制
    if (output_value > pid->output_limit_max)
    {
        output_value = pid->output_limit_max;
    }
    else if (output_value < pid->output_limit_min)
    {
        output_value = pid->output_limit_min;
    }
    // 更新历史误差
    pid->prev_prev_error = pid->prev_error; // e(k-2) = e(k-1)
    pid->prev_error = error;               // e(k-1) = e(k)
    // 更新上次输出值
    pid->prev_output = output_value;
    // 返回当前输出值
    return output_value;
}
//冷壁热流纯K偶
float executeAlgorithm4(int loopIndex)
{
    // 获取 PID 控制器
    PID_Controller *pid =&rtLoop[loopIndex].pidControl;
    AlgorithmConstants* algorithmConstans=&rtLoop[loopIndex].algorithmConstans;
    AlgorithmFrameOutputs *frameOutputs=&rtLoop[loopIndex].frameOutputs;
    frameOutputs->pv2Out = rtLoop[loopIndex].feedbackValue*(1-pid->param1)+frameOutputs->pv2Out*pid->param1;
    createLoopHeatFluxPIDSetPoints(loopIndex,3);
    // 计算误差
    float error = (rtLoop[loopIndex].finalTargetValue - frameOutputs->pv2Out)
                                                / gRunParam.m_chnldefs.m_chnlParam[rtLoop[loopIndex].intput_chnlsIndex[1]]->kCoff;

    float output_value=0;
    if (d_mTime < algorithmConstans->tThreshold)
    {
        output_value = algorithmConstans->outputInit;
    }
    else
    {
        // 计算增量 Δu(k)
        float delta_output = pid->kp * (error - pid->prev_error) /algorithmConstans->duDivP+
                pid->ki * error /algorithmConstans->duDivI+
                pid->kd * (error - algorithmConstans->duDiffCoeff * pid->prev_error + pid->prev_prev_error);
        // 计算总的输出值 u(k)
        output_value = pid->prev_output + delta_output;
    }
    // 应用输出限制
    if (output_value > pid->output_limit_max)
    {
        output_value = pid->output_limit_max;
    }
    else if (output_value < pid->output_limit_min)
    {
        output_value = pid->output_limit_min;
    }
    // 更新历史误差
    pid->prev_prev_error = pid->prev_error; // e(k-2) = e(k-1)
    pid->prev_error = error;               // e(k-1) = e(k)
    // 更新上次输出值
    pid->prev_output = output_value;
    // 返回当前输出值
    return output_value;
}
//温度切换点温仪
float executeAlgorithm5(int loopIndex)
{
    // 获取 PID 控制器
    PID_Controller *pid =&rtLoop[loopIndex].pidControl;
    AlgorithmConstants* algorithmConstans=&rtLoop[loopIndex].algorithmConstans;
    AlgorithmFrameOutputs *frameOutputs=&rtLoop[loopIndex].frameOutputs;
    frameOutputs->pv1Out = rtLoop[loopIndex].currentValue1*(1-pid->param1)+frameOutputs->pv1Out*pid->param1;
    frameOutputs->pv2Out = rtLoop[loopIndex].currentValue2*(1-pid->param2)+frameOutputs->pv2Out*pid->param2;

    createLoopClosePIDSetPoints(loopIndex);
    // 计算误差
    float error = rtLoop[loopIndex].finalTargetValue - frameOutputs->pv2Out;
    if(frameOutputs->pv2Out<pid->param3)
    {
        error = rtLoop[loopIndex].finalTargetValue - frameOutputs->pv1Out;
    }
    // 将误差转换为电压
    // error /= gRunParam.m_chnldefs.m_chnlParam[rtLoop[loopIndex].feedback_chnl]->kCoff;

    float output_value=0;
    if (d_mTime < algorithmConstans->tThreshold)
    {
        output_value = algorithmConstans->outputInit;
    }
    else
    {
        // 计算增量 Δu(k)
        float delta_output = pid->kp * (error - pid->prev_error) /algorithmConstans->duDivP+
                pid->ki * error /algorithmConstans->duDivI+
                pid->kd * (error - algorithmConstans->duDiffCoeff * pid->prev_error + pid->prev_prev_error);
        // 计算总的输出值 u(k)
        output_value = pid->prev_output + delta_output;
    }
    // 应用输出限制
    if (output_value > pid->output_limit_max)
    {
        output_value = pid->output_limit_max;
    }
    else if (output_value < pid->output_limit_min)
    {
        output_value = pid->output_limit_min;
    }
    // 更新历史误差
    pid->prev_prev_error = pid->prev_error; // e(k-2) = e(k-1)
    pid->prev_error = error;               // e(k-1) = e(k)
    // 更新上次输出值
    pid->prev_output = output_value;
    // 返回当前输出值
    return output_value;
}
//冷壁一部头罩
float executeAlgorithm6(int loopIndex)
{
    // 获取 PID 控制器
    PID_Controller *pid =&rtLoop[loopIndex].pidControl;
    AlgorithmConstants* algorithmConstans=&rtLoop[loopIndex].algorithmConstans;
    AlgorithmFrameOutputs *frameOutputs=&rtLoop[loopIndex].frameOutputs;
    frameOutputs->pv2Out = rtLoop[loopIndex].feedbackValue*(1-pid->param2)+frameOutputs->pv2Out*pid->param1;
    // 计算误差
    float error = 0;
    if(rtLoop[loopIndex].di_bool)
        createLoopHeatFluxPIDSetPoints(loopIndex,1);
    else
        createLoopHeatFluxPIDSetPoints(loopIndex,3);
    // 将误差转换为电压

    error = (rtLoop[loopIndex].finalTargetValue - frameOutputs->pv2Out)
                                                / gRunParam.m_chnldefs.m_chnlParam[rtLoop[loopIndex].intput_chnlsIndex[1]]->kCoff;
    float output_value=0;
    if (d_mTime < algorithmConstans->tThreshold)
    {
        output_value = algorithmConstans->outputInit;
    }
    else
    {
        // 计算增量 Δu(k)
        float delta_output = pid->kp * (error - pid->prev_error) /algorithmConstans->duDivP+
                pid->ki * error /algorithmConstans->duDivI+
                pid->kd * (error - algorithmConstans->duDiffCoeff * pid->prev_error + pid->prev_prev_error);
        // 计算总的输出值 u(k)
        output_value = pid->prev_output + delta_output;
    }
    // 应用输出限制
    if (output_value > pid->output_limit_max)
    {
        output_value = pid->output_limit_max;
    }
    else if (output_value < pid->output_limit_min)
    {
        output_value = pid->output_limit_min;
    }
    // 更新历史误差
    pid->prev_prev_error = pid->prev_error; // e(k-2) = e(k-1)
    pid->prev_error = error;               // e(k-1) = e(k)
    // 更新上次输出值
    pid->prev_output = output_value;
    // 返回当前输出值
    return output_value;
}
struct timespec time1,time2,time3,time4,time5,time6,time7,time8;
double maxPhysicTime=0,averPhysicTime=0;
double maxWriteTime=0,averWriteTime=0;
static int printfTime()
{
    long total1_nanoseconds1 = (time5.tv_sec- time4.tv_sec)*1000000000 +time5.tv_nsec- time4.tv_nsec;
   // long total1_nanoseconds2 =  (time6.tv_sec- time5.tv_sec)*1000000000 +time6.tv_nsec- time5.tv_nsec;
    averPhysicTime+=(double)total1_nanoseconds1*0.01;
   // averWriteTime+=(double)total1_nanoseconds2*0.01;
    if(maxPhysicTime<total1_nanoseconds1)
        maxPhysicTime= total1_nanoseconds1;
   // if(maxWriteTime<total1_nanoseconds2)
    //    maxWriteTime= total1_nanoseconds2;
    // 打印运行时间
    printf("单次运算时间：%ld .\n",total1_nanoseconds1);
}
static int printfTime1()
{
   // averPhysicTime;
    // 计算时间差
    long total1_nanoseconds2 = (time7.tv_sec- time6.tv_sec)*1000000000 + time7.tv_nsec- time6.tv_nsec;
   // total1_nanoseconds2/=1000;
    // 打印运行时间
    //  printf("准备100ms目标值时间：%ld us rt将100ms数据抄送时间：%ld us\n",total_nanoseconds, total1_nanoseconds2);
    printf("平均运算时间：%.1f 最大运算时间：%.1f 抄写时间%ld.\n", averPhysicTime,maxPhysicTime,total1_nanoseconds2);

//    float alltime =total_nanoseconds +total1_nanoseconds2 +averPhysicTime*100 +averWriteTime*100 ;
//    alltime*=0.001;
    // printf("单次采样消耗时间：%.f us  100ms总时间:%.1f ms\n",averPhysicTime+averWriteTime,alltime);
    maxPhysicTime=0;
    averPhysicTime=0;
    //  maxWriteTime=0;
   // averWriteTime=0;
}

static uint8_t reciveDataPtr[16*48*4];

static int doSample()
{
    // 获取开始时间
    // clock_gettime(CLOCK_MONOTONIC, &time1);
    //createSetPoints();
    //  generateSineWave(temp);
    //   generateSquareWave(temp);
    //谱和目标值计算


    int cChannel=0;
    int nChannel=0;
    // clock_gettime(CLOCK_MONOTONIC, &time2);
    for(int time=0;time<100;time++)
    {
        //采集所有通道
        for(int cardIndex=0;cardIndex<gRunParam.m_nAnalogCards;cardIndex++)
        {
            clock_gettime(CLOCK_MONOTONIC, &time3);
            if (read_exact_timeout(adc_fd, &reciveDataPtr[0], 16*48*4,1) != 0)//48K
            {
                printf("time out !\n");
                continue;
            }

            time8.tv_nsec=time4.tv_nsec;
            time8.tv_sec=time4.tv_sec;
            clock_gettime(CLOCK_MONOTONIC, &time4);
            long dtime =(time4.tv_sec- time8.tv_sec)*1000000000 + time4.tv_nsec- time8.tv_nsec;
            if(dtime>1200000)
                printf("单次超时1.2ms %ld\n",dtime);
            cChannel=0;
            uint32_t data = 0;
            for(int i=0;i<16*48*4;i++ )
            {
                //去掉头 3字节
                if(i%16==0)
                {
                    i+=3;
                    continue;
                }
                data = 0;
                data = (reciveDataPtr[i+2] << 16) | (reciveDataPtr[i+ 1] << 8) | (reciveDataPtr[i]);
                nChannel=transChannel[cChannel];
                float result = adcode_to_mVoltage(data) * gRunParam.m_chnldefs.m_AnalogChnlParam[nChannel+cardIndex*16]->kFinally
                        + gRunParam.m_chnldefs.m_AnalogChnlParam[nChannel+cardIndex*16]->bFinally;

                (*(float*) &dataPtr[nChannel+cardIndex*16][dataPtrIndex[nChannel+cardIndex*16] * 4]) = result;
                gRunParam.m_chnldefs.m_chnlParam[nChannel+cardIndex*16]-> fCurrentValue= result;
                dataPtrIndex[nChannel+cardIndex*16]++; //记录点数
                dataAllPoints[nChannel+cardIndex*16]++; //记录点数
                i+=2;
                cChannel++;
                if(cChannel==16)
                    cChannel=0;
            }
        }
        //虚拟通道计算
        for (int j = 0; j < gRunParam.m_chnldefs.m_nVirtualChnls; j++)
        {
            int virtualChnlIndex= gRunParam.m_chnldefs.m_VirtualChnlParam[j]->chnlIndex_device;
            double result =  te_eval(gRunParam.m_chnldefs.m_VirtualChnlParam[j]->vparams.expr);
            (*(float*) &dataPtr[virtualChnlIndex][dataPtrIndex[virtualChnlIndex] * 4]) =(float) result; // 计算结果
            // if(j==0)
            //  printf("%f %f %f %f\n",gRunParam.m_chnldefs.m_chnlParam[1]-> fCurrentValue,gRunParam.m_chnldefs.m_chnlParam[20]-> fCurrentValue,(float) result,gRunParam.m_chnldefs.m_chnlParam[1]-> fCurrentValue+gRunParam.m_chnldefs.m_chnlParam[20]-> fCurrentValue);
            dataPtrIndex[virtualChnlIndex]++; //记录点数
            dataAllPoints[virtualChnlIndex]++; //记录点数
        }
        //全部DA输出置0
        for(int chnl=0;chnl<gRunParam.m_chnldefs.m_nDigitalChnls;chnl++)
        {
            gRunParam.m_chnldefs.m_DigitalChnlParam[chnl]->fCurrentValue=0;
        }
        //回路计算
        for(int loopIndex=0;loopIndex<n_rtLoops;loopIndex++)
        {
            int outputChnlIndex= rtLoop[loopIndex].output_chnlIndex;
            int target_chnl= rtLoop[loopIndex].target_chnl;
            int error_chnl= rtLoop[loopIndex].error_chnl;
            int inputChnlIndex= rtLoop[loopIndex].feedback_chnl;
            float measured_value = gRunParam.m_chnldefs.m_chnlParam[inputChnlIndex]->fCurrentValue;
            if(! rtLoop[loopIndex].enabled)
            {
                //给输出
                gRunParam.m_chnldefs.m_chnlParam[outputChnlIndex]->fCurrentValue=0;
                //给目标值
                gRunParam.m_chnldefs.m_chnlParam[target_chnl]->fCurrentValue=0;
                //给误差
                gRunParam.m_chnldefs.m_chnlParam[error_chnl]->fCurrentValue=0-measured_value;
            }
            else
            {
                //手动开环模式
                if(rtLoop[loopIndex].manual_mode)
                {
                    float setpointValue =rtLoop[loopIndex].manual_output;
                    //   setpointValue=sineWave[time];
                    float result =setpointValue;
                    if(!allow_da)
                        result=0;
                    //给输出
                    gRunParam.m_chnldefs.m_chnlParam[outputChnlIndex]->fCurrentValue=result;
                    //给目标值
                    gRunParam.m_chnldefs.m_chnlParam[target_chnl]->fCurrentValue=setpointValue;
                    //给误差
                    gRunParam.m_chnldefs.m_chnlParam[error_chnl]->fCurrentValue=setpointValue-measured_value;

                    // printf("开环模式:输出%d:%f 目标%d:%f 误差%d:%f\n",outputChnlIndex,result,target_chnl,setpointValue,error_chnl,setpointValue-measured_value);
                }
                //PID闭环模式
                else
                {
                    RtLoopParamInfo *currentLoop = &rtLoop[loopIndex];
                    int other_chnl= rtLoop[loopIndex].other_chnl;

                    for(int tempChnl =0;tempChnl<currentLoop->n_input_chnls;tempChnl++)
                    {
                        if(tempChnl==0)
                        {
                            if(currentLoop->intput_chnlsIndex[0] == -1)
                                currentLoop->currentValue1=nanf("");
                            else
                                currentLoop->currentValue1=gRunParam.m_chnldefs.m_chnlParam[currentLoop->intput_chnlsIndex[0]]->fCurrentValue;
                        }
                        else if(tempChnl==1)
                        {
                            if(currentLoop->intput_chnlsIndex[1] == -1)
                                currentLoop->currentValue2=nanf("");
                            else
                                currentLoop->currentValue2=gRunParam.m_chnldefs.m_chnlParam[currentLoop->intput_chnlsIndex[1]]->fCurrentValue;
                        }
                        else if(tempChnl==2)
                        {
                            if(currentLoop->intput_chnlsIndex[2] == -1)
                                currentLoop->currentValue3=nanf("");
                            else
                                currentLoop->currentValue3=gRunParam.m_chnldefs.m_chnlParam[currentLoop->intput_chnlsIndex[2]]->fCurrentValue;
                        }
                        else if(tempChnl==3)
                        {
                            if(currentLoop->intput_chnlsIndex[3] == -1)
                                currentLoop->currentValue4=nanf("");
                            else
                                currentLoop->currentValue4=gRunParam.m_chnldefs.m_chnlParam[currentLoop->intput_chnlsIndex[3]]->fCurrentValue;
                        }
                    }
                    currentLoop->feedbackValue=measured_value;
                    float result= currentLoop->algorithmExcute(loopIndex);
                    if(!allow_da)
                    {
                        result=0;
                    }
                    if(test_status==0)
                        result=0;
                    gRunParam.m_chnldefs.m_chnlParam[outputChnlIndex]->fCurrentValue= result;
                    gRunParam.m_chnldefs.m_chnlParam[target_chnl]->fCurrentValue=currentLoop->finalTargetValue;
                    gRunParam.m_chnldefs.m_chnlParam[error_chnl]->fCurrentValue=currentLoop->pidControl.prev_error;
                    // printf("闭环模式:输出%d:%f 目标%d:%f 误差%d:%f 热流%d:%f\n",outputChnlIndex,result,target_chnl,currentLoop->finalTargetValue,error_chnl,gRunParam.m_chnldefs.m_chnlParam[error_chnl]->fCurrentValue,other_chnl,gRunParam.m_chnldefs.m_chnlParam[other_chnl]->fCurrentValue);
                }
            }

            //给输出
            (*(float*) &dataPtr[outputChnlIndex][dataPtrIndex[outputChnlIndex] * 4]) = gRunParam.m_chnldefs.m_chnlParam[outputChnlIndex]->fCurrentValue;
            dataPtrIndex[outputChnlIndex]++; //记录点数
            dataAllPoints[outputChnlIndex]++; //记录点数
            //给目标值
            (*(float*) &dataPtr[target_chnl][dataPtrIndex[target_chnl] * 4]) = gRunParam.m_chnldefs.m_chnlParam[target_chnl]->fCurrentValue;
            dataPtrIndex[target_chnl]++; //记录点数
            dataAllPoints[target_chnl]++; //记录点数
            //给误差
            (*(float*) &dataPtr[error_chnl][dataPtrIndex[error_chnl] * 4]) = gRunParam.m_chnldefs.m_chnlParam[error_chnl]->fCurrentValue;
            dataPtrIndex[error_chnl]++; //记录点数
            dataAllPoints[error_chnl]++; //记录点数
        }
        //更新回路时间
        if(allow_timeRun)  //注意：非暂停状态下时间累计
        {
            d_mTime += 0.001;//100MS
        }
        clock_gettime(CLOCK_MONOTONIC, &time5);
       // printfTime();
        //输出发送
        for(int outChnlIndex=0;outChnlIndex<gRunParam.m_chnldefs.m_nDigitalChnls;outChnlIndex++)
        {
            // if(outChnlIndex ==4)
            //     printf("输出值:%f\n",gRunParam.m_chnldefs.m_DigitalChnlParam[outChnlIndex]->fCurrentValue);
            //输出驱动要不要看？
            send_da_sample(outChnlIndex,mVoltage_to_adcode1(gRunParam.m_chnldefs.m_DigitalChnlParam[outChnlIndex]->fCurrentValue));
            //   send_da_sample(outChnlIndex,wData[time]);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &time6);


    memcpy(&dataBuf[0], &dataPtrIndex[0], MAX_CHANNEL * 4); //点数
    for (int i = 0; i < MAX_CHANNEL; i++)
    {
        dataDAQPoints[i] = dataAllPoints[i] - dataPtrIndex[i];
    }
    memcpy(&dataBuf[MAX_CHANNEL * 4], &dataDAQPoints[0], MAX_CHANNEL * 8); //索引
    temp++;
    return 1;

}
char* initPosit[MAX_CHANNEL];
void channelPtrMoveFirst()
{
    //移动指针指向
    frameCount = 0;
    channelCount = 0;
    memset(&dataBuf[0], 0, FRAME_MAX_SIZE);
    memset(&cardData[0], 0, FRAME_MAX_SIZE);
    //模拟板卡
    for (int y = 0; y < gRunParam.m_chnldefs.m_nChnls; y++)
    {
        dataPtr[channelCount] = &dataBuf[MAX_CHANNEL * 4 + MAX_CHANNEL * 8 + frameCount]; //2*4 = 8字节时间 +64*2 =136字节 单通道点数
        initPosit[channelCount]= &dataBuf[MAX_CHANNEL * 4 + MAX_CHANNEL * 8 + frameCount];
        dataPtrIndex[channelCount] = 0; //处理后数据多指针
        frameCount += gRunParam.m_chnldefs.m_chnlParam[y]->nFrameSize;
        channelCount++;
    }
}
//用于计算拷贝到数组的位置，不是采集帧协议的位置
static void channelPtrMove()
{
    //移动指针指向
    channelCount = 0;
    //  memset(&dataBuf[0], 0, FRAME_MAX_SIZE);
    //  memset(&cardData[0], 0, FRAME_MAX_SIZE);
    //模拟板卡
    for (int y = 0; y < gRunParam.m_chnldefs.m_nChnls; y++)
    {
        dataPtr[channelCount] =  initPosit[channelCount];
        dataPtrIndex[channelCount] = 0; //处理后数据多指针
        channelCount++;
    }
}
//热更新
static void updateHotParamDAQ()
{
    for(int loopIndex=0;loopIndex<n_rtLoops;loopIndex++)
    {
        //抄写上位机下发pid参数
        rtLoop[loopIndex].pidControl.kp=gRunParam.m_loopParam[loopIndex]->pid_p;
        rtLoop[loopIndex].pidControl.ki=gRunParam.m_loopParam[loopIndex]->pid_i;
        rtLoop[loopIndex].pidControl.kd=gRunParam.m_loopParam[loopIndex]->pid_d;
        //修改参数
        rtLoop[loopIndex].pidControl.param1=gRunParam.m_loopParam[loopIndex]->param1;
        rtLoop[loopIndex].pidControl.param2=gRunParam.m_loopParam[loopIndex]->param2;
        rtLoop[loopIndex].pidControl.param3=gRunParam.m_loopParam[loopIndex]->param3;
        rtLoop[loopIndex].pidControl.param4=gRunParam.m_loopParam[loopIndex]->param4;
        // 注意切换手动时积分值要不要跟随
        //    rtLoop[loopIndex].pidControl.integral=;
        //切换曲线
        rtLoop[loopIndex].active_curve_index=gRunParam.m_loopParam[loopIndex]->active_curve_index;//上位机下发
        printf("当前曲线:%d \n",rtLoop[loopIndex].active_curve_index);
        //是否启用
        rtLoop[loopIndex].enabled=gRunParam.m_loopParam[loopIndex]->enabled;
        //修改反馈通道
        rtLoop[loopIndex].feedback_chnl=gRunParam.m_loopParam[loopIndex]->feedback_chnl;
        //修改手动输出值
        rtLoop[loopIndex].manual_mode=gRunParam.m_loopParam[loopIndex]->manual_mode;
        rtLoop[loopIndex].manual_output=gRunParam.m_loopParam[loopIndex]->manual_output;
        rtLoop[loopIndex].di_bool=gRunParam.m_loopParam[loopIndex]->di_bool;
    }
}
//冷更新
static void updateParamDAQ()
{

    n_rtLoops=gRunParam.m_nLoops;
    for(int loopIndex=0;loopIndex<gRunParam.m_nLoops;loopIndex++)
    {
        rtLoop[loopIndex].pidControl.kp=gRunParam.m_loopParam[loopIndex]->pid_p;
        rtLoop[loopIndex].pidControl.ki=gRunParam.m_loopParam[loopIndex]->pid_i;
        rtLoop[loopIndex].pidControl.kd=gRunParam.m_loopParam[loopIndex]->pid_d;
        rtLoop[loopIndex].pidControl.param1=gRunParam.m_loopParam[loopIndex]->param1;
        rtLoop[loopIndex].pidControl.param2=gRunParam.m_loopParam[loopIndex]->param2;
        rtLoop[loopIndex].pidControl.param3=gRunParam.m_loopParam[loopIndex]->param3;
        rtLoop[loopIndex].pidControl.param4=gRunParam.m_loopParam[loopIndex]->param4;
        rtLoop[loopIndex].pidControl.integral=0;
        rtLoop[loopIndex].pidControl.prev_error=0;
        rtLoop[loopIndex].pidControl.integral_limit=10000;//积分限制 要上位机给？
        rtLoop[loopIndex].pidControl.output_limit_min=gRunParam.m_loopParam[loopIndex]->voltage_lowLimit*1000;
        rtLoop[loopIndex].pidControl.output_limit_max=gRunParam.m_loopParam[loopIndex]->voltage_limit*1000;
        rtLoop[loopIndex].active_curve_index=gRunParam.m_loopParam[loopIndex]->active_curve_index;//上位机下发
        rtLoop[loopIndex].enabled=gRunParam.m_loopParam[loopIndex]->enabled;
        rtLoop[loopIndex].output_chnlIndex=gRunParam.m_loopParam[loopIndex]->output_chnlIndex;
        rtLoop[loopIndex].n_input_chnls=gRunParam.m_loopParam[loopIndex]->n_input_chnls;
        memcpy(&rtLoop[loopIndex].intput_chnlsIndex[0],&gRunParam.m_loopParam[loopIndex]->intput_chnlsIndex[0],sizeof(int)*rtLoop[loopIndex].n_input_chnls);
        rtLoop[loopIndex].feedback_chnl=gRunParam.m_loopParam[loopIndex]->feedback_chnl;
        rtLoop[loopIndex].target_chnl=gRunParam.m_loopParam[loopIndex]->target_chnl;
        rtLoop[loopIndex].error_chnl=gRunParam.m_loopParam[loopIndex]->error_chnl;
        rtLoop[loopIndex].other_chnl=gRunParam.m_loopParam[loopIndex]->other_chnl;
        strcpy( rtLoop[loopIndex].algorithm,gRunParam.m_loopParam[loopIndex]->algorithm);
        printf("%s \n",rtLoop[loopIndex].algorithm);
        if(strstr(rtLoop[loopIndex].algorithm,"PID")!=NULL)
        {
            rtLoop[loopIndex].algorithmExcute =executeAlgorithmPID;
        }
        else if(strstr(rtLoop[loopIndex].algorithm,"冷壁一部头罩")!=NULL)
        {
            rtLoop[loopIndex].algorithmExcute =executeAlgorithm6;
            memcpy(&rtLoop[loopIndex].algorithmConstans,&gRunParam.algorithmConstans[5],sizeof(AlgorithmConstants));
        }
        else if(strstr(rtLoop[loopIndex].algorithm,"温度切换点温仪")!=NULL)
        {
            rtLoop[loopIndex].algorithmExcute =executeAlgorithm5;
            memcpy(&rtLoop[loopIndex].algorithmConstans,&gRunParam.algorithmConstans[4],sizeof(AlgorithmConstants));
        }
        else if(strstr(rtLoop[loopIndex].algorithm,"冷壁热流纯K偶")!=NULL)
        {
            rtLoop[loopIndex].algorithmExcute =executeAlgorithm4;
            memcpy(&rtLoop[loopIndex].algorithmConstans,&gRunParam.algorithmConstans[3],sizeof(AlgorithmConstants));
        }
        else if(strstr(rtLoop[loopIndex].algorithm,"点温仪")!=NULL)
        {
            rtLoop[loopIndex].algorithmExcute =executeAlgorithm3;
            memcpy(&rtLoop[loopIndex].algorithmConstans,&gRunParam.algorithmConstans[2],sizeof(AlgorithmConstants));
        }
        else if(strstr(rtLoop[loopIndex].algorithm,"K偶")!=NULL)
        {
            rtLoop[loopIndex].algorithmExcute =executeAlgorithm2;
            memcpy(&rtLoop[loopIndex].algorithmConstans,&gRunParam.algorithmConstans[1],sizeof(AlgorithmConstants));
        }
        else if(strstr(rtLoop[loopIndex].algorithm,"热流")!=NULL)
        {
            rtLoop[loopIndex].algorithmExcute =executeAlgorithm1;
            memcpy(&rtLoop[loopIndex].algorithmConstans,&gRunParam.algorithmConstans[0],sizeof(AlgorithmConstants));
        }
        memset(& rtLoop[loopIndex].frameOutputs,0,sizeof(AlgorithmFrameOutputs));

        rtLoop[loopIndex].n_curve1=gRunParam.m_loopParam[loopIndex]->n_curve1;
        rtLoop[loopIndex].n_curve2=gRunParam.m_loopParam[loopIndex]->n_curve2;
        //把工况系数和曲线系数直接乘进去
        for(int temp=0;temp<rtLoop[loopIndex].n_curve1;temp++)
        {
            rtLoop[loopIndex].curve1[temp].index=gRunParam.m_loopParam[loopIndex]->curve1[temp].index;
            rtLoop[loopIndex].curve1[temp].t=gRunParam.m_loopParam[loopIndex]->curve1[temp].t;
            rtLoop[loopIndex].curve1[temp].y=gRunParam.m_loopParam[loopIndex]->curve1[temp].y*gRunParam.m_loopParam[loopIndex]->condition_coeff*gRunParam.m_loopParam[loopIndex]->curve1_coeff;
            //  rtLoop[loopIndex].curve1[temp].y=gRunParam.m_loopParam[loopIndex]->curve1[temp].y;

            strcpy(rtLoop[loopIndex].curve1[temp].unit,gRunParam.m_loopParam[loopIndex]->curve1[temp].unit);
        }
        for(int temp=0;temp<rtLoop[loopIndex].n_curve2;temp++)
        {
            rtLoop[loopIndex].curve2[temp].index=gRunParam.m_loopParam[loopIndex]->curve2[temp].index;
            rtLoop[loopIndex].curve2[temp].t=gRunParam.m_loopParam[loopIndex]->curve2[temp].t;
            rtLoop[loopIndex].curve2[temp].y=gRunParam.m_loopParam[loopIndex]->curve2[temp].y*gRunParam.m_loopParam[loopIndex]->condition_coeff*gRunParam.m_loopParam[loopIndex]->curve2_coeff;
            strcpy(rtLoop[loopIndex].curve2[temp].unit,gRunParam.m_loopParam[loopIndex]->curve2[temp].unit);
            //  rtLoop[loopIndex].curve2[temp].y=gRunParam.m_loopParam[loopIndex]->curve2[temp].y;
        }
        printf("工况系数:%f 曲线系数1 %f 曲线系数2 %f\n",gRunParam.m_loopParam[loopIndex]->condition_coeff,gRunParam.m_loopParam[loopIndex]->curve1_coeff,gRunParam.m_loopParam[loopIndex]->curve2_coeff);

        rtLoop[loopIndex].manual_mode=gRunParam.m_loopParam[loopIndex]->manual_mode;
        rtLoop[loopIndex].manual_output=gRunParam.m_loopParam[loopIndex]->manual_output;
        rtLoop[loopIndex].di_bool=gRunParam.m_loopParam[loopIndex]->di_bool;
        rtLoop[loopIndex].n_curveIndex1=0;
        rtLoop[loopIndex].n_curveIndex2=0;

        rtLoop[loopIndex].pidControl.prev_prev_error = 0; // e(k-2) = e(k-1)
        rtLoop[loopIndex].pidControl.prev_error = 0;               // e(k-1) = e(k)
        rtLoop[loopIndex].pidControl.prev_output = 0;
        memset(& rtLoop[loopIndex].frameOutputs,0,sizeof(AlgorithmFrameOutputs));
        d_mTime=0;
    }
}
static void updateLGORITHM()
{
    for(int loopIndex=0;loopIndex<gRunParam.m_nLoops;loopIndex++)
    {

        strcpy( rtLoop[loopIndex].algorithm,gRunParam.m_loopParam[loopIndex]->algorithm);
        printf("更新算法库指令: %s \n",rtLoop[loopIndex].algorithm);
        if(strstr(rtLoop[loopIndex].algorithm,"冷壁一部头罩")!=NULL)
        {
            rtLoop[loopIndex].algorithmExcute =executeAlgorithm6;
            memcpy(&rtLoop[loopIndex].algorithmConstans,&gRunParam.algorithmConstans[5],sizeof(AlgorithmConstants));
        }
        else if(strstr(rtLoop[loopIndex].algorithm,"温度切换点温仪")!=NULL)
        {
            rtLoop[loopIndex].algorithmExcute =executeAlgorithm5;
            memcpy(&rtLoop[loopIndex].algorithmConstans,&gRunParam.algorithmConstans[4],sizeof(AlgorithmConstants));
        }
        else if(strstr(rtLoop[loopIndex].algorithm,"冷壁热流纯K偶")!=NULL)
        {
            rtLoop[loopIndex].algorithmExcute =executeAlgorithm4;
            memcpy(&rtLoop[loopIndex].algorithmConstans,&gRunParam.algorithmConstans[3],sizeof(AlgorithmConstants));
        }
        else if(strstr(rtLoop[loopIndex].algorithm,"点温仪")!=NULL)
        {
            rtLoop[loopIndex].algorithmExcute =executeAlgorithm3;
            memcpy(&rtLoop[loopIndex].algorithmConstans,&gRunParam.algorithmConstans[2],sizeof(AlgorithmConstants));
        }
        else if(strstr(rtLoop[loopIndex].algorithm,"K偶")!=NULL)
        {
            rtLoop[loopIndex].algorithmExcute =executeAlgorithm2;
            memcpy(&rtLoop[loopIndex].algorithmConstans,&gRunParam.algorithmConstans[1],sizeof(AlgorithmConstants));
        }
        else if(strstr(rtLoop[loopIndex].algorithm,"热流")!=NULL)
        {
            rtLoop[loopIndex].algorithmExcute =executeAlgorithm1;
            memcpy(&rtLoop[loopIndex].algorithmConstans,&gRunParam.algorithmConstans[0],sizeof(AlgorithmConstants));
        }
        memset(& rtLoop[loopIndex].frameOutputs,0,sizeof(AlgorithmFrameOutputs));
    }
}
/*
 * 开始采集
 */
static void startDAQ()
{
    //    if(UpdateHardware()!=0)
    //    {
    //        isSampling = false;
    //        int i = DEVICEDAQSTOPED;
    //        kfifo_write_pack(&gR2MFifoState, &i, sizeof(i));
    //        pthread_kill(thr_main, sigStateR2M); //发出给主线程的信号
    //        return;
    //    }
    printf("UpdateHardware Sucess!!!!!!!!!!!!!!!!!!!!\n");
    memset(&dataPtrIndex[0], 0, MAX_CHANNEL * 4);
    memset(&dataDAQPoints[0], 0, MAX_CHANNEL * 8);
    memset(&dataAllPoints[0], 0, MAX_CHANNEL * 8);

    channelPtrMoveFirst(); //将通道指针指向结果数组各自初始位置
    send_rxsize = frameCount + MAX_CHANNEL * 4 + MAX_CHANNEL * 8; //处理后数据包大小+8字节时间
    printf("rt channel info : channelCount %d  send_rxsize %d  frameCOunt %d\n", channelCount, send_rxsize,frameCount);



    gRunParam.nStartDaqTime = GetTimeStampU();
    isSampling = true;


    // 计算每个表达式
    for (int j = 0; j < gRunParam.m_chnldefs.m_nVirtualChnls; j++)
    {
        int error = 0;
        // te_expr* expr = gRunParam.m_chnldefs.m_VirtualChnlParam[j]->vparams.expr;
        gRunParam.m_chnldefs.m_VirtualChnlParam[j]->vparams.expr= te_compile(gRunParam.m_chnldefs.m_VirtualChnlParam[j]->vparams.formula_text,gRunParam.m_chnldefs.m_VirtualChnlParam[j]->vparams.vars,gRunParam.m_chnldefs.m_VirtualChnlParam[j]->vparams.n_count, &error);
        if (gRunParam.m_chnldefs.m_VirtualChnlParam[j]->vparams.expr)
        {
            double result = te_eval(gRunParam.m_chnldefs.m_VirtualChnlParam[j]->vparams.expr); // 计算结果
            printf("表达式 %d: %s 计算结果: %f\n", j + 1,gRunParam.m_chnldefs.m_VirtualChnlParam[j]->vparams.formula_text, result);
        }
        else
        {
            // 打印语法树
            printf("表达式的语法树:\n");
            te_print(gRunParam.m_chnldefs.m_VirtualChnlParam[j]->vparams.expr);
            printf("表达式 %d: %s 错误代码: %d\n", j + 1,gRunParam.m_chnldefs.m_VirtualChnlParam[j]->vparams.formula_text, error);
        }
    }



    // startDaqDMA();
    int i = DAQSTARTED;
    kfifo_write_pack(&gR2MFifoState, &i, sizeof(i));
    pthread_kill(thr_main, sigStateR2M); //发出给主线程的信号

}

/*
 * 停止采集
 */
static void stopDAQ()
{
    pxi_sys_deinit();
    isSampling = false; //改变状态
    int i = DAQSTOPED;
    kfifo_write_pack(&gR2MFifoState, &i, sizeof(i));
    pthread_kill(thr_main, sigStateR2M); //发出给主线程的信号
}
/*
 * 读取其他线程传递过来的指令并进行相应操作
 */
static char gStatebuf[STATE_MAX_SIZE];
static void readCMD()
{
    int readFifolength = kfifo_read_pack(&gM2RFifoState, &gStatebuf, STATE_FIFO_SIZE);	//收到信号，从fifo中读取到数组
    if (readFifolength <= 0)
        return;
    if (readFifolength != sizeof(int))
    {
        printf("rtthread recieve cmd != sizeof plan struct!\n");
        return;
    }
    int i = 0;
    memcpy(&i, &gStatebuf, readFifolength);
    if (i == STARTDAQ)
    {
        printf("rt recive start daq\n");
        if (!isSampling)
            startDAQ();
        else
        {
            int b = DAQSTARTED;
            kfifo_write_pack(&gR2MFifoState, &b, sizeof(b));
            pthread_kill(thr_main, sigStateR2M); //发出给主线程的信号
        }
    }
    else if (i == STOPDAQ)
    {
        printf("rt recive stop daq\n");
        if (isSampling)
            stopDAQ();
        else
        {
            int b = DAQSTOPED;
            kfifo_write_pack(&gR2MFifoState, &b, sizeof(b));
            pthread_kill(thr_main, sigStateR2M); //发出给主线程的信号
        }
    }
    else if(i == UPDATEPARAM)
    {
        updateParamDAQ();
    }
    else if(i==HOTUPDATEPARAM)
    {
        updateHotParamDAQ();
    }
    else if(i==TOTALINTERLOCK_OPEN)
    {
        //发送DIO

        //还需要回令确认发送出去

        allow_da=true;
    }
    else if(i==TOTALINTERLOCK_CLOSE)
    {
        //发送DIO

        //还需要回令确认发送出去

        allow_da=false;
    }
    else if(i==STARTTEST)
    {
        allow_timeRun=true;
        test_status=1;
    }
    else if(i==PAUSETEST)
    {
        allow_timeRun=false;
        test_status=2;
    }
    else if(i==STOPTEST)
    {
        stopTest();
    }
    else if (i==UPDATEALGORITHM)
    {
        updateLGORITHM();
    }


}

static int initFifo()
{
    int ret = 0;
    m_r2mFifoStateBuffer = (char*) malloc(STATE_FIFO_SIZE);
    if (m_r2mFifoStateBuffer == NULL)
    {
        printf("m_r2mFifoStateBuffer malloc fail\n");
        return -1;
    }
    ret = kfifo_init(&gR2MFifoState, m_r2mFifoStateBuffer, STATE_FIFO_SIZE, 1);
    if (ret != 0)
    {
        printf("gR2MFifoState init fail\n");
        return -1;
    }
    m_r2pFifoDataBuffer = (char*) malloc(DATA_FIFO_SIZE);
    if (m_r2pFifoDataBuffer == NULL)
    {
        printf("m_r2pFifoDataBuffer malloc fail\n");
        return -1;
    }
    ret = kfifo_init(&gR2PFifoData, m_r2pFifoDataBuffer, DATA_FIFO_SIZE, 1);
    if (ret != 0)
    {
        printf("gR2PFifoData init fail\n");
        return -1;
    }
    return 0;
}
static void uninitFifo()
{
    kfifo_free(&gR2MFifoState);
    kfifo_free(&gR2PFifoData);
    printf("rt fifoBuffer free success\n");
}
/*
 rt线程运行主函数
 */
void* rt_run(void *arg)
{
    if (initFifo() != 0)
    {
        return NULL;
    }
    printf("rt_thread ready\n");
    thread_wait_start();	//主线程开启
    printf("rt_thread go\n");
    while (run_untill_shutDown)
    {
        readCMD();
        if (!isSampling)
        {
            sleep_ms(100);
        }
        else
        {
            int readCount = doSample();
            //    printf("%lld %lld\n",dataDAQPoints[44],dataAllPoints[44]);
            if (readCount >= 0) //采样成功
            {
                //    begin = GetTimeStampU();
                if (kfifo_write_pack(&gR2PFifoData, &dataBuf, send_rxsize) == PIPE_FULL)
                {
                    stopDAQ();
                    printf("DAQPIPE_FULL fail!!!!!!!!!!!! %d\n", send_rxsize);
                    int i = DAQPIPE_FULL;
                    kfifo_write_pack(&gR2MFifoState, &i, sizeof(i));
                    pthread_kill(thr_main, sigStateR2M); //发出给主线程的信号
                }
                else
                {
                    pthread_kill(thr_process, sigDataR2P); //发出给处理线程的信号
                }
            }
            else
            {
                printf("readCountSample fail!!!!!!!!!!!!\n");
                int i = DAQSAMPLEFALSE;
                kfifo_write_pack(&gR2MFifoState, &i, sizeof(i));
                pthread_kill(thr_main, sigStateR2M); //发出给主线程的信号
            }
            channelPtrMove();

            clock_gettime(CLOCK_MONOTONIC, &time7);
          //  printfTime1();
            // end = GetTimeStampU() - begin;
            //    if (end > LOOP_TIME_NEED * 1000+30)
            //  printf(" rt speed more than %dms!: %lld us \n", LOOP_TIME_NEED , end);

            //printfTime1();
        }
    }
    pxi_sys_deinit();
    uninitFifo();
    return NULL;
}
