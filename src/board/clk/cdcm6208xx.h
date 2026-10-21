/*
 * cdcm6208xx.h
 *
 *  Created on: 2022Äê4ÔÂ15ÈÕ
 *      Author: biseadmin
 */

#ifndef BOARD_CLK_CDCM6208XX_H_
#define BOARD_CLK_CDCM6208XX_H_

void cdcm6208_enable(void *base_addr);
void cdcm6208_disable(void *base_addr);

int cdcm6208_init(void *base_addr, long long int sampleRate);

#endif /* BOARD_CLK_CDCM6208XX_H_ */
