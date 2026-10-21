/*
 * pxie_7021.c
 *
 *  Created on: 2022年4月15日
 *      Author: biseadmin
 */
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
#include <dirent.h>
#include <errno.h>

#include "../xilinx/gpio/xgpio.h"
#include "../board/clk/cdcm6208xx.h"
#include "../board/adc/ad7768.h"
#include "../board/io/pca9555.h"
#include "../board/clk/si53xx.h"
#include "../card/cards.h"

#define PCA_I2C_ADDR_U26     (0x21)
#define PCA_I2C_ADDR_U27     (0x23)
#define PCA_I2C_ADDR_U28     (0x20)

#define PCA_I2C_ADDR_U5     (0x21)
#define PCA_I2C_ADDR_U11     (0x23)
#define PCA_I2C_ADDR_U16     (0x20)

void chann_close(void* dev, int num)
{
    uint8_t pinset, pinnum;

    switch (num) {
    case 1:
        pinset = 0; pinnum = 3;
        break;
    case 2:
        pinset = 1; pinnum = 3;
        break;
    case 3:
        pinset = 0; pinnum = 6;
        break;
    case 4:
        pinset = 1; pinnum = 0;
        break;
    case 5:
        pinset = 0; pinnum = 5;
        break;
    case 6:
        pinset = 1; pinnum = 4;
        break;
    case 7:
        pinset = 1; pinnum = 6;
        break;
    case 8:
        pinset = 0; pinnum = 2;
        break;
    default :
        return;
    }

    pca9555_pin_set(dev, PCA_I2C_ADDR_U5, pinset, pinnum);
    usleep(10000);
    pca9555_pin_clr(dev, PCA_I2C_ADDR_U5, pinset, pinnum);
    usleep(10000);
}

void chann_open(void* dev, int num)
{
    uint8_t pinset, pinnum;

    switch (num) {
    case 1:
        pinset = 0; pinnum = 1;
        break;
    case 2:
        pinset = 1; pinnum = 2;
        break;
    case 3:
        pinset = 1; pinnum = 1;
        break;
    case 4:
        pinset = 0; pinnum = 7;
        break;
    case 5:
        pinset = 0; pinnum = 4;
        break;
    case 6:
        pinset = 1; pinnum = 5;
        break;
    case 7:
        pinset = 1; pinnum = 7;
        break;
    case 8:
        pinset = 0; pinnum = 0;
        break;
    default :
        return;
    }

    pca9555_pin_set(dev, PCA_I2C_ADDR_U5, pinset, pinnum);
    usleep(20000);
    pca9555_pin_clr(dev, PCA_I2C_ADDR_U5, pinset, pinnum);
    usleep(20000);
}

void icp_set(void* dev, int num, int excit_src, int iftermCfg)
{
    uint8_t ps1, pn1, ps2, pn2, cid1, cid2;
    int setValue;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 0; pn1 = 1;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 1; pn2 = 6;
        setValue = iftermCfg;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 0; pn1 = 3;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 0; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 1; pn1 = 3;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 0; pn2 = 5;
        setValue = iftermCfg;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 1; pn1 = 1;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR_U16;
        ps1 = 0; pn1 = 1;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 0; pn2 = 0;
        setValue = iftermCfg;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR_U16;
        ps1 = 0; pn1 = 3;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 0; pn2 = 4;
        setValue = iftermCfg;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR_U16;
        ps1 = 0; pn1 = 7;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 1; pn2 = 5;
        setValue = iftermCfg;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR_U16;
        ps1 = 1; pn1 = 0;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    default :
        return;
    }

    pca9555_iepe_pin_set(dev, cid1, ps1, pn1, setValue);
    pca9555_iepe_pin_set(dev, cid2, ps2, pn2, setValue);
}
void RSE_set(void* dev, int num, int iftermCfg)
{
    uint8_t ps2, pn2, cid2;
    int setValue;

    switch (num) {
    case 1:
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 1; pn2 = 6;
        setValue = iftermCfg;
        break;
    case 2:
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 0; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 3:
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 0; pn2 = 5;
        setValue = iftermCfg;
        break;
    case 4:
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 5:
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 0; pn2 = 0;
        setValue = iftermCfg;
        break;
    case 6:
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 0; pn2 = 4;
        setValue = iftermCfg;
        break;
    case 7:
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 1; pn2 = 5;
        setValue = iftermCfg;
        break;
    case 8:
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    default :
        return;
    }

    pca9555_iepe_pin_set(dev, cid2, ps2, pn2, setValue);
}

