/*
 * hardware.c
 *
 *  Created on: 2022��3��11��
 *      Author: biseadmin
 */

#include "hardware.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <aio.h>
#include <stdio.h>
#include <unistd.h>
#include "upper_computer/common.h"
#include "xilinx/gpio/xgpio.h"
#include "card/cards.h"
#include "card/pxie_7021.h"
#include "card/pxie_7022.h"
#include "card/pxie_7023.h"
#include "board/adc/ad4134.h"
struct aiocb *aiocb_list[MAX_PXI_SLOTS];
struct pxi_sys_t g_pxi_system;
static int64_t timebeg0 = 0;
extern struct RunningParam gRunParam;
volatile void *writeData;
volatile void *readData;
//开映射
static void* mmap_control(int device, long mapsize)
{
    void *vir_addr;
    vir_addr = mmap(0, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED, device, 0);
    return vir_addr;
}
//关映射
static int munmap_control(void *user_base, long mapsize)
{
    return munmap(user_base, mapsize);
}

void pxi_reg_write(uintptr_t address, unsigned int val)
{
    unsigned int writeval = htoll(val);
    *((unsigned int*) (address)) = writeval;
}

unsigned int pxi_reg_read(uintptr_t address)
{
    unsigned int read_result = *((unsigned int*) (address));
    read_result = ltohl(read_result);
    return read_result;
}

static int get_pxi_slot(void *user_base)
{
    int slot_gpio_chn = 1;
    uintptr_t base = (uintptr_t) user_base;

    return XGpio_ReadReg(
            base + PXI_GA_BASEADDR,
            ((slot_gpio_chn - 1) * XGPIO_CHAN_OFFSET) + XGPIO_DATA_OFFSET);
}

