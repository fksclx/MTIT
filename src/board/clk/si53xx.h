#ifndef BOARD_CLK_SI53XX_H_
#define BOARD_CLK_SI53XX_H_
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
//#include "../xdma/xdma.h"
#include "../../xilinx/iic/xiic.h"
//#include "../xilinx/iic/xiic_l.h"
//#include "../xilinx/iic/xparameters.h"
//#include "usrDAQmx.h"
struct reg_data{
    u8 reg_27;
    u8 reg_29;
    u8 reg_31;
    u8 reg_33;
    u8 reg_35;
    u8 reg_37;
    u8 reg_39;
    u8 reg_41;
    u8 reg_43;
    u8 reg_45;
};
//int si53xx_initDAC(HANDLE dev);
//int si53xx_initADC(HANDLE dev);
void si53xx_term(void *dev);
int si53xx_initR(void *dev, uint32_t fout, uint32_t select_ad);
int si53freqcal(uint32_t fin, uint32_t fout, struct reg_data *reg_data);
#endif /* BOARD_CLK_SI53XX_H_ */
