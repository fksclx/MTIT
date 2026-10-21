/*
 * pxie_7022.h
 *
 *  Created on: 2022Äê8ÔÂ15ÈÕ
 *      Author: biseadmin
 */

#ifndef CARD_PXIE_7022_H_
#define CARD_PXIE_7022_H_

int pxie_7022_init(struct pxi_card_t *p, int sampleRate, int *cfg);
void pxie_7022_deinit(struct pxi_card_t *p);
int pxie_7022_read(struct pxi_card_t *p, void *buf, size_t size);
int pxie_7022_write(struct pxi_card_t *p, void *buf, size_t size);
void pxie_7022_trig(struct pxi_card_t *p);
void pxie_7022_sync(struct pxi_card_t *p);

#endif /* CARD_PXIE_7022_H_ */
