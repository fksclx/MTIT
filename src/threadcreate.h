#pragma once

#include <pthread.h>
#include "kfifo.h"
int mem_lockall(); //禁止使用磁盘映射虚拟内存
int mem_unlockall(); //解除禁止使用磁盘映射虚拟内存
pthread_t thread_create(void *(*func)(void *), void *arg, int prio, int cpu); //创建线程
int thread_join(pthread_t thread); //线程退出并销毁
int thread_set_priority(pthread_t pthread, int prio); //设置优先级
void thread_wait_init(int thread_num); //设置拦截线程个数
void thread_wait_start(); //发令，开启所有线程
void thread_wait_destroy(); //在线程都开启后，需要退出之前调用

