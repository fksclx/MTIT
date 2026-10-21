/*
 * pxie_7022.c
 *
 *  Created on: 2022��8��15��
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
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#include "../xilinx/gpio/xgpio.h"
#include "../board/clk/si53xx.h"
#include "../board/dac/pcm1794a.h"
#include "../board/io/pca9555.h"

#include "../card/cards.h"

#define PCA_I2C_ADDR_U11     (0x23)
#define PCA_I2C_ADDR_U15     (0x21)

void pca_close(void *dev, int num)
{
    uint8_t pinset, pinnum;

    switch (num) {
    case 1:
        pinset = 1; pinnum = 5;
        break;
    case 2:
        pinset = 0; pinnum = 4;
        break;
    case 3:
        pinset = 0; pinnum = 3;
        break;
    case 4:
        pinset = 0; pinnum = 2;
        break;
    case 5:
        pinset = 1; pinnum = 7;
        break;
    case 6:
        pinset = 1; pinnum = 6;
        break;
    case 7:
        pinset = 0; pinnum = 1;
        break;
    case 8:
        pinset = 0; pinnum = 0;
        break;
    default :
        return;
    }

    pca9555_pin_set(dev, PCA_I2C_ADDR_U15, pinset, pinnum);
//    pca9555_pin_set(dev, PCA_I2C_ADDR_U15, pinset, pinnum);
//    usleep(20000);
//    pca9555_pin_clr(dev, PCA_I2C_ADDR_U15, pinset, pinnum);
//    usleep(20000);
    //pca9555_iepe_pin_set(dev, PCA_I2C_ADDR_U15, pinset, pinnum, 1);

}
void pca_clr(void *dev, int num)
{
    uint8_t pinset, pinnum;

    switch (num) {
    case 1:
        pinset = 1; pinnum = 5;
        break;
    case 2:
        pinset = 0; pinnum = 4;
        break;
    case 3:
        pinset = 0; pinnum = 3;
        break;
    case 4:
        pinset = 0; pinnum = 2;
        break;
    case 5:
        pinset = 1; pinnum = 7;
        break;
    case 6:
        pinset = 1; pinnum = 9;
        break;
    case 7:
        pinset = 0; pinnum = 1;
        break;
    case 8:
        pinset = 0; pinnum = 0;
        break;
    default :
        return;
    }


    //pca9555_pin_clr(dev, PCA_I2C_ADDR_U15, pinset, pinnum);
    pca9555_iepe_pin_set(dev, PCA_I2C_ADDR_U15, pinset, pinnum, 0);

}
int pxie_7022_init(struct pxi_card_t *p, int sampleRate, int* cfg)
{
    int h_dma;
    char dma_name[256];
    void *usr_base;
    uint64_t samplingRate_in;

    if (p == NULL)
        return -1;

    printf("init PXIe-7022 slot %d\n" , p->phy_slot);

    sprintf(dma_name, "/dev/xdma%d_h2c_0", p->card_num);

    h_dma = open(dma_name, O_RDWR);
    if (h_dma < 0)
        return -1;

    p->h_dma = h_dma;

    usr_base = p->user_base;

    pca9555_init(usr_base, PCA_I2C_ADDR_U15);

    for (int i = 0; i < 8; i++) {
        pca_close(usr_base, i + 1);
    }
    if(sampleRate >= 12800)
    {
        samplingRate_in = sampleRate*128;
        fir_set_input_da(usr_base, 1);
    }
    else
    {
        samplingRate_in = sampleRate*32*128;
        fir_set_input_da(usr_base, 0);
    }
    int ret = si53xx_initR(usr_base, samplingRate_in, 0);
    printf("si53xx_initR:  %d\t%d\t%d\n", ret, sampleRate, samplingRate_in);
    if (ret < 0)
        return -1;

    pcm1794a_enable(usr_base);

    return 0;
}

void pxie_7022_deinit(struct pxi_card_t *p)
{
    void *usr_base;

    if (p == NULL)
        return;

    printf("deinit PXIe-7022\n");

    if (p->h_dma >= 0)
        close(p->h_dma);

    usr_base = p->user_base;

    si53xx_term(usr_base);

    pcm1794a_disable(usr_base);
}

int pxie_7022_read(struct pxi_card_t *p, void *buf, size_t size)
{
    if (p == NULL)
        return -1;
    return 0;
}

int pxie_7022_write(struct pxi_card_t *p, void *buf, size_t size)
{
    if (p == NULL)
        return -1;

    return write(p->h_dma, buf, size);
}

//void pxie_7022_trig(struct pxi_card_t *p)
//{
//    if (p == NULL)
//        return;
//
//    printf("\n\n--> TRIGGER 7022.\n\n\n");
//    tri_io_set_input_da(p->user_base);
//    usleep(1000);
//}
void pxie_7022_trig(struct pxi_card_t *p)
{
    if (p == NULL)
        return;

    printf("\n\n-->DAC TRIGGER.\n\n\n");
    tri_io_set_input_da(p->user_base);
    usleep(1000);

    odr_set_output_da(p->user_base);
    usleep(1000);
}
void pxie_7022_sync(struct pxi_card_t *p)
{
    if (p == NULL)
        return;

    sync_tri_io_da(p->user_base);
    printf("\n\n-->DAC SYNC ADC.\n\n\n");
}