static int get_pxi_type(void *user_base)
{
    int slot_gpio_chn = 1;
    uintptr_t base = (uintptr_t) user_base;

    return XGpio_ReadReg(
            base + PXI_GA_BASEADDR,
            ((slot_gpio_chn - 1) * XGPIO_CHAN_OFFSET) + XGPIO_DATA2_OFFSET);
}
//int RouteTrigger (int myTrigger, int myBusSegment)
//{
//    ViSession RsrcManager;
//    ViSession vi;
//    ViStatus status = VI_SUCCESS;
//
//    /* Open VISA Resource Manager and PXI Backplane Resource */
//    status = viOpenDefaultRM (&RsrcManager);
//    if (status < VI_SUCCESS)
//        goto ReserveDone;
//
//    status = viOpen (RsrcManager, "PXI0::2::BACKPLANE", 0, 0, &vi);
//    if (status < VI_SUCCESS)
//        goto CloseRMSession;
//
//    /* Set the Trigger ID and the Trigger Bus Attributes */
//    status = viSetAttribute (vi, VI_ATTR_TRIG_ID, myTrigger);
//    if (status < VI_SUCCESS)
//        goto CloseBackplaneSession;
//
//    status = viSetAttribute (vi, VI_ATTR_PXI_TRIG_BUS, myBusSegment);
//    if (status < VI_SUCCESS)
//        goto CloseBackplaneSession;
//
//    /* Reserve the Trigger */
//    status = viAssertTrigger (vi, VI_TRIG_PROT_RESERVE);
//    if (status < VI_SUCCESS)
//        printf("ReserveTrigger Failed\n");
////        goto CloseBackplaneSession;
//
//    /* TODO: Enter code that requires the trigger reserved here */
//
//    /* UnReserve the Trigger - only do this if we succeeded in Reserving */
////    status = viAssertTrigger (vi, VI_TRIG_PROT_UNRESERVE);
//
//CloseBackplaneSession:
//    /* Close the VISA Session to the PXI Backplane */
//    viClose (vi);
//CloseRMSession:
//    viClose (RsrcManager);
//ReserveDone:
//    return (status);
//}
//
//
//int UnreserveTrigger (int myTrigger, int myBusSegment)
//{
//    ViSession RsrcManager;
//    ViSession vi;
//    ViStatus status = VI_SUCCESS;
//
//    /* Open VISA Resource Manager and PXI Backplane Resource */
//    status = viOpenDefaultRM (&RsrcManager);
//    if (status < VI_SUCCESS)
//        goto ReserveDone;
//
//    status = viOpen (RsrcManager, "PXI0::2::BACKPLANE", 0, 0, &vi);
//    if (status < VI_SUCCESS)
//        goto CloseRMSession;
//
//    /* Set the Trigger ID and the Trigger Bus Attributes */
//    status = viSetAttribute (vi, VI_ATTR_TRIG_ID, myTrigger);
//    if (status < VI_SUCCESS)
//        goto CloseBackplaneSession;
//
//    status = viSetAttribute (vi, VI_ATTR_PXI_TRIG_BUS, myBusSegment);
//    if (status < VI_SUCCESS)
//        goto CloseBackplaneSession;
//
//    /* Reserve the Trigger */
////    status = viAssertTrigger (vi, VI_TRIG_PROT_RESERVE);
////    if (status < VI_SUCCESS)
////        printf("ReserveTrigger Failed\n");
////        goto CloseBackplaneSession;
//
//    /* TODO: Enter code that requires the trigger reserved here */
//
//    /* UnReserve the Trigger - only do this if we succeeded in Reserving */
//    status = viAssertTrigger (vi, VI_TRIG_PROT_UNRESERVE);
//
//CloseBackplaneSession:
//    /* Close the VISA Session to the PXI Backplane */
//    viClose (vi);
//CloseRMSession:
//    viClose (RsrcManager);
//ReserveDone:
//    return (status);
//}
int RouteTrigger (ViConstRsrc chassisname, ViInt16 myTrigger, int mySourceSegment, int myDestinationSegment)
{
    ViSession RsrcManager;
    ViSession vi;
    ViStatus status = VI_SUCCESS;

    /* Open VISA Resource Manager and PXI Backplane Resource */
    status = viOpenDefaultRM (&RsrcManager);
    if (status < VI_SUCCESS){
        printf("viOpenDefaultRM failed\n");
        goto RouteDone;}

    status = viOpen (RsrcManager, chassisname, 0, 0, &vi);  //
    if (status < VI_SUCCESS){
        printf("viOpen failed\n");
        goto CloseRMSession;}

    /* Set the Source and Destination bus PXI VISA Attributes */
    status = viSetAttribute (vi, VI_ATTR_PXI_SRC_TRIG_BUS, mySourceSegment);
    if (status < VI_SUCCESS){
        printf("viSetAttribute 1 failed\n");
        goto CloseBackplaneSession;}

    status = viSetAttribute (vi, VI_ATTR_PXI_DEST_TRIG_BUS,myDestinationSegment);
    if (status < VI_SUCCESS){
        printf("viSetAttribute 2 failed\n");
        goto CloseBackplaneSession;}

    /* Route the trigger */
    status = viMapTrigger (vi, myTrigger, myTrigger, VI_NULL);
    if (status < VI_SUCCESS){
        printf("viMapTrigger failed\n");
        goto CloseBackplaneSession;}

    /* TODO: Enter code that requires the trigger routed here */

    /* UnRoute the trigger - only do this if we succeeded in Routing */
    //status = viUnmapTrigger (vi, myTrigger, myTrigger);

    CloseBackplaneSession:
    /* Close your VISA Sessions */
    status = viClose (vi);
    CloseRMSession:
    status = viClose (RsrcManager);
    RouteDone:
    return (status);
}
int unRouteTrigger (ViConstRsrc chassisname, ViInt16 myTrigger, int mySourceSegment, int myDestinationSegment)
{
    ViSession RsrcManager;
    ViSession vi;
    ViStatus status = VI_SUCCESS;

    /* Open VISA Resource Manager and PXI Backplane Resource */
    status = viOpenDefaultRM (&RsrcManager);
    if (status < VI_SUCCESS){
        //printf("viOpenDefaultRM failed\n");
        goto RouteDone;}

    status = viOpen (RsrcManager, chassisname, 0, 0, &vi);  //
    if (status < VI_SUCCESS){
        //printf("viOpen failed\n");
        goto CloseRMSession;}

    /* Set the Source and Destination bus PXI VISA Attributes */
    status = viSetAttribute (vi, VI_ATTR_PXI_SRC_TRIG_BUS, mySourceSegment);
    if (status < VI_SUCCESS){
        //printf("viSetAttribute 1 failed\n");
        goto CloseBackplaneSession;}

    status = viSetAttribute (vi, VI_ATTR_PXI_DEST_TRIG_BUS,myDestinationSegment);
    if (status < VI_SUCCESS){
        //printf("viSetAttribute 2 failed\n");
        goto CloseBackplaneSession;}


    /* TODO: Enter code that requires the trigger routed here */

    /* UnRoute the trigger - only do this if we succeeded in Routing */
    status = viUnmapTrigger (vi, myTrigger, myTrigger);

    CloseBackplaneSession:
    /* Close your VISA Sessions */
    status = viClose (vi);
    CloseRMSession:
    status = viClose (RsrcManager);
    RouteDone:
    return (status);
}
//获取单张板卡类型
static int pxi_prob_dev(struct pxi_card_t *p, int num)
{
    int h_dev;
    char dev_name[256];

    if (p == NULL)
        return -1;

    sprintf(dev_name, "/dev/xdma%d_user", num);
    h_dev = open(dev_name, O_RDWR | O_SYNC);
    if (h_dev < 0)
        return -1;
    p->h_user = h_dev;
    p->user_base = mmap_control(h_dev, MAP_SIZE);
    p->card_num = num;
    p->phy_slot = get_pxi_slot(p->user_base);
    p->card_type = get_pxi_type(p->user_base);

    printf("card type %x\n",(p->card_type));
    printf("card type %x\n",(p->card_type & 0xFFFF0000));
    if ((p->card_type & 0xFFFF0000) == 0x70210000) {
        p->init = &pxie_7021_init;
        p->deinit = &pxie_7021_deinit;
        p->read = &pxie_7021_read;
        p->trig = &pxie_7021_trig;
        return 1;
    }
    else if ((p->card_type & 0xFFFF0000) == 0x70220000) {
        p->init = &pxie_7022_init;
        p->deinit = &pxie_7022_deinit;
        p->write = &pxie_7022_write;
        p->trig = &pxie_7022_trig;
        p->sync = &pxie_7022_sync;
        return 2;
    }

    else if ((p->card_type) == 1) {
        p->init = &pxie_7022_init;
        p->deinit = &pxie_7022_deinit;
        p->write = &pxie_7022_write;
        p->trig = &pxie_7022_trig;
        p->sync = &pxie_7022_sync;
        return 2;
    }
    else if((p->card_type  & 0xFFFF0000) == 0x70230000){ // 16 channel per Card
        p->init = &pxie_7023_init;
        p->deinit = &pxie_7023_deinit;
        p->trig = &pxie_7023_odrtrig;
        p->trig_o = &pxie_7023_odrtrig_o;
        p->sync = &pxie_7023_sync;
        p->syncin = &pxie_7023_syncIN;
        p->setCfg = &pxie_7023_CfgSet;
        p->resetCfg = &pxie_7023_CfgReset;
        p->deinittmp = &pxie_7023_deinit_tmp;
        p->read = &pxie_7023_read;
        //   p->inittmp = &pxie_7023_init_tmp;
        return 1;
    }
    else {
        printf("unknown PXIE card type\n");
    }
    return 0;
}
//板卡排序
void pxi_sys_sort()
{
    {
        printf("排序读取卡\n");
        int size = g_pxi_system.n_rCards;
        int j, i;
        struct pxi_card_t tem_card;
        for (i = 0; i < size - 1; i++)
        {
            printf("%d \n",size);

            int count = 0;
            for (j = 0; j < size - 1 - i; j++)
            {
                printf("卡槽%d 卡槽%d\n",g_pxi_system.r_cards[j].phy_slot, g_pxi_system.r_cards[j + 1].phy_slot);
                if (g_pxi_system.r_cards[j].phy_slot > g_pxi_system.r_cards[j + 1].phy_slot)
                {
                    printf("交换顺序\n");
                    memcpy(&tem_card, &(g_pxi_system.r_cards[j]), sizeof(struct pxi_card_t));
                    memcpy(&(g_pxi_system.r_cards[j]), &(g_pxi_system.r_cards[j + 1]),
                           sizeof(struct pxi_card_t));
                    memcpy(&(g_pxi_system.r_cards[j + 1]), &tem_card,
                           sizeof(struct pxi_card_t));

                    count = 1;
                }
            }
            if (0 == count)
                break;
        }
    }
    {
        printf("排序写入\n");
        int size = g_pxi_system.n_wCards;
        int j, i;
        struct pxi_card_t tem_card;
        for (i = 0; i < size - 1; i++)
        {
            printf("%d \n",size);

            int count = 0;
            for (j = 0; j < size - 1 - i; j++)
            {
                printf("卡槽%d 卡槽%d\n",g_pxi_system.w_cards[j].phy_slot, g_pxi_system.w_cards[j + 1].phy_slot);
                if (g_pxi_system.w_cards[j].phy_slot > g_pxi_system.w_cards[j + 1].phy_slot)
                {
                    printf("交换顺序\n");
                    memcpy(&tem_card, &(g_pxi_system.w_cards[j]), sizeof(struct pxi_card_t));
                    memcpy(&(g_pxi_system.w_cards[j]), &(g_pxi_system.w_cards[j + 1]),
                           sizeof(struct pxi_card_t));
                    memcpy(&(g_pxi_system.w_cards[j + 1]), &tem_card,
                           sizeof(struct pxi_card_t));

                    count = 1;
                }
            }
            if (0 == count)
                break;
        }
    }

}
//获取所有板卡类型
static void pxi_sys_init_prob()
{
    g_pxi_system.n_cards = 0;
    g_pxi_system.n_rCards = 0;
    g_pxi_system.n_wCards = 0;
    g_pxi_system.n_IOCards = 0;
    for (int i = 0; i < MAX_PXI_SLOTS; i++)
    {
        struct pxi_card_t temp_card;
        temp_card.h_user = -1;
        int ret = pxi_prob_dev(&temp_card, i);//根据板卡类型创建
        if (ret < 0)
        {
            continue;
        }
        else
        {
            g_pxi_system.n_cards ++;
        }
        if (ret == 1)
        {
            memcpy(&(g_pxi_system.r_cards[g_pxi_system.n_rCards]), &temp_card, sizeof(struct pxi_card_t));
            printf("probe pxi read card %d in slot %d\n", i, g_pxi_system.r_cards[g_pxi_system.n_rCards].phy_slot);
            g_pxi_system.n_rCards++;
        }
        else if (ret == 2)
        {
            memcpy(&(g_pxi_system.w_cards[g_pxi_system.n_wCards]), &temp_card, sizeof(struct pxi_card_t));
            printf("probe pxi write card %d in slot %d\n", i, g_pxi_system.w_cards[g_pxi_system.n_wCards].phy_slot);
            g_pxi_system.n_wCards++;
        }
    }
    printf("total %d pxi cards %d read card %d write card\n", g_pxi_system.n_cards, g_pxi_system.n_rCards, g_pxi_system.n_wCards);
}

