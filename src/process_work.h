#pragma once
static int initEpoll();
static int initPipeSignal1();
static int initPipeSignal2();
void* process_run(void *arg);
void recvRtDataSignal();

void applyZero();
void countZero();
void countPhysical();
void applyWireCompensate();
void countShuntCoff();

float getVoltageByTemperature(float InputValue, int channelIndex);

void countLimit(float *data, int channelIndex, int frameSize);
void countTemprature(float *data, int channelIndex, int frameSize);
void sendDataSave(int size);
void sendStartSave();
void sendStopSave();
static void recvMainCMDSignal();


