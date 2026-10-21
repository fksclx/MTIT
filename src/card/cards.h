/*
 * cards.h
 *
 *  Created on: 2022��4��15��
 *      Author: biseadmin
 */

#ifndef CARD_CARDS_H_
#define CARD_CARDS_H_

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

struct pxi_card_t
{
    int h_user;
    void *user_base;
    int phy_slot;
    int card_num;
    int card_type;
    int h_dma;
    int (*init)(struct pxi_card_t *p, int sampleRate, int* cfg);
    void (*deinit)(struct pxi_card_t *p);
    int (*read)(struct pxi_card_t *p, void *buf, size_t size);
    int (*write)(struct pxi_card_t *p, void *buf, size_t size);
    void (*trig)(struct pxi_card_t *p);
    void (*trig_o)(struct pxi_card_t *p);
    void (*trigOUT)(struct pxi_card_t *p);
    void (*sync)(struct pxi_card_t *p);
    void (*syncin)(struct pxi_card_t *p);
    void (*deinittmp)(struct pxi_card_t *p);
    int (*inittmp)(struct pxi_card_t *p, int64_t samplingRate);
    int (*setCfg)(struct pxi_card_t *p, int excit_src, int coupl_way, int iftermCfg, int gain);
    void (*resetCfg)(struct pxi_card_t *p);
};

#endif /* CARD_CARDS_H_ */
