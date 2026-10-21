#ifndef PXIE_7023_H
#define PXIE_7023_H
#include <stddef.h>
#include <stdint.h>
int pxie_7023_init(struct pxi_card_t *p, int samplingRate, int* cfg);
void pxie_7023_deinit(struct pxi_card_t *p);
int pxie_7023_read(struct pxi_card_t *p, void *buf, size_t size);
void pxie_7023_CfgReset(struct pxi_card_t *p);
int pxie_7023_CfgSet(struct pxi_card_t *p, int excit_src, int coupl_way, int termCfg, int gain);
void pxie_7023_odrtrig(struct pxi_card_t *p);
void pxie_7023_odrtrig_o(struct pxi_card_t *p);
void pxie_7023_sync(struct pxi_card_t *p);
void pxie_7023_syncIN(struct pxi_card_t *p);
void pxie_7023_deinit_tmp(struct pxi_card_t *p);
int pxie_7023_init_tmp(struct pxi_card_t *p, int64_t samplingRate);
#endif // PXIE_7023_H