void coupl_selec(void* dev, int num, int coupl_way)
{
    uint8_t ps1, pn1, ps2, pn2, cid1, cid2;
    int setValue;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 1; pn1 = 7;
        setValue = coupl_way;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 0; pn2 = 0;
        setValue = coupl_way;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 1; pn1 = 5;
        setValue = coupl_way;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 1; pn2 = 4;
        setValue = coupl_way;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 0; pn1 = 4;
        setValue = coupl_way;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 0; pn2 = 6;
        setValue = coupl_way;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 0; pn1 = 7;
        setValue = coupl_way;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 1; pn2 = 0;
        setValue = coupl_way;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR_U16;
        ps1 = 1; pn1 = 7;
        setValue = coupl_way;
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 0; pn2 = 2;
        setValue = coupl_way;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR_U16;
        ps1 = 1; pn1 = 6;
        setValue = coupl_way;
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 0; pn2 = 5;
        setValue = coupl_way;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 0; pn1 = 6;
        setValue = coupl_way;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 1; pn2 = 4;
        setValue = coupl_way;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR_U16;
        ps1 = 1; pn1 = 3;
        setValue = coupl_way;
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 1; pn2 = 1;
        setValue = coupl_way;
        break;
    default :
        return;
    }

    pca9555_iepe_pin_set(dev, cid1, ps1, pn1, setValue);
    pca9555_iepe_pin_set(dev, cid2, ps2, pn2, setValue);
}
void RSE_close(void* dev, int num)
{
    uint8_t ps2, pn2, cid2;

    switch (num) {
    case 1:
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 1; pn2 = 6;
        break;
    case 2:
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 1; pn2 = 2;
        break;
    case 3:
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 1; pn2 = 5;
        break;
    case 4:
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 0; pn2 = 4;
        break;
    case 5:
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 0; pn2 = 0;
        break;
    case 6:
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 1; pn2 = 2;
        break;
    case 7:
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 0; pn2 = 5;
        break;
    case 8:
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 0; pn2 = 2;
        break;
    default :
        return;
    }

    pca9555_pin_clr(dev, cid2, ps2, pn2);
    //pca9555_iepe_pin_set(dev, cid2, ps2, pn2, setValue);
}
void icp_close(void* dev, int num)
{
    uint8_t ps1, pn1, ps2, pn2, cid1, cid2;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 0; pn1 = 1;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 1; pn2 = 6;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR_U16;
        ps1 = 1; pn1 = 0;
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 1; pn2 = 2;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR_U16;
        ps1 = 0; pn1 = 7;
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 1; pn2 = 5;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR_U16;
        ps1 = 0; pn1 = 3;
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 0; pn2 = 4;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR_U16;
        ps1 = 0; pn1 = 1;
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 0; pn2 = 0;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 1; pn1 = 1;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 1; pn2 = 2;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 1; pn1 = 3;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 0; pn2 = 5;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 0; pn1 = 3;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 0; pn2 = 2;
        break;
    default :
        return;
    }

    pca9555_pin_clr(dev, cid1, ps1, pn1);
    pca9555_pin_clr(dev, cid2, ps2, pn2);
}
void coupl_close(void* dev, int num)
{
    uint8_t ps1, pn1, ps2, pn2, cid1, cid2;
    int setValue;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 1; pn1 = 7;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 0; pn2 = 0;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR_U16;
        ps1 = 1; pn1 = 3;
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 1; pn2 = 1;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 0; pn1 = 6;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 1; pn2 = 4;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR_U16;
        ps1 = 1; pn1 = 6;
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 0; pn2 = 5;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR_U16;
        ps1 = 1; pn1 = 7;
        cid2 = PCA_I2C_ADDR_U16;
        ps2 = 0; pn2 = 2;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 0; pn1 = 7;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 1; pn2 = 0;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 0; pn1 = 4;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 0; pn2 = 6;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR_U11;
        ps1 = 1; pn1 = 5;
        cid2 = PCA_I2C_ADDR_U11;
        ps2 = 1; pn2 = 4;
        break;
    default :
        return;
    }

    pca9555_pin_clr(dev, cid1, ps1, pn1);
    pca9555_pin_clr(dev, cid2, ps2, pn2);
    //pca9555_iepe_pin_set(dev, cid1, ps1, pn1, setValue);
    //pca9555_iepe_pin_set(dev, cid2, ps2, pn2, setValue);
}

