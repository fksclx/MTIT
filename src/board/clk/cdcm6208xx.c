/*
 * cdcm6208xx.c
 *
 *  Created on: 2022Äê4ÔÂ14ÈÕ
 *      Author: biseadmin
 */

#include "../../xilinx/gpio/xgpio.h"
#include "../../xilinx/comm/xparameters.h"
#include "../../xilinx/spi/xspi.h"

#include <stdio.h>
#include <unistd.h>

#define CDCM6208_BaseAddress        0x00040000
#define CDCM6208_GPIO_BaseAddress   0x00030000

#define CDCM6208_GPIO_PDN       0x0004
#define CDCM6208_GPIO_RST       0x0002
#define CDCM6208_GPIO_SYNC      0x0001

void cdcm6208_enable(void *base_addr)
{
    uintptr_t base = (uintptr_t) base_addr;

    XGpio_WriteReg(base + CDCM6208_GPIO_BaseAddress, XGPIO_TRI_OFFSET, 0x0);
    XGpio_WriteReg(
            base + CDCM6208_GPIO_BaseAddress, XGPIO_DATA_OFFSET,
            (CDCM6208_GPIO_PDN | CDCM6208_GPIO_RST /*| CDCM6208_GPIO_SYNC*/));
}

void cdcm6208_disable(void *base_addr)
{
    uintptr_t base = (uintptr_t) base_addr;

    XGpio_WriteReg(base + CDCM6208_GPIO_BaseAddress, XGPIO_TRI_OFFSET, 0x0);
    XGpio_WriteReg(base + CDCM6208_GPIO_BaseAddress, XGPIO_DATA_OFFSET, 0);
}

static void cdcm6208_output(uintptr_t base)
{
    XGpio_WriteReg(base + CDCM6208_GPIO_BaseAddress, XGPIO_TRI_OFFSET, 0x0);
    XGpio_WriteReg(base + CDCM6208_GPIO_BaseAddress, XGPIO_DATA_OFFSET,
                   CDCM6208_GPIO_PDN | CDCM6208_GPIO_RST| CDCM6208_GPIO_SYNC);
}

static u16 cdcm6208_spi_trans(uintptr_t base, u32 buf)
{
    u32 Control;
    u32 Buffer;

    Buffer = buf;

    XSpi_WriteReg(base + CDCM6208_BaseAddress, XSP_SRR_OFFSET,
                  XSP_SRR_RESET_MASK);
    XSpi_WriteReg(base + CDCM6208_BaseAddress, XSP_SSR_OFFSET, 0x01);

    /*
     * Set up the device in enable master mode.
     */
    Control = XSpi_ReadReg(base + CDCM6208_BaseAddress, XSP_CR_OFFSET);
    Control |= (XSP_CR_MASTER_MODE_MASK);
    XSpi_WriteReg(base + CDCM6208_BaseAddress, XSP_CR_OFFSET, Control);

    XSpi_WriteReg(base + CDCM6208_BaseAddress, XSP_SSR_OFFSET, 0x00);

    /*
     * Fill up the transmitter with data, assuming the receiver can hold
     * the same amount of data.
     */
    while ((XSpi_ReadReg(base + CDCM6208_BaseAddress, XSP_SR_OFFSET) &
    XSP_SR_TX_FULL_MASK) == 0)
    {
        XSpi_WriteReg((base + CDCM6208_BaseAddress), XSP_DTR_OFFSET, Buffer);
    }

    /*
     * Enable the device.
     */
    Control = XSpi_ReadReg(base + CDCM6208_BaseAddress, XSP_CR_OFFSET);
    Control |= XSP_CR_ENABLE_MASK;
    Control &= ~XSP_CR_TRANS_INHIBIT_MASK;
    XSpi_WriteReg(base + CDCM6208_BaseAddress, XSP_CR_OFFSET, Control);

    /*
     * Initialize the buffer with zeroes so that it can be used to receive
     * data.
     */
    Buffer = 0;

    /*
     * Wait for the transmit FIFO to transition to empty before checking
     * the receive FIFO, this prevents a fast processor from seeing the
     * receive FIFO as empty
     */
    while (!(XSpi_ReadReg(base + CDCM6208_BaseAddress, XSP_SR_OFFSET) &
    XSP_SR_TX_EMPTY_MASK))
        ;

    /*
     * Transmitter is full, now receive the data just looped back until
     * the receiver is empty.
     */
    while ((XSpi_ReadReg(base + CDCM6208_BaseAddress, XSP_SR_OFFSET) &
    XSP_SR_RX_EMPTY_MASK) == 0)
    {
        Buffer = XSpi_ReadReg((base + CDCM6208_BaseAddress), XSP_DRR_OFFSET);
    }

    XSpi_WriteReg(base + CDCM6208_BaseAddress, XSP_SSR_OFFSET, 0x01);

    return (Buffer & 0xFFFF);
}

