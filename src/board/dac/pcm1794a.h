/*
 * pcm1794a.h
 *
 *      Author: biseadmin
 */

#ifndef BOARD_DAC_PCM1794A_H_
#define BOARD_DAC_PCM1794A_H_

#include "../../xilinx/gpio/xgpio.h"
#include "../../xilinx/comm/xparameters.h"
#include "../../xilinx/spi/xspi.h"

void fir_set_input_da(void *dev, int regval);
void tri_io_set_input_da(void *dev);
void pcm1794a_enable(void *base_addr);
void pcm1794a_disable(void *base_addr);
void odr_set_output_da(void *base_addr);
void sync_tri_io_da(void *base_addr);

#endif /* BOARD_DAC_PCM1794A_H_ */
