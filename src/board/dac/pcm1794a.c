/*
 * pcm1794a.c
 *

 *      Author: biseadmin
 */


#include <stdio.h>
#include <unistd.h>
#include "../adc/ad7768.h"

#define PCM1794A_GPO_BASEADDR     0x00010000
#define PCM1794A_TRG_BASEADDR     0x00060000
#define TRI_IO_BASEADDR           0x00050000
#define FIR_IO_BASEADDR           0x00020000
#define PCM1794A_GPO_RESET_MASk   0x0001
#define PCM1794A_GPO_SMUTE_MASK   0x0002
#define PCM1794A_GPO_SMURE_MASK   0x0003
#define ADC_TRIG_BASEADDR           0x00090000
#define ODR_IO_BASEADDR           0x00080000

void fir_set_input_da(void *dev, uint32_t regval) {

    uintptr_t base = (uintptr_t)dev;

    XGpio_WriteReg(base + FIR_IO_BASEADDR, XGPIO_TRI_OFFSET, 0x00);
    XGpio_WriteReg(base + FIR_IO_BASEADDR, XGPIO_DATA_OFFSET, regval);
    usleep(1000);

}
void tri_io_set_input_da(void *dev) {

    uintptr_t base = (uintptr_t)dev;

    XGpio_WriteReg(base + PCM1794A_TRG_BASEADDR, XGPIO_TRI_OFFSET, 0x00);
    XGpio_WriteReg(base + PCM1794A_TRG_BASEADDR, XGPIO_DATA_OFFSET, 0x00);

    XGpio_WriteReg(base + TRI_IO_BASEADDR, XGPIO_TRI_OFFSET, 0xFE);//FD
    XGpio_WriteReg(base + TRI_IO_BASEADDR, XGPIO_DATA_OFFSET, 0xFF);

    usleep(1000);
    XGpio_WriteReg(base + TRI_IO_BASEADDR, XGPIO_DATA_OFFSET, 0x00);

    usleep(1000);
    XGpio_WriteReg(base + TRI_IO_BASEADDR, XGPIO_DATA_OFFSET, 0XFF);

    XGpio_WriteReg(base + TRI_IO_BASEADDR, XGPIO_TRI_OFFSET, 0xFF);
    //write_device(dev, PCM1794A_TRG_BASEADDR + XGPIO_TRI_OFFSET, REG_SIZE, 0xFF);



}


void pcm1794a_enable(void *base_addr)
{
    uintptr_t base = (uintptr_t)base_addr;

    XGpio_WriteReg(base + PCM1794A_GPO_BASEADDR, XGPIO_TRI_OFFSET, 0x0);
    XGpio_WriteReg(base + PCM1794A_GPO_BASEADDR, XGPIO_DATA_OFFSET, PCM1794A_GPO_RESET_MASk);
 //   XGpio_WriteReg(base + PCM1794A_GPO_BASEADDR, XGPIO_DATA_OFFSET, PCM1794A_GPO_SMURE_MASK);
    printf("--> PCM1794A ENABLED.\n");

//    XGpio_WriteReg(base + PCM1794A_TRG_BASEADDR, XGPIO_TRI_OFFSET, 0x0);
//    XGpio_WriteReg(base + PCM1794A_TRG_BASEADDR, XGPIO_DATA_OFFSET, 0x1);
}

void pcm1794a_disable(void *base_addr)
{
    uintptr_t base = (uintptr_t)base_addr;

    XGpio_WriteReg(base + PCM1794A_GPO_BASEADDR, XGPIO_TRI_OFFSET, 0x0);
    XGpio_WriteReg(base + PCM1794A_GPO_BASEADDR, XGPIO_DATA_OFFSET, 0);

    printf("--> PCM1794A DISABLED.\n");
}
void sync_tri_io_da(void *base_addr) {

    printf("sync_tri_io_da\n");
    uintptr_t base = (uintptr_t)base_addr;

    XGpio_WriteReg(base + ADC_TRIG_BASEADDR, XGPIO_TRI_OFFSET, 0x0);
    XGpio_WriteReg(base + ADC_TRIG_BASEADDR , XGPIO_DATA_OFFSET, 0xf);

    XGpio_WriteReg(base + ADC_TRIG_BASEADDR , XGPIO_DATA_OFFSET, 0x0);

    XGpio_WriteReg(base + ADC_TRIG_BASEADDR , XGPIO_DATA_OFFSET, 0xf);

}
void odr_set_output_da(void *base_addr) {

    uintptr_t base = (uintptr_t)base_addr;
    XGpio_WriteReg(base + ODR_IO_BASEADDR , XGPIO_DATA_OFFSET, 0x0);  //ODR

}