u16 cdcm6208_read(uintptr_t base, u16 addr)
{
    u32 buf = (0x8000 | addr) << 16;
    return cdcm6208_spi_trans(base, buf);
}

void cdcm6208_write(uintptr_t base, u32 addr_data)
{
    cdcm6208_spi_trans(base, addr_data);
}

//u32 reg_tab_13M1[21] = { 0x000001B9, 0x0001018C, 0x00023A82, 0x00030070,
//                         0x000420D7, 0x00050199, 0x00060000, 0x00070199,
//                         0x00080000, 0x00090011, 0x000A0000, 0x000B0000,
//                         0x000C00D1, 0x000D0000, 0x000E0000, 0x000F0013,
//                         0x001003A0, 0x00110000, 0x00120053, 0x001303A0,
//                         0x00140000 };
//
//u32 reg_tab_26M2[21] = { 0x00000239, 0x000100C4, 0x00021C82, 0x00030070,
//                         0x000420D7, 0x00050199, 0x00060000, 0x00070199,
//                         0x00080000, 0x00090011, 0x000A0000, 0x000B0000,
//                         0x000C00D1, 0x000D0000, 0x000E0000, 0x000F0011,
//                         0x001001C0, 0x00110000, 0x00120053, 0x001301C0,
//                         0x00140000 };
//
//u32 reg_tab_26M2144[21] = { 0x000001B1, 0x000130D3, 0x00029FFF, 0x00030070,
//                            0x000420D7, 0x00050199, 0x00060000, 0x00070199,
//                            0x00080000, 0x00090011, 0x000A0000, 0x000B0000,
//                            0x000C00D1, 0x000D0000, 0x000E0000, 0x000F0013,
//                            0x001001C0, 0x00110000, 0x00120053, 0x001301C0,
//                            0x00140000 };
//u32 reg_tab_26M2OLD[21] = { 0x00000239, 0x000100C4, 0x00021C82, 0x00030070,
//                         0x000420D7, 0x00050199, 0x00060000, 0x00070199,
//                         0x00080000, 0x00090011, 0x000A0000, 0x000B0000,
//                         0x000C00D1, 0x000D0000, 0x000E0000, 0x000F0011,
//                         0x001001C0, 0x00110000, 0x00120053, 0x001301C0,
//                         0x00140000 };

/*
u32 reg_tab_26M2144[21] = { 0x000001B1, 0x000130D3, 0x00029FFF, 0x00030070,
                            0x000420D7, 0x00050199, 0x00060000, 0x00070199,
                            0x00080000, 0x00090011, 0x000A0000, 0x000B0000,
                            0x000C00D1, 0x000D0000, 0x000E0000, 0x000F0013,
                            0x001001C0, 0x00110000, 0x00120053, 0x001301C0,
                            0x00140000 };*/

u32 reg_tab_6M55[21] = { 0x00000231, 0x0001031C, 0x00027682, 0x000300F0,
                         0x00042097, 0x00050001, 0x00060000, 0x00070001,
                         0x00080000, 0x00090001, 0x000A0000, 0x000B0000,
                         0x000C0001, 0x000D0000, 0x000E0000, 0x000F0001,
                         0x00100000, 0x00110000, 0x00120053, 0x00130760,
                         0x00140000 };

