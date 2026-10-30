#pragma once
/* Set RT_PID_SINE_ENABLE to 0 to restore the original PID curve path.
 * Amplitude/offset use feedback engineering units, NOT necessarily mV.
 * Requested defaults: 1 Hz, amplitude 1000 mV, offset 100 mV.
 */
#ifndef RT_PID_SINE_ENABLE
#define RT_PID_SINE_ENABLE 1
#endif
#ifndef RT_PID_PP_COMP_ENABLE
#define RT_PID_PP_COMP_ENABLE 1
#endif
#ifndef RT_PID_SINE_HZ
#define RT_PID_SINE_HZ 1.0
#endif
#ifndef RT_PID_SINE_AMPLITUDE
#define RT_PID_SINE_AMPLITUDE 1000.0
#endif
#ifndef RT_PID_SINE_OFFSET
#define RT_PID_SINE_OFFSET 100.0
#endif
#define RT_PID_PP_RATE       0.10
#define RT_PID_PP_MAX_STEP   0.05
#define RT_PID_PP_GAIN_MIN   0.50
#define RT_PID_PP_GAIN_MAX   2.00
#define RT_PID_PP_DEADBAND   0.02
#define RT_PID_PP_MIN_RATIO  0.01

static int doSample();
static void startDAQ();
static void stopDAQ();
static void channelPtrMove();
void* rt_run(void *arg); //rt线程运行函数

