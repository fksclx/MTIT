/*
 * pca9555.c
 *
 *  Created on: 2022
 *      Author: biseadmin
 */
//#include <Windows.h>
//#include "../../xilinx/iic/xiic.h"

#include "../../xilinx/iic/xiic.h"
//#include "../../xilinx/comm/xparameters.h"

#include <stdio.h>
#include <unistd.h>

#define GERENAL_I2C_BASEADDR 0x00030000  //SI53XXI2C

u8 pca9555_read(void * dev, u8 addr_7bit, u8 cmd_byte)
{
    int ByteCount;
    u8 data;

    XIic_Send((uintptr_t)dev + GERENAL_I2C_BASEADDR, addr_7bit, &cmd_byte, 1, XIIC_STOP);
    ByteCount = XIic_Recv((uintptr_t)dev + GERENAL_I2C_BASEADDR, addr_7bit, &data, 1, XIIC_STOP);
//printf("ByteCount 0x%x\n", ByteCount);
    if(ByteCount != 1)
        return -1;

    return data;
}

void pca9555_write(void * dev, u8 addr_7bit, u8 cmd_byte, u8 data)
{
    u8 sendBuf[2];

    sendBuf[0] = cmd_byte;
    sendBuf[1] = data;

    XIic_Send((uintptr_t)dev + GERENAL_I2C_BASEADDR, addr_7bit, sendBuf, 2, XIIC_STOP);
}

void pca9555_init(void *dev, uint8_t chip_id)
{

//    //output reg set 0
    pca9555_write(dev, chip_id, 0x02, 0x0);
    pca9555_write(dev, chip_id, 0x03, 0x0);


    //enable pin as output
    pca9555_write(dev, chip_id, 0x06, 0x0);
    pca9555_write(dev, chip_id, 0x07, 0x0);

   // printf("--> PCA9555 0x%02x set output logic 0.\n", chip_id);
}

//void pca9555_init(void * dev, uint8_t chip_id)
//{
////    printf("--> PCA9555 Reg 0 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x00));
////    printf("--> PCA9555 Reg 1 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x01));
////    printf("--> PCA9555 Reg 2 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x02));
////    printf("--> PCA9555 Reg 3 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x03));
////    printf("--> PCA9555 Reg 4 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x04));
////    printf("--> PCA9555 Reg 5 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x05));
////    printf("--> PCA9555 Reg 6 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x06));
////    printf("--> PCA9555 Reg 7 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x07));
////    //output reg set 0
//    pca9555_write(dev, chip_id, 0x02, 0x0);
//    pca9555_write(dev, chip_id, 0x03, 0x0);
//
//
////    printf("--> PCA9555 Reg 0 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x00));
////    printf("--> PCA9555 Reg 1 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x01));
////    printf("--> PCA9555 Reg 2 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x02));
////    printf("--> PCA9555 Reg 3 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x03));
////    printf("--> PCA9555 Reg 4 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x04));
////    printf("--> PCA9555 Reg 5 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x05));
////    printf("--> PCA9555 Reg 6 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x06));
////    printf("--> PCA9555 Reg 7 : 0x%02x set output logic 0.\n", pca9555_read(dev, chip_id, 0x07));
//
//    //enable pin as output
//    pca9555_write(dev, chip_id, 0x06, 0x0);
//    pca9555_write(dev, chip_id, 0x07, 0x0);
//
//    printf("--> PCA9555 0x%02x set output logic 0.\n", chip_id);
//}

void pca9555_pin_set(void * dev, uint8_t chip_id, uint8_t pinset, uint8_t pinnum)
{
    u8 reg;

    if(pinset == 0)
        reg = 0x02;
    else if(pinset == 1)
        reg = 0x03;
    else
        return;

    u8 regval = pca9555_read(dev, chip_id, reg);
    u8 mask = 1 << pinnum;

    regval |= mask;

    pca9555_write(dev, chip_id, reg, regval);

    regval = pca9555_read(dev, chip_id, reg);
    //printf("--> PCA9555: reg 0x%02x: 0x%02x\n", reg, regval);
}
void pca9555_iepe_pin_set(void * dev, uint8_t chip_id, uint8_t pinset, uint8_t pinnum, int setValue)
{

    u8 reg;

    if(pinset == 0)
        reg = 0x02;
    else if(pinset == 1)
        reg = 0x03;
    else
        return;

    u8 regval = pca9555_read(dev, chip_id, reg);
    u8 mask = setValue << pinnum;

    regval |= mask;

    pca9555_write(dev, chip_id, reg, regval);

    regval = pca9555_read(dev, chip_id, reg);
    //printf("--> PCA9555 setValue 0x%02x iepe: reg 0x%02x: 0x%02x, %d\n", setValue, reg, regval, regval);

}
void pca9555_pin_clr(void * dev, uint8_t chip_id, uint8_t pinset, uint8_t pinnum)
{
    u8 reg;

    if(pinset == 0)
        reg = 0x02;
    else if(pinset == 1)
        reg = 0x03;
    else
        return;

    u8 regval = pca9555_read(dev, chip_id, reg);
    u8 mask = 1 << pinnum;

    regval &= (~mask);

    pca9555_write(dev, chip_id, reg, regval);

    regval = pca9555_read(dev, chip_id, reg);
    //printf("--> PCA9555 clear: reg 0x%02x: 0x%02x\n", reg, regval);
}