//板卡反初始化
void pxi_sys_deinit()
{
    printf("pxi_sys_deinit\n");
    struct pxi_card_t *p;

    for (int i = 0; i < g_pxi_system.n_rCards; i++)
    {
        p = &g_pxi_system.r_cards[i];
        p->deinit(p);
    }
    for (int i = 0; i < g_pxi_system.n_wCards; i++)
    {
        p = &g_pxi_system.w_cards[i];
        p->deinit(p);
    }
    p = &g_pxi_system.r_cards[0];
    write_freq_high(p->user_base, 0);
    write_freq_low(p->user_base, 0);
    for(int i = 0; i < g_pxi_system.n_rCards; i++)
    {
        p = &g_pxi_system.r_cards[i];
        p->trig_o(p);
    }
    for (int i = 0; i < g_pxi_system.n_rCards; i++)
    {
        p = &g_pxi_system.r_cards[i];
        munmap_control(p->user_base, MAP_SIZE);
        close(p->h_user);
    }
    for (int i = 0; i < g_pxi_system.n_wCards; i++)
    {
        p = &g_pxi_system.w_cards[i];
        munmap_control(p->user_base, MAP_SIZE);
        close(p->h_user);
    }

    printf("pxi_sys_deinit success\n");

    pxi_sys_init_prob();
    pxi_sys_sort();//板卡排序
}

