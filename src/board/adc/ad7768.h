
#ifndef BOARD_ADC_AD7768_H_
#define BOARD_ADC_AD7768_H_

#include "../../xilinx/gpio/xgpio.h"
#include "../../xilinx/comm/xparameters.h"
#include "../../xilinx/spi/xspi.h"

void ad7768_enable(void *base_addr);
void ad7768_disable(void *base_addr);

void ad7768_start_signal(void *base_addr);
void freqfir_select(void *base_addr, uint32_t regVal);
//u8 ad7768_reg_read(u8 addr);
//void ad7768_reg_write(void *base_addr, u8 addr, u8 data);
int ad7768_init(void *base_addr, uint32_t decRate);

void ad7768_start_SW(void *base_addr);

void tri_io_set_input(void *base_addr);

u8 ad7768_reg_read(uintptr_t base, u8 addr);

void ad7768_reg_write(uintptr_t base, u8 addr, u8 data);

#endif /* BOARD_ADC_AD7768_H_ */
