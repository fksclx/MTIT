/*
 * pca9555.h
 *
 *  Created on: 2022Äê4ÔÂ20ÈÕ
 *      Author: biseadmin
 */

#ifndef BOARD_IO_PCA9555_H_
#define BOARD_IO_PCA9555_H_
//#include <windows.h>
#include <stdint.h>
void pca9555_init(void *dev, uint8_t chip_id);
void pca9555_pin_set(void * dev, uint8_t chip_id, uint8_t pinset, uint8_t pinnum);
void pca9555_pin_clr(void * dev, uint8_t chip_id, uint8_t pinset, uint8_t pinnum);
void pca9555_iepe_pin_set(void *base_addr, uint8_t chip_id, uint8_t pinset, uint8_t pinnum, int setValue);

#endif /* BOARD_IO_PCA9555_H_ */