void* init_card_thread(void* arg)
{
    thread_param_t *param = (thread_param_t*)arg;
    param->ret = param->card->init(param->card, param->samplingRate, param->senserType);
    printf("Thread for card %d finished, ret = %d\n", param->cardIndex, param->ret);
    return NULL;
}
void readDeviceCards()
{
    ViConstRsrc chassisname = "PXI0::1::BACKPLANE";
    RouteTrigger (chassisname,0,1,2);
    RouteTrigger (chassisname,1,1,2);
    RouteTrigger (chassisname,2,1,2);
    RouteTrigger (chassisname,3,1,2);

    RouteTrigger (chassisname,0,2,3);
    RouteTrigger (chassisname,1,2,3);
    RouteTrigger (chassisname,2,2,3);
    RouteTrigger (chassisname,3,2,3);

    //    ViConstRsrc chassisname = "PXI0::2::BACKPLANE";
    //    RouteTrigger (chassisname,0,2,1);
    //    RouteTrigger (chassisname,1,2,1);
    //    RouteTrigger (chassisname,2,2,1);
    //    RouteTrigger (chassisname,3,2,1);
    pxi_sys_init_prob();

    pxi_sys_sort();//板卡排序

    gRunParam.m_nAnalogCards = g_pxi_system.n_rCards;
    if (g_pxi_system.n_rCards > 0)
    {
        gRunParam.m_AnalogCardParams = malloc(gRunParam.m_nAnalogCards * sizeof(AnalogCardParams*)); //开辟指针空间
        memset(gRunParam.m_AnalogCardParams, 0x00, gRunParam.m_nAnalogCards * sizeof(AnalogCardParams*));
    }

    gRunParam.m_nDigitalCards = g_pxi_system.n_wCards;
    if (g_pxi_system.n_wCards > 0)
    {
        gRunParam.m_DigitalCardParams = malloc(g_pxi_system.n_wCards * sizeof(DigitalCardParams*)); //开辟指针空间
        memset(gRunParam.m_DigitalCardParams, 0x00, g_pxi_system.n_wCards * sizeof(DigitalCardParams*));
    }

    gRunParam.m_nIOCards = g_pxi_system.n_IOCards;
    if ( g_pxi_system.n_IOCards > 0)
    {
        gRunParam.m_IOCardParams = malloc(g_pxi_system.n_IOCards * sizeof(IOCardParams*)); //开辟指针空间
        memset(gRunParam.m_IOCardParams, 0x00, g_pxi_system.n_IOCards * sizeof(IOCardParams*));
    }

    for (int i = 0; i < g_pxi_system.n_rCards; i++)
    {
        gRunParam.m_AnalogCardParams[i] = malloc(sizeof(AnalogCardParams));
        memset(gRunParam.m_AnalogCardParams[i], 0x00, sizeof(AnalogCardParams));
        gRunParam.m_AnalogCardParams[i]->nCardChannel = 16;
        gRunParam.m_AnalogCardParams[i]->nCardSlot = g_pxi_system.r_cards[i].phy_slot;
        gRunParam.m_AnalogCardParams[i]->nCardIndex = g_pxi_system.r_cards[i].card_num;
        strcpy(gRunParam.m_AnalogCardParams[i]->strCardModel,"Card_AD_1608");
        strcpy(gRunParam.m_AnalogCardParams[i]->strCardName, "模拟板卡");
    }
    for (int i = 0; i < g_pxi_system.n_wCards; i++)
    {
        gRunParam.m_DigitalCardParams[i] = malloc(sizeof(DigitalCardParams));
        memset(gRunParam.m_DigitalCardParams[i], 0x00, sizeof(DigitalCardParams));
        gRunParam.m_DigitalCardParams[i]->nCardChannel = 8;
        gRunParam.m_DigitalCardParams[i]->nCardSlot = g_pxi_system.w_cards[i].phy_slot;
        gRunParam.m_DigitalCardParams[i]->nCardIndex = g_pxi_system.w_cards[i].card_num;
        strcpy(gRunParam.m_DigitalCardParams[i]->strCardModel, "Card_DA_1608");
        strcpy(gRunParam.m_DigitalCardParams[i]->strCardName, "输出板卡");
    }
    for (int i = 0; i < g_pxi_system.n_IOCards; i++)
    {
        gRunParam.m_IOCardParams[i] = malloc(sizeof(IOCardParams));
        memset(gRunParam.m_IOCardParams[i], 0x00, sizeof(IOCardParams));
        gRunParam.m_IOCardParams[i]->nCardChannel =  8;
        gRunParam.m_IOCardParams[i]->nCardSlot =  g_pxi_system.io_cards[i].phy_slot;
        gRunParam.m_IOCardParams[i]->nCardIndex = g_pxi_system.io_cards[i].card_num;
        strcpy(gRunParam.m_IOCardParams[i]->strCardModel,  "Card_Digital_IO");
        strcpy(gRunParam.m_IOCardParams[i]->strCardName, "DIO板卡");
    }
    //差虚拟板卡
}
//void addVirtualCards()
//{
//    gRunParam.m_nVirtualCards = 1;
//
//    gRunParam.m_VirtualCardParams = malloc(  gRunParam.m_nVirtualCards * sizeof(VirtualCardParams*)); //开辟指针空间
//    memset(gRunParam.m_VirtualCardParams, 0x00,   gRunParam.m_nVirtualCards * sizeof(VirtualCardParams*));
//
//    for (int i = 0; i < gRunParam.m_nVirtualCards ; i++)
//    {
//        gRunParam.m_VirtualCardParams[i] = malloc(sizeof(VirtualCardParams));
//        memset(gRunParam.m_VirtualCardParams[i], 0x00, sizeof(VirtualCardParams));
//        gRunParam.m_VirtualCardParams[i]->nCardChannel =  16;
//        gRunParam.m_VirtualCardParams[i]->nCardSlot =  5;
//        gRunParam.m_VirtualCardParams[i]->nCardIndex = 5;
//        strcpy(gRunParam.m_VirtualCardParams[i]->strCardModel,  "Card_Digital_Virtual");
//        strcpy(gRunParam.m_VirtualCardParams[i]->strCardName, "虚拟计算板卡");
//    }
//
//}
void readDeviceChannels()
{
    int r_channelCount = 0,w_channelCount=0,io_channelCount=0,virtual_channelCount=0,target_channelCount=0,other_channelCount=0,error_channelCount=0;
    for (int i = 0; i < gRunParam.m_nAnalogCards; i++)
    {
        r_channelCount += gRunParam.m_AnalogCardParams[i]->nCardChannel;
    }
    for (int i = 0; i < gRunParam.m_nDigitalCards; i++)
    {
        w_channelCount += gRunParam.m_DigitalCardParams[i]->nCardChannel;
    }
    for (int i = 0; i < gRunParam.m_nDigitalCards; i++)
    {
        target_channelCount += gRunParam.m_DigitalCardParams[i]->nCardChannel;
    }
    for (int i = 0; i < gRunParam.m_nDigitalCards; i++)
    {
        other_channelCount += gRunParam.m_DigitalCardParams[i]->nCardChannel;
    }
    for (int i = 0; i < gRunParam.m_nDigitalCards; i++)
    {
        error_channelCount += gRunParam.m_DigitalCardParams[i]->nCardChannel;
    }
    for (int i = 0; i < gRunParam.m_nIOCards; i++)
    {
        io_channelCount += gRunParam.m_IOCardParams[i]->nCardChannel;
    }
    //    for (int i = 0; i < gRunParam.m_nVirtualCards; i++)
    //    {
    //        virtual_channelCount += gRunParam.m_VirtualCardParams[i]->nCardChannel;
    //    }
    virtual_channelCount += gRunParam.m_chnldefs.m_nVirtualChnls;
    int channelCount = r_channelCount+w_channelCount+io_channelCount+virtual_channelCount+target_channelCount+error_channelCount+other_channelCount;


    gRunParam.m_chnldefs.m_nChnls = channelCount;
    gRunParam.m_chnldefs.m_chnlParam = malloc(channelCount * sizeof(ChnlParams*));
    memset(gRunParam.m_chnldefs.m_chnlParam, 0x00, channelCount * sizeof(ChnlParams*));

    gRunParam.m_chnldefs.m_nAnalogChnls = r_channelCount;
    gRunParam.m_chnldefs.m_AnalogChnlParam = malloc(r_channelCount * sizeof(ChnlParams*));
    memset(gRunParam.m_chnldefs.m_AnalogChnlParam, 0x00, r_channelCount * sizeof(ChnlParams*));

    gRunParam.m_chnldefs.m_nDigitalChnls = w_channelCount;
    gRunParam.m_chnldefs.m_DigitalChnlParam = malloc(w_channelCount * sizeof(ChnlParams*));
    memset(gRunParam.m_chnldefs.m_DigitalChnlParam, 0x00, w_channelCount * sizeof(ChnlParams*));


    gRunParam.m_chnldefs.m_nTargetChnls = target_channelCount;
    gRunParam.m_chnldefs.m_TargetChnlParam = malloc(target_channelCount * sizeof(ChnlParams*));
    memset(gRunParam.m_chnldefs.m_TargetChnlParam, 0x00, target_channelCount * sizeof(ChnlParams*));


    gRunParam.m_chnldefs.m_nErrorChnls = error_channelCount;
    gRunParam.m_chnldefs.m_ErrorChnlParam = malloc(error_channelCount * sizeof(ChnlParams*));
    memset(gRunParam.m_chnldefs.m_ErrorChnlParam, 0x00, error_channelCount * sizeof(ChnlParams*));

    gRunParam.m_chnldefs.m_nOtherChnls = other_channelCount;
    gRunParam.m_chnldefs.m_OtherChnlParam = malloc(other_channelCount * sizeof(ChnlParams*));
    memset(gRunParam.m_chnldefs.m_OtherChnlParam, 0x00, other_channelCount * sizeof(ChnlParams*));

    gRunParam.m_chnldefs.m_nIOChnls = io_channelCount;
    gRunParam.m_chnldefs.m_IOChnlParam = malloc(io_channelCount * sizeof(ChnlParams*));
    memset(gRunParam.m_chnldefs.m_IOChnlParam, 0x00, io_channelCount * sizeof(ChnlParams*));


    gRunParam.m_chnldefs.m_VirtualChnlParam = malloc(virtual_channelCount * sizeof(ChnlParams*));
    memset(gRunParam.m_chnldefs.m_VirtualChnlParam, 0x00, virtual_channelCount * sizeof(ChnlParams*));


    int cardIndex=0;
    int chnlIndex_device=0;
    int chnlIndex_card=0;
    int r_chnlIndex=0;
    int w_chnlIndex=0;
    int t_chnlIndex=0;
    int io_chnlIndex=0;
    int other_chnlIndex=0;
    int error_chnlIndex=0;
    int virtual_chnlIndex=0;
    for (int i = 0; i < gRunParam.m_nAnalogCards; i++,cardIndex++)
    {
        chnlIndex_card=0;
        for(int y=0;y<gRunParam.m_AnalogCardParams[i]->nCardChannel;y++,chnlIndex_device++,chnlIndex_card++,r_chnlIndex++)
        {
            gRunParam.m_chnldefs.m_AnalogChnlParam[r_chnlIndex] = malloc(sizeof(ChnlParams));
            memset(gRunParam.m_chnldefs.m_AnalogChnlParam[r_chnlIndex], 0, sizeof(ChnlParams)); //初始化为0
            ChnlParams *channelParam = gRunParam.m_chnldefs.m_AnalogChnlParam[r_chnlIndex];
            channelParam->chnlIndex_device=chnlIndex_device;
            channelParam->chnlIndex_card=chnlIndex_card;
            channelParam->chnl_cardIndex=cardIndex;
            gRunParam.m_chnldefs.m_chnlParam[chnlIndex_device] = channelParam;
        }
    }
    for (int i = 0; i < gRunParam.m_nDigitalCards; i++,cardIndex++)
    {
        chnlIndex_card=0;
        for(int y=0;y<gRunParam.m_DigitalCardParams[i]->nCardChannel;y++,chnlIndex_device++,chnlIndex_card++,w_chnlIndex++)
        {
            gRunParam.m_chnldefs.m_DigitalChnlParam[w_chnlIndex] = malloc(sizeof(ChnlParams));
            memset(gRunParam.m_chnldefs.m_DigitalChnlParam[w_chnlIndex], 0, sizeof(ChnlParams)); //初始化为0
            ChnlParams *channelParam = gRunParam.m_chnldefs.m_DigitalChnlParam[w_chnlIndex];
            channelParam->chnlIndex_device=chnlIndex_device;
            channelParam->chnlIndex_card=chnlIndex_card;
            channelParam->chnl_cardIndex=cardIndex;
            gRunParam.m_chnldefs.m_chnlParam[chnlIndex_device] = channelParam;
        }
    }
    for (int i = 0; i < gRunParam.m_nDigitalCards; i++,cardIndex++)
    {
        chnlIndex_card=0;
        for(int y=0;y<gRunParam.m_DigitalCardParams[i]->nCardChannel;y++,chnlIndex_device++,chnlIndex_card++,t_chnlIndex++)
        {
            gRunParam.m_chnldefs.m_TargetChnlParam[t_chnlIndex] = malloc(sizeof(ChnlParams));
            memset(gRunParam.m_chnldefs.m_TargetChnlParam[t_chnlIndex], 0, sizeof(ChnlParams)); //初始化为0
            ChnlParams *channelParam = gRunParam.m_chnldefs.m_TargetChnlParam[t_chnlIndex];
            channelParam->chnlIndex_device=chnlIndex_device;
            channelParam->chnlIndex_card=chnlIndex_card;
            channelParam->chnl_cardIndex=cardIndex;
            gRunParam.m_chnldefs.m_chnlParam[chnlIndex_device] = channelParam;
        }
    }
    for (int i = 0; i < gRunParam.m_nDigitalCards; i++,cardIndex++)
    {
        chnlIndex_card=0;
        for(int y=0;y<gRunParam.m_DigitalCardParams[i]->nCardChannel;y++,chnlIndex_device++,chnlIndex_card++,error_chnlIndex++)
        {
            gRunParam.m_chnldefs.m_ErrorChnlParam[error_chnlIndex] = malloc(sizeof(ChnlParams));
            memset(gRunParam.m_chnldefs.m_ErrorChnlParam[error_chnlIndex], 0, sizeof(ChnlParams)); //初始化为0
            ChnlParams *channelParam = gRunParam.m_chnldefs.m_ErrorChnlParam[error_chnlIndex];
            channelParam->chnlIndex_device=chnlIndex_device;
            channelParam->chnlIndex_card=chnlIndex_card;
            channelParam->chnl_cardIndex=cardIndex;
            gRunParam.m_chnldefs.m_chnlParam[chnlIndex_device] = channelParam;
        }
    }
    for (int i = 0; i < gRunParam.m_nDigitalCards; i++,cardIndex++)
    {
        chnlIndex_card=0;
        for(int y=0;y<gRunParam.m_DigitalCardParams[i]->nCardChannel;y++,chnlIndex_device++,chnlIndex_card++,other_chnlIndex++)
        {
            gRunParam.m_chnldefs.m_OtherChnlParam[other_chnlIndex] = malloc(sizeof(ChnlParams));
            memset(gRunParam.m_chnldefs.m_OtherChnlParam[other_chnlIndex], 0, sizeof(ChnlParams)); //初始化为0
            ChnlParams *channelParam = gRunParam.m_chnldefs.m_OtherChnlParam[other_chnlIndex];
            channelParam->chnlIndex_device=chnlIndex_device;
            channelParam->chnlIndex_card=chnlIndex_card;
            channelParam->chnl_cardIndex=cardIndex;
            gRunParam.m_chnldefs.m_chnlParam[chnlIndex_device] = channelParam;
        }
    }

    for (int i = 0; i < gRunParam.m_nIOCards; i++,cardIndex++)
    {
        chnlIndex_card=0;
        for(int y=0;y<gRunParam.m_IOCardParams[i]->nCardChannel;y++,chnlIndex_device++,chnlIndex_card++,io_chnlIndex++)
        {
            gRunParam.m_chnldefs.m_IOChnlParam[io_chnlIndex] = malloc(sizeof(ChnlParams));
            memset(gRunParam.m_chnldefs.m_IOChnlParam[io_chnlIndex], 0, sizeof(ChnlParams)); //初始化为0
            ChnlParams *channelParam = gRunParam.m_chnldefs.m_IOChnlParam[io_chnlIndex];
            channelParam->chnlIndex_device=chnlIndex_device;
            channelParam->chnlIndex_card=chnlIndex_card;
            channelParam->chnl_cardIndex=cardIndex;
            gRunParam.m_chnldefs.m_chnlParam[chnlIndex_device] = channelParam;
        }
    }

    for(int y=0;y<virtual_channelCount;y++,chnlIndex_device++,chnlIndex_card++,virtual_chnlIndex++)
    {
        gRunParam.m_chnldefs.m_VirtualChnlParam[virtual_chnlIndex] = malloc(sizeof(ChnlParams));
        memset(gRunParam.m_chnldefs.m_VirtualChnlParam[virtual_chnlIndex], 0, sizeof(ChnlParams)); //初始化为0
        ChnlParams *channelParam = gRunParam.m_chnldefs.m_VirtualChnlParam[virtual_chnlIndex];
        channelParam->chnlIndex_device=chnlIndex_device;
        channelParam->chnlIndex_card=chnlIndex_card;
        channelParam->chnl_cardIndex=cardIndex;
//        channelParam->nSamplingRate=1000;//临时
//        channelParam-> nFrameCount=100;//临时
//        channelParam-> nFrameSize=400;//临时
//        channelParam->bStoreEnable=true;//临时
//        channelParam->bDaqEnable=true;//临时
        sprintf(channelParam->strChnlName,"%s%d","虚拟通道_",y+1);
        gRunParam.m_chnldefs.m_chnlParam[chnlIndex_device] = channelParam;
    }

}
void refreshCardSampleRate()
{
    int r_chnlIndex=0;
    int w_chnlIndex=0;
    int io_chnlIndex=0;
    int virtual_chnlIndex=0;
    for (int i = 0; i < gRunParam.m_nAnalogCards; i++)
    {
        gRunParam.m_AnalogCardParams[i]->nSamplingRate=0;
        for(int y=0;y<gRunParam.m_AnalogCardParams[i]->nCardChannel;y++,r_chnlIndex++)
        {
            ChnlParams *channelParam = gRunParam.m_chnldefs.m_AnalogChnlParam[r_chnlIndex];
            if(channelParam->nSamplingRate>gRunParam.m_AnalogCardParams[i]->nSamplingRate)
                gRunParam.m_AnalogCardParams[i]->nSamplingRate=channelParam->nSamplingRate;
        }
    }
    for (int i = 0; i < gRunParam.m_nDigitalCards; i++)
    {
        gRunParam.m_DigitalCardParams[i]->nSamplingRate=0;
        for(int y=0;y<gRunParam.m_DigitalCardParams[i]->nCardChannel;y++,w_chnlIndex++)
        {
            ChnlParams *channelParam = gRunParam.m_chnldefs.m_DigitalChnlParam[w_chnlIndex];
            if(channelParam->nSamplingRate>gRunParam.m_DigitalCardParams[i]->nSamplingRate)
                gRunParam.m_DigitalCardParams[i]->nSamplingRate=channelParam->nSamplingRate;
        }
    }

    for (int i = 0; i < gRunParam.m_nIOCards; i++)
    {
        gRunParam.m_IOCardParams[i]->nSamplingRate=0;
        for(int y=0;y<gRunParam.m_IOCardParams[i]->nCardChannel;y++,io_chnlIndex++)
        {
            ChnlParams *channelParam = gRunParam.m_chnldefs.m_IOChnlParam[io_chnlIndex];
            if(channelParam->nSamplingRate>gRunParam.m_IOCardParams[i]->nSamplingRate)
                gRunParam.m_IOCardParams[i]->nSamplingRate=channelParam->nSamplingRate;
        }
    }


//    for (int i = 0; i < gRunParam.m_nVirtualCards; i++)
//    {
//        gRunParam.m_VirtualCardParams[i]->nSamplingRate=0;
//        for(int y=0;y<gRunParam.m_VirtualCardParams[i]->nCardChannel;y++,virtual_chnlIndex++)
//        {
//            ChnlParams *channelParam = gRunParam.m_chnldefs.m_VirtualChnlParam[virtual_chnlIndex];
//            if(channelParam->nSamplingRate>gRunParam.m_VirtualCardParams[i]->nSamplingRate)
//                gRunParam.m_VirtualCardParams[i]->nSamplingRate=channelParam->nSamplingRate;
//        }
//    }
}
extern int LOOP_TIME_NEED;
int UpdateHardware()
{
    struct pxi_card_t *ptrig;
    //注意通道顺序！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
    struct pxi_card_t *card;
    for(int i = 0; i < gRunParam.m_nAnalogCards; i++)
    {
        int64_t rate = gRunParam.m_AnalogCardParams[i]->nSamplingRate;
        if(rate < 2500)
        {
            freqfir_select_1024(g_pxi_system.r_cards[i].user_base, 0);
            printf("choose 0 fir rate ##########\n");
        }
        else
        {
            freqfir_select_1024(g_pxi_system.r_cards[i].user_base, 1);
            printf("choose 1 fir rate ##########\n");
        }
        if(rate < 2500)
        {
            adcODR_x16_status(g_pxi_system.r_cards[i].user_base, 2);
        }
        else if(rate < 12800)
        {
            adcODR_x16_status(g_pxi_system.r_cards[i].user_base, 1);
        }
        else
        {
            adcODR_x16_status(g_pxi_system.r_cards[i].user_base, 0);
        }
    }


    //DA初始化
    int tt[8]={2,2,2,2,2,2,2,2};
    for(int i=0;i<g_pxi_system.n_wCards;i++)
    {
        struct pxi_card_t *card = &g_pxi_system.w_cards[i];
        printf("%d %d\n",card->card_type,card->phy_slot);
        printf("采样率：%d\n",gRunParam.m_DigitalCardParams[i]->nSamplingRate);
        card->init(card,gRunParam.m_DigitalCardParams[i]->nSamplingRate,tt);
        //        params[i].card = card;
        //        params[i].channelCount =  8;
        //        params[i].cardIndex = i+cardIndex;
        //        params[i].samplingRate = sampleRate;
        //        //注意通道顺序
        //        for(int temp=0;temp<8;temp++)
        //        {
        //            params[i+cardIndex].senserType[temp] = 2;
        //        }
        //
        //        // 启动线程执行init
        //        pthread_create(&threads[i+cardIndex], NULL, init_card_thread, (void*)&params[i+cardIndex]);
        //        printf("start thread write init %d\n",i);
    }


    for(int i=0; i<g_pxi_system.n_wCards; i++)
    {
        ptrig = &g_pxi_system.w_cards[i];
        ptrig->trig(ptrig);
    }
    for(int i=0; i<g_pxi_system.n_rCards; i++)
    {
        ptrig = &g_pxi_system.r_cards[i];
        ptrig->trig_o(ptrig);
    }



    //        int64_t rate = gRunParam.m_chnldefs.m_chnlParam[0]->nSamplingRate;
    //        int64_t rate_in=0;
    //        if(rate < 2500)
    //        {
    //            rate_in = rate * 32;
    //        }
    //        else
    //        {
    //            rate_in = rate;
    //        }
    //        int64_t freq_coef_high,freq_coef_low,freq_in;
    //        freq_in = round(200000000.0/rate_in);
    //        if(freq_in % 2 != 0)
    //        {
    //            freq_coef_high = freq_in/2 + 1;
    //            freq_coef_low = freq_in/2;
    //        }else{
    //            freq_coef_high = freq_in/2;
    //            freq_coef_low = freq_in/2;
    //        }
    //
    //        printf("freq_coef_high %lld, freq_coef_low %lld\n", freq_coef_high, freq_coef_low);
    //
    //        ptrig = &g_pxi_system.r_cards[0];
    //        write_freq_high(ptrig->user_base, freq_coef_high);   //p->cards[0].h    p->p0->h
    //        write_freq_low(ptrig->user_base, freq_coef_low);     //
    //        ptrig->trig(ptrig);

    //单独换线程
    pthread_t threads[MAX_CARDS];
    thread_param_t params[MAX_CARDS];
    int channelCount=0;

    int cardIndex=0;
    for(int i=0;i<g_pxi_system.n_rCards;i++)
    {
        struct pxi_card_t *card = &g_pxi_system.r_cards[i];
        params[i].card = card;
        params[i].cardIndex = i;
        params[i].samplingRate = gRunParam.m_AnalogCardParams[i]->nSamplingRate;

        printf("采样率：%d\n",gRunParam.m_AnalogCardParams[i]->nSamplingRate);
        //注意通道顺序


        params[i].channelCount =  gRunParam.m_AnalogCardParams[i]->nCardChannel;
        for (int y = 0; y < gRunParam.m_AnalogCardParams[i]->nCardChannel; y++)
        {
            int type=0;
            if (strstr(gRunParam.m_chnldefs.m_AnalogChnlParam[channelCount]->strSenserType, "ICP") != NULL)
            {
                type=1;
            }
            else if(strstr(gRunParam.m_chnldefs.m_AnalogChnlParam[channelCount]->strSenserType, "AC单端") != NULL)//AC单端
            { type=2;
            }
            else if(strstr(gRunParam.m_chnldefs.m_AnalogChnlParam[channelCount]->strSenserType, "AC差分") != NULL)//AC插分
            { type=3;
            }
            else if(strstr(gRunParam.m_chnldefs.m_AnalogChnlParam[channelCount]->strSenserType, "DC单端") != NULL)//DC单端
            { type=4;
            }
            else if(strstr(gRunParam.m_chnldefs.m_AnalogChnlParam[channelCount]->strSenserType, "DC差分") != NULL)//DC插分
            { type=5;
            }
            else
            {
                type=2;
            }

            params[i].senserType[y] = type;
            channelCount++;
        }
        cardIndex++;
        // 启动线程执行init
        pthread_create(&threads[i], NULL, init_card_thread, (void*)&params[i]);
        printf("start thread read init %d\n",i);
    }


    // 等待所有线程返回
    for (int i = 0; i < g_pxi_system.n_rCards; i++)
    {
        pthread_join(threads[i], NULL);

        if (params[i].ret < 0)
        {
            printf("card %d init samplingrate failed\n", i);
            return -1;
        }
        printf("card %d init success\n", i);
    }




    channelCount=0;
    //注意板卡顺序
    for (int i = 0; i < gRunParam.m_nAnalogCards; i++)
    {
        card = &g_pxi_system.r_cards[i];
        int allPointCount=0;
        for (int y = 0; y < gRunParam.m_AnalogCardParams[i]->nCardChannel; y++)
        {
            //JW 注意要的是点数还是字节数
            allPointCount += gRunParam.m_chnldefs.m_AnalogChnlParam[channelCount]->nFrameCount;
            channelCount++;
        }
        int allSize = allPointCount*sizeof(float);
        printf("each card size :%d \n",allSize);
        struct aiocb *r = malloc(sizeof(struct aiocb));
        if (!r)
        {
            printf("malloc aiocb failed\n");
            return -1;
        }
        bzero(r, sizeof(struct aiocb));
        if (posix_memalign((void**)&(r->aio_buf), 4096, allSize + 4096) != 0)
        {
            printf("posix_memalign failed\n");
            return -1;
        }
        if (posix_memalign(&readData, 4096, allSize + 4096) != 0)
        {
            printf("posix_memalign failed\n");
            return -1;
        }
        r->aio_fildes = card->h_dma;
        r->aio_nbytes = allSize;        //BUFFER_SIZE
        r->aio_offset = 0;
        r->aio_lio_opcode = LIO_READ;
        aiocb_list[i] = r;
    }
    channelCount=0;
    double code_pre_uVoltage =  ((double)0xFFFFFFFF + 1) / (10.0 * 2) / 1000;


    for (int i=0; i<gRunParam.m_nDigitalCards; ++i)
    {
        int allPointCount=0;
        for (int y = 0; y < gRunParam.m_DigitalCardParams[i]->nCardChannel; y++)
        {
            //JW 注意要的是点数还是字节数
            allPointCount += gRunParam.m_chnldefs.m_DigitalChnlParam[channelCount]->nFrameCount;
            channelCount++;
        }
        int allSize = allPointCount*sizeof(float);
        printf("%d\n",allSize);
        if (posix_memalign(&writeData, 4096, allSize + 4096) != 0)
        {
            printf("posix_memalign failed\n");
            return -1;
        }
        printf("hardwareInitI write posix_memalign: %d\n",i);
    }

    for (int i = 0; i < g_pxi_system.n_rCards; i++)
    {
        card = &g_pxi_system.r_cards[i];
        card->syncin(card);
    }
    printf("syncin success\n");

    for (int i = 0; i < g_pxi_system.n_wCards; i++)
    {
        card = &g_pxi_system.w_cards[i];
        card->sync(card);
    }
    for (int i=0; i<gRunParam.m_nDigitalCards; ++i)
    {
        struct pxi_card_t *cardi = &(g_pxi_system.w_cards[i]);
        int nnn = cardi->write( cardi, writeData, 4*gRunParam.m_DigitalCardParams[i]->nSamplingRate*gRunParam.m_DigitalCardParams[i]->nCardChannel/1000*LOOP_TIME_NEED);
        nnn = cardi->write( cardi,writeData, 4*gRunParam.m_DigitalCardParams[i]->nSamplingRate*gRunParam.m_DigitalCardParams[i]->nCardChannel/1000*LOOP_TIME_NEED);
    }

    printf("update success\n");

    return 0;
}
int startDaqDMA()
{
    struct pxi_card_t *card;
    for (int i = 0; i < gRunParam.m_nAnalogCards; i++)
    {
        card = &g_pxi_system.r_cards[i];
        int h_dma;
        char dma_name[256];
        sprintf(dma_name, "/dev/xdma%d_c2h_0", card->card_num);
        h_dma = open(dma_name, O_RDWR);
        if (h_dma < 0)
            return -1;
        card->h_dma = h_dma;
        aiocb_list[i]->aio_fildes = h_dma;
    }
    printf("start dma success!\n");
}

