#include "threadcreate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sched.h>
static pthread_barrier_t start_barrier; //计数锁，充当栏杆，锁住提前初始化完成的线程，等待发令同时开始
/*
 使其不在使用硬盘虚拟内存，更快速
 */
int mem_lockall()
{
	if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) //未锁住
	{
		perror("failed to lock memory\n");
		return -1; //未锁住返回-1
	}
	return 0; //锁住返回0
}
/*
 解锁开始时使用的锁
 */
int mem_unlockall()
{
	if (munlockall() != 0)
	{
		perror("failed to unlock memory\n");
		return -1; //未解锁返回-1
	}
	return 0; //解锁成功返回1
}
/*
 创建线程函数，传递线程运行函数
 */
pthread_t thread_create(void *(*func)(void *), void *arg, int prio, int cpu)
{
	struct sched_param param; //设置优先级
	pthread_t thread;
	pthread_attr_t attr;
	int ret;
	param.sched_priority = prio;
	pthread_attr_init(&attr); //初始化线程对象

	cpu_set_t cpuset;
	/*
	 if(cpu == 0 || cpu == 1)
	 CPU_SET(cpu, &cpuset);
	 else
	 {
	 CPU_SET(0, &cpuset);
	 CPU_SET(1, &cpuset);
	 }

	 cpu_set_t cpuSet;
	 CPU_ZERO(&cpuSet);
	 if (sched_getaffinity(0, sizeof(cpuSet), &cpuSet) == -1) {
	 perror("sched_getaffinity");
	 return 1;
	 }
	 printf("可选择的逻辑 CPU 核数：%d\n", CPU_COUNT(&cpuSet));

	 if(0!=pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset))//指定绑定对象
	 {
	 printf("!set cpu false!!!!!!!!!!!!\n");
	 }
	 */

	pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED); //必须设置这个优先级才有用，显示调度
	pthread_attr_setschedpolicy(&attr, SCHED_FIFO); //默认用fifo
	pthread_attr_setschedparam(&attr, &param); //设置优先级
	ret = pthread_create(&thread, &attr, func, (void *) arg);
	if (ret)
	{
		printf("pthread_create failed: %d (%s)\n", ret, strerror(ret));
		pthread_attr_destroy(&attr); //创建失败需要摧毁
		return 0;
	}
	pthread_attr_destroy(&attr); //摧毁线程属性，释放内存

	pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset); //指定绑定对象

	return thread;
}
/*
 线程退出函数，非0返回错误
 */
int thread_join(pthread_t thread)
{
	char *returnVal;
	int returnval; //返回值
	if ((returnval = pthread_join(thread, (void**) &returnVal)) != 0) //非0线程退出错误
	{
		printf("error joining thread: %s\n", strerror(returnval));  //1st optiop
		perror("error joining thread:");  //2nd option
		printf("error joining thread: %m\n");  //3rd option
	}
	return returnval;
}
/*
 设置线程优先级，负数为默认，正数采用fifo
 */
int thread_set_priority(pthread_t pthread, int prio)
{
	struct sched_param sched_param;  //进程调度结构体
	sched_param.sched_priority = prio;  //优先级必须介于sched_get_priority_min()和sched_get_priority_max()
	int policy;
	//FIFO:一直保证最高运行，除非有更优先的线程 //RR 实时调度，每次cpu重新分配时间块
	policy = (prio > 0) ? SCHED_FIFO : SCHED_OTHER;  //先入先出还是other，默认线程创建后是other不支持优先级设置
	return pthread_setschedparam(pthread, policy, &sched_param);
}
/*
 等待线程初始化，传递线程个数，后续每个线程准备完毕需要调用一次
 */
void thread_wait_init(int thread_num)
{
	pthread_barrier_init(&start_barrier, NULL, thread_num);  //NULL 表示不配具体参数
	return;
}
/*
 需要每个线程调用一次，用于告知已经到栅栏。都到齐后自动运行全部线程
 */
void thread_wait_start()
{
	pthread_barrier_wait(&start_barrier);
	return;
}
/*
 避免无法发令时调用，销毁init时创建的栅栏，释放内存
 或发令后线程退出前调用
 */
void thread_wait_destroy()
{
	pthread_barrier_destroy(&start_barrier);
	return;
}