u32 reg_tab_26M2TEST[21] = { 0x00000231, 0x000100C4, 0x00021C82, 0x000300F0,
                         0x00042097, 0x00050001, 0x00060000, 0x00070001,
                         0x00080000, 0x00090011, 0x000A0000, 0x000B0000,
                         0x000C0001, 0x000D0000, 0x000E0000, 0x000F0001,
                         0x00100000, 0x00110000, 0x00120053, 0x001301C0,
                         0x00140000 };

u32 reg_tab_13M1[21] = { 0x00000231, 0x0001018C, 0x00023A82, 0x000300F0,
                         0x00042097, 0x00050001, 0x00060000, 0x00070001,
                         0x00080000, 0x00090001, 0x000A0000, 0x000B0000,
                         0x000C0001, 0x000D0000, 0x000E0000, 0x000F0001,
                         0x00100000, 0x00110000, 0x00120053, 0x001303A0,
                         0x00140000 };

u32 reg_tab_26M2[21] = { 0x00000231, 0x000100C4, 0x00021C82, 0x000300F0,
                         0x00042097, 0x00050001, 0x00060000, 0x00070001,
                         0x00080000, 0x00090011, 0x000A0000, 0x000B0000,
                         0x000C0001, 0x000D0000, 0x000E0000, 0x000F0001,
                         0x00100000, 0x00110000, 0x00120053, 0x001301C0,
                         0x00140000 };  // 26.2002M  Reg1, Reg2, Reg19 / reg18

u32 reg_tab_52M4[21] = { 0x00000231, 0x00010060, 0x00020E82, 0x000300F0,
                         0x00042097, 0x00050001, 0x00060000, 0x00070001,
                         0x00080000, 0x00090011, 0x000A0000, 0x000B0000,
                         0x000C0001, 0x000D0000, 0x000E0000, 0x000F0001,
                         0x00100000, 0x00110000, 0x00120053, 0x001300E0,
                         0x00140000 };

u32 reg_tab_3M275[21] = { 0x00000231, 0x0001031C, 0x00027682, 0x000300F0,
                         0x00042097, 0x00050001, 0x00060000, 0x00070001,
                         0x00080000, 0x00090001, 0x000A0000, 0x000B0000,
                         0x000C0001, 0x000D0000, 0x000E0000, 0x000F0001,
                         0x00100000, 0x00110000, 0x00120253, 0x00130760,
                         0x00143C3C };

u32 reg_tab_3M4[21] = { 0x00000231, 0x00010024, 0x0002066E, 0x000300F0,
                         0x00042097, 0x00050181, 0x00060000, 0x00070001,
                         0x00080000, 0x00090001, 0x000A0000, 0x000B0000,
                         0x000C0001, 0x000D0000, 0x000E0000, 0x000F0001,
                         0x00100000, 0x00110000, 0x00120253, 0x00130714,
                         0x00143C3C};