void iepe_open(void *usr_base, int num)
{
    uint8_t ps1, pn1, ps2, pn2, cid1, cid2;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR_U27;
        ps1 = 0; pn1 = 2;
        cid2 = PCA_I2C_ADDR_U27;
        ps2 = 0; pn2 = 0;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR_U27;
        ps1 = 0; pn1 = 6;
        cid2 = PCA_I2C_ADDR_U27;
        ps2 = 0; pn2 = 4;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR_U27;
        ps1 = 1; pn1 = 5;
        cid2 = PCA_I2C_ADDR_U27;
        ps2 = 1; pn2 = 7;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR_U27;
        ps1 = 1; pn1 = 1;
        cid2 = PCA_I2C_ADDR_U27;
        ps2 = 1; pn2 = 4;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR_U28;
        ps1 = 0; pn1 = 2;
        cid2 = PCA_I2C_ADDR_U28;
        ps2 = 0; pn2 = 0;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR_U28;
        ps1 = 0; pn1 = 6;
        cid2 = PCA_I2C_ADDR_U28;
        ps2 = 0; pn2 = 4;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR_U28;
        ps1 = 1; pn1 = 5;
        cid2 = PCA_I2C_ADDR_U28;
        ps2 = 1; pn2 = 7;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR_U28;
        ps1 = 1; pn1 = 1;
        cid2 = PCA_I2C_ADDR_U28;
        ps2 = 1; pn2 = 3;
        break;
    default :
        return;
    }

    pca9555_pin_set(usr_base, cid1, ps1, pn1);
    pca9555_pin_set(usr_base, cid2, ps2, pn2);
}

void iepe_close(void *usr_base, int num)
{
    uint8_t ps1, pn1, ps2, pn2, cid1, cid2;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR_U27;
        ps1 = 0; pn1 = 2;
        cid2 = PCA_I2C_ADDR_U27;
        ps2 = 0; pn2 = 0;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR_U27;
        ps1 = 0; pn1 = 6;
        cid2 = PCA_I2C_ADDR_U27;
        ps2 = 0; pn2 = 4;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR_U27;
        ps1 = 1; pn1 = 5;
        cid2 = PCA_I2C_ADDR_U27;
        ps2 = 1; pn2 = 7;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR_U27;
        ps1 = 1; pn1 = 1;
        cid2 = PCA_I2C_ADDR_U27;
        ps2 = 1; pn2 = 4;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR_U28;
        ps1 = 0; pn1 = 2;
        cid2 = PCA_I2C_ADDR_U28;
        ps2 = 0; pn2 = 0;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR_U28;
        ps1 = 0; pn1 = 6;
        cid2 = PCA_I2C_ADDR_U28;
        ps2 = 0; pn2 = 4;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR_U28;
        ps1 = 1; pn1 = 5;
        cid2 = PCA_I2C_ADDR_U28;
        ps2 = 1; pn2 = 7;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR_U28;
        ps1 = 1; pn1 = 1;
        cid2 = PCA_I2C_ADDR_U28;
        ps2 = 1; pn2 = 3;
        break;
    default :
        return;
    }

    pca9555_pin_clr(usr_base, cid1, ps1, pn1);
    pca9555_pin_clr(usr_base, cid2, ps2, pn2);
}

int pxie_7021_init_OO(struct pxi_card_t *p, int sampleRate)
{

    int h_dma;
    char dma_name[256];
    void *usr_base;

    if (p == NULL)
        return -1;

    printf("init PXIe-7021 slot %d\n" , p->phy_slot);

    sprintf(dma_name, "/dev/xdma%d_c2h_0", p->card_num);

    h_dma = open(dma_name, O_RDWR);
    if (h_dma < 0)
        return -1;

    p->h_dma = h_dma;

    usr_base = p->user_base;


    pca9555_init(usr_base, PCA_I2C_ADDR_U26); // U26
    pca9555_init(usr_base, PCA_I2C_ADDR_U27); // U27
    pca9555_init(usr_base, PCA_I2C_ADDR_U28); // U28

    for (int i = 0; i < 8; i++) {
        chann_open(usr_base, i + 1);

//        iepe_open(usr_base, i + 1);
    }

    cdcm6208_enable(usr_base);
    usleep(20000);
    int ret = cdcm6208_init(usr_base, sampleRate);
    if(ret < 0)
        return -1;


    ad7768_enable(usr_base);
    usleep(40000);
//    ad7768_init(usr_base);

    return 0;
}

