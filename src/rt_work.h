#pragma once
static int doSample();
static void startDAQ();
static void stopDAQ();
static void channelPtrMove();
void* rt_run(void *arg); //rt线程运行函数