int cdcm6208_init_0(void *base_addr, int sampleRate)
{

    uintptr_t base = (uintptr_t) base_addr;
    int n_max = 100;

    printf("--> CDCM6208 CLOCK CONFIG TO...\n");


    if(sampleRate == 6400 )
    {
        for (int i = 0; i < 21; i++)
        {
            cdcm6208_write(base, reg_tab_13M1[i]);
        }
    }
    else if(sampleRate == 12800 )
    {
        for (int i = 0; i < 21; i++)
        {
            cdcm6208_write(base, reg_tab_26M2[i]);
        }
    }

    for (int i = 0; i < n_max; i++)
    {
        usleep(100000);
        u16 reg = cdcm6208_read(base, 21);
        if (!(reg & 0x0004))
        {
            printf("--> CDCM6208 PLL LOCKED!\n");
            break;
        }

        if(i == (n_max -1))
        {
            printf("--> CDCM6208 PLL FAILED!!!!!!!!!!!!!!!\n");
            return -1;
        }
    }

    cdcm6208_output(base);

    return 0;
}
int cdcm6208_init(void* dev, long long int samplingRate) //, int64 samplingRate
{
    int n_max = 100;
    u32 reg_tab[21];
    long long int oversampRate[] = { 409600, 204800, 102400, 51200, 25600, 12800, 6400,
            3200, 1600 };
    long long int sRate;
    uintptr_t base = (uintptr_t) dev;

    //printf("--> CDCM6208 CLOCK CONFIG TO..oversampRate1 %lld.\n", oversampRate[0]);
    double divcount;
    u32 coef1, coef2;
    int len = sizeof(oversampRate) / sizeof(double);

    if (samplingRate >= 409600) {
        for (int i = 0; i < 21; i++) {
            reg_tab[i] = reg_tab_52M4[i];
        }
        sRate = 409600;
        //printf("-->XXXX1 CDCM6208 CLOCK select sRate %lld...\n", sRate);

        divcount = sRate * 1.0 / samplingRate;
        coef1 = 25 * divcount - 1;
        coef2 = 14 * divcount - 1;
    } else if (samplingRate >= 3200 && samplingRate < 409600) {
        for (int i = 0; i < 21; i++) {
            reg_tab[i] = reg_tab_26M2[i];
        }

        for (int j = len - 1; j >= 0; j--) {

            if (samplingRate < oversampRate[j]) {
                sRate = oversampRate[j + 1];
                break;
            }
        }
        printf("-->XXXX2 CDCM6208 CLOCK select sRate %lld...\n", sRate);

        divcount = sRate * 1.0 / samplingRate;
        coef1 = 50 * divcount - 1;
        coef2 = 28 * divcount - 1; // coef2 = coef19
    } else if (samplingRate > 800 && samplingRate < 3200) {
        for (int i = 0; i < 21; i++) {
            reg_tab[i] = reg_tab_13M1[i];
        }
        printf("-->XXXX3 CDCM6208 CLOCK select 2000@@@@ \n");
        divcount = 800.0 / samplingRate;
        coef1 = 100 * divcount - 1;
        coef2 = 58 * divcount - 1; // coef2 = coef19
    } else if(samplingRate > 200 && samplingRate <= 800){

        for (int i = 0; i < 21; i++) {
            reg_tab[i] = reg_tab_6M55[i];
        }
        printf("-->XXXX4 CDCM6208 CLOCK select 2000@@@@ \n");
        divcount = 200.0 / samplingRate;
        coef1 = 200 * divcount - 1;
        coef2 = 118 * divcount - 1; // coef2 = coef19

    }else if(samplingRate > 50 && samplingRate <= 200) {

        for (int i = 0; i < 21; i++) {
            reg_tab[i] = reg_tab_3M275[i];
        }
        divcount = 50.0 / samplingRate;
        coef1 = 400 * divcount - 1;
        coef2 = 238 * divcount - 1; // coef2 = coef19, 400, 238
        printf("-->XXXX5 CDCM6208 CLOCK select 2000@@@@ \n");
    }else{
        for (int i = 0; i < 21; i++) {
            reg_tab[i] = reg_tab_3M275[i];
        }
        reg_tab[18] = 0x00120653;
        divcount = (100.0 / 3) / samplingRate;
        coef1 = 400 * divcount - 1;
        coef2 = 238 * divcount - 1; // coef2 = coef19, 400, 238
        printf("-->XXXX6 CDCM6208 CLOCK select 2000@@@@ \n");
    }
    reg_tab[1] = ((coef1 << 2) & 0xFFFF) + 0x10000;
    reg_tab[2] = (((coef2 << 8) + 0x0082) & 0xFFFF) + 0x20000;
    reg_tab[19] = ((coef2 << 4) & 0xFFFF) + 0x130000;
    printf("--> CDCM6208 reg1: %u, divcount: %f, coef :%d\n", reg_tab[1], divcount, coef1);

    for (int i = 0; i < 21; i++) {
        cdcm6208_write(base, reg_tab[i]); // reg_tab_9M825[i]
    }
    for (int i = 0; i < n_max; i++) {
        usleep(100000);
        u16 reg = cdcm6208_read(base, 21);
        if (!(reg & 0x0004)) {
            printf("--> CDCM6208 PLL LOCKED!\n");
            break;
        }

        if (i == (n_max - 1)) {
            printf("--> CDCM6208 PLL FAILED!!!!!!!!!!!!!!!\n");
            return -1;
        }
    }
    cdcm6208_output(base);
    return 0;
}