int pxie_7021_init(struct pxi_card_t *p, int samplingRate, int *cfg)
{
    if (p == NULL)
        return -1;

    printf("init PXI-7021 slot %d\n" , p->phy_slot);

    int h_dma;
    char dma_name[256];
    uint32_t fout;
    double samplingRate_in;
    void *usr_base;
    uint32_t  decRate;

    sprintf(dma_name, "/dev/xdma%d_c2h_0", p->card_num);

    h_dma = open(dma_name, O_RDWR);
    if (h_dma < 0)
        return -1;

    p->h_dma = h_dma;

    usr_base = p->user_base;

    pca9555_init(usr_base, PCA_I2C_ADDR_U5); // U26
    pca9555_init(usr_base, PCA_I2C_ADDR_U11); // U27
    pca9555_init(usr_base, PCA_I2C_ADDR_U16); // U28

//    printf("excit_src %d, iftermCfg %d, coupl_way %d\n", excit_src, termCfg, coupl_way);
    for (int i = 0; i < 8; i++) {
        chann_open(usr_base, i + 1);
//        RSE_set(usr_base, i + 1, 1);
        if(cfg[i] == 1)//icp
        {
            icp_set(usr_base, i + 1, 1, 1);
        }
        else
        {
            if((cfg[i]-2) % 2 == 0)//单端
                RSE_set(usr_base, i + 1, 1);
            if((cfg[i]-2) / 2)     //DC
                coupl_selec(usr_base, i + 1, 1);
        }

//        if(excit_src == 1){
//            if(termCfg != 1){
//                printf("---------------------> ERROR TERMCFG SET!!! <------------------------\n");
//                break;
//            }else{
//                icp_set(user_h, i + 1, 1, 1);
//            }
//            if(coupl_way == 1){
//                printf("---------------------> ERROR RSE SET!!! <------------------------\n");
//                break;
//            }
//        }
//        if(termCfg == 1 && excit_src != 1){
//            RSE_set(usr_base, i + 1, 1);
//        }
//        if(coupl_way == 1){
//            coupl_selec(usr_base, i + 1, 1);
//        }
    }
    if(samplingRate < 6400){ //0){//
        samplingRate_in = samplingRate * 32;
        freqfir_select(usr_base, 0);
    }else{
        samplingRate_in = samplingRate;
        freqfir_select(usr_base, 1);
    }
    printf("input samplingRate_in %lf\n", samplingRate_in);


    if(samplingRate_in > 102400){
        decRate = 32;
        fout = samplingRate_in *32 *4;
    }else if(samplingRate_in <= 102400 && samplingRate_in > 51200){
        decRate = 64;
        fout = samplingRate_in *64 *4;
    }else if(samplingRate_in <= 51200 && samplingRate_in > 25600){
        decRate = 128;
        fout = samplingRate_in *128 *4;
    }else if(samplingRate_in <= 25600 && samplingRate_in > 12800){
        decRate = 256;
        fout = samplingRate_in *256 *4;
    }else if(samplingRate_in <= 12800 && samplingRate_in > 6400){
        decRate = 512;
        fout = samplingRate_in *512 *4;
    }else{
        decRate = 1024;
        fout = samplingRate_in *1024 *4;
    }
    int ret;
    ret = si53xx_initR(usr_base, fout, 1);
    //printf("after fout %d\n", fout);
    uint32_t decim = 100;
    while(ret < 0){
        fout = (fout / decim) * decim;
        ret = si53xx_initR(usr_base, fout, 1);
        decim = decim * 10;
    }

//    if(ret < 0)
//        return -1;
//    si53xx_init(usr_base);

    ad7768_enable(usr_base);
    usleep(40000);
    ad7768_init(usr_base, decRate);
//
//    ad7768_reg_write(usr_base, 0x01, 0x00); //dec x32, filter WB
//    ad7768_reg_write(usr_base, 0x04, 0x33);
//    ad7768_reg_write(usr_base, 0x07, 0x03);

    return 0;
}