int getHardwareOneSapmle()
{
    //    int64_t begin=GetTimeStampU();
    //    int ret = lio_listio(LIO_WAIT, aiocb_list, g_pxi_system.n_rCards, NULL);
    //    if(ret <0)
    //    {
    //        printf("sample lio_listio data fail\n");
    //        return -1;
    //    }
    //    int64_t end = GetTimeStampU() - begin;
    //    printf(" getHardwareOneSapmle speed  : %lld us \n",  end);


}
int hardwareSapmle(void **data,int cardIndex,bool *isOk)
{
    //    int64_t begin=GetTimeStampU();
    struct aiocb * r = aiocb_list[cardIndex];
    int rx_size = aio_return(r);
    int size = r->aio_nbytes;
    if (rx_size != size)
    {
        printf("card in slot, data stream underflow. card num: %d %d %d\n",cardIndex,rx_size,size);
        *isOk=false;
    }
    *isOk=true;
    *data= r->aio_buf;

    //    int64_t end = GetTimeStampU() - begin;
    //    printf(" rt speed  : %lld us \n",  end);
    return rx_size;
}
double maxTemprature = 40.0;
static char logdata[LOG_DATA_SIZE];
bool getDevicePowerState()
{
    int fd;
    char buffer[1024];
    ssize_t num_read;

    // 打开文件
    fd = open("/sys/class/thermal/thermal_zone0/temp", O_RDONLY);
    if (fd == -1)
    {
        perror("open");
        return false;
    }

    // 读取文件内容
    num_read = read(fd, buffer, 1024);
    if (num_read == -1)
    {
        perror("read");
        close(fd);
        return false;
    }

    // 添加字符串结束符
    buffer[num_read] = '\0';
    int temp = atoi(buffer);
    double rtemp = temp /1000;
    if (rtemp > maxTemprature)
    {
        // 打印文件内容
        sprintf(logdata, "当前最大温度值: %.2f°\n", rtemp);
        printf(logdata);
        writelog(logdata);
        maxTemprature = rtemp;
    }
    // 关闭文件
    close(fd);

    bool isHighTem=false;
    if(maxTemprature>90)
        isHighTem=true;
    return isHighTem;
}
void syncCMD()
{
    system("sync");
}
// 递归删除文件夹及其所有内容
void delete_directory(const char *path,bool deleteSelf)
{
    DIR *dir;
    struct dirent *entry;
    char full_path[1024];
    struct stat statbuf;

    if ((dir = opendir(path)) == NULL)
    {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (lstat(full_path, &statbuf) == -1)
        {
            perror("lstat");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode))
        {
            // 递归删除子目录
            delete_directory(full_path,true);
        }
        else
        {
            // 删除文件
            if (unlink(full_path) == -1)
            {
                perror("unlink");
            }
        }
    }
    closedir(dir);
    if(deleteSelf)
    {
        if (rmdir(path) == -1)
        {
            perror("rmdir");
        }
    }
}




