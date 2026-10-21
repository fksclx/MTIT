/*
 * pxie_7021.h
 *
 *  Created on: 2022Äê4ÔÂ15ÈÕ
 *      Author: biseadmin
 */

#ifndef CARD_PXIE_7021_H_
#define CARD_PXIE_7021_H_

int pxie_7021_init(struct pxi_card_t *p, int sampleRate, int *cfg);
void pxie_7021_deinit(struct pxi_card_t *p);
int pxie_7021_read(struct pxi_card_t *p, void *buf, size_t size);
void pxie_7021_trig(struct pxi_card_t *p);

#endif /* CARD_PXIE_7021_H_ */