int pxie_7021_init_o(struct pxi_card_t *p, int sampleRate)
{

    int h_dma;
    char dma_name[256];
    void *usr_base;

    if (p == NULL)
        return -1;

    printf("init PXIe-7021 slot %d\n" , p->phy_slot);

    sprintf(dma_name, "/dev/xdma%d_c2h_0", p->card_num);

    h_dma = open(dma_name, O_RDWR);
    if (h_dma < 0)
        return -1;

    p->h_dma = h_dma;

    usr_base = p->user_base;


    pca9555_init(usr_base, PCA_I2C_ADDR_U26); // U26
    pca9555_init(usr_base, PCA_I2C_ADDR_U27); // U27
    pca9555_init(usr_base, PCA_I2C_ADDR_U28); // U28

    for (int i = 0; i < 8; i++) {
        chann_open(usr_base, i + 1);

//        iepe_open(usr_base, i + 1);
    }

    cdcm6208_enable(usr_base);
    usleep(20000);
    int ret = cdcm6208_init(usr_base, sampleRate);
    if(ret < 0)
        return -1;


    ad7768_enable(usr_base);
    usleep(40000);
//    ad7768_init(usr_base);

    return 0;
}

void pxie_7021_deinit(struct pxi_card_t *p)
{
    void *usr_base;

    if (p == NULL)
        return;

    printf("deinit PXIe-7021\n");

    if (p->h_dma >= 0)
        close(p->h_dma);

    usr_base = p->user_base;

    ad7768_disable(usr_base);
    si53xx_term(usr_base);
//    cdcm6208_disable(usr_base);

    for (int i = 0; i < 8 ; i++) {
        chann_close(usr_base, i+1);
        icp_close(usr_base, i+1);
        coupl_close(usr_base, i+1);
        RSE_close(usr_base, i+1);
//        iepe_close(usr_base, i + 1);
    }
}

int pxie_7021_read(struct pxi_card_t *p, void *buf, size_t size)
{
    if (p == NULL)
        return -1;
    printf("pxie_7021_read  size:%lu\n", size);
    return read(p->h_dma, buf, size);
}


int pxie_7021_write(struct pxi_card_t *p, void *buf, size_t size)
{
    if (p == NULL)
        return -1;
    return 0;
}

void pxie_7021_trig(struct pxi_card_t *p)
{
    if (p == NULL)
        return;

//    ad7768_start_signal(p->user_base);
    printf("\n\n--> TRIGGER 7021.\n\n\n");
    tri_io_set_input(p->user_base);
}

int pxi_7021_init_o(struct pxi_card_t *p, int sampleRate)
{
    if (p == NULL)
        return -1;

    printf("init PXI-7021 slot %d\n" , p->phy_slot);

    int h_dma;
    char dma_name[256];

    sprintf(dma_name, "/dev/xdma%d_c2h_0", p->card_num);

    h_dma = open(dma_name, O_RDWR);
    if (h_dma < 0)
        return -1;

    p->h_dma = h_dma;

    void *usr_base = p->user_base;

    pca9555_init(usr_base, PCA_I2C_ADDR_U26); // U26
    pca9555_init(usr_base, PCA_I2C_ADDR_U27); // U27
    pca9555_init(usr_base, PCA_I2C_ADDR_U28); // U28
//    sleep(1);
//
    for (int i = 0; i < 8; i++) {
        chann_open(usr_base, i + 1);

        iepe_open(usr_base, i + 1);
    }

    cdcm6208_enable(usr_base);
    usleep(20000);
    int ret = cdcm6208_init(usr_base, sampleRate);
    if(ret < 0)
        return -1;
//    si53xx_init(usr_base);

    ad7768_enable(usr_base);
    usleep(40000);
//    ad7768_init(usr_base);
//
//    ad7768_reg_write(usr_base, 0x01, 0x00); //dec x32, filter WB
//    ad7768_reg_write(usr_base, 0x04, 0x33);
//    ad7768_reg_write(usr_base, 0x07, 0x03);

    return 0;
}

void pxi_7021_deinit(struct pxi_card_t *p)
{
    if (p == NULL)
        return;

    printf("deinit PXI-7021\n");

    void *usr_base = p->user_base;

    ad7768_disable(usr_base);

    for (int i = 0; i < 8 ; i++) {
        chann_close(usr_base, i+1);

        iepe_close(usr_base, i + 1);
    }

    cdcm6208_disable(usr_base);

    if (p->h_dma >= 0)
        close(p->h_dma);

}

int pxi_7021_read(struct pxi_card_t *p, void *buf, size_t size)
{
    if (p == NULL)
        return -1;

    return read(p->h_dma, buf, size);
}

void pxi_7021_trig(struct pxi_card_t *p)
{
    if (p == NULL)
        return;

//    ad7768_start_signal(p->user_base);
    tri_io_set_input(p->user_base);
}
