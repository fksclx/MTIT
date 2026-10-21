#pragma once

#include <stdio.h>
#include <stdint.h>
#include "upper_computer/protoHelper.h"
#include "card/cards.h"
#include <stdbool.h>
#include "visa.h"


#define MAX_CARDS 16
#define MAX_CHANNELS_PER_CARD 16

typedef struct {
    struct pxi_card_t *card;
    int samplingRate;
    int senserType[MAX_CHANNELS_PER_CARD];
    int channelCount;
    int ret;
    int cardIndex;
} thread_param_t;

#if __BYTE_ORDER == __LITTLE_ENDIAN
#  define ltohl(x)       (x)
#  define ltohs(x)       (x)
#  define htoll(x)       (x)
#  define htols(x)       (x)
#elif __BYTE_ORDER == __BIG_ENDIAN
#  define ltohl(x)     __bswap_32(x)
#  define ltohs(x)     __bswap_16(x)
#  define htoll(x)     __bswap_32(x)
#  define htols(x)     __bswap_16(x)
#endif

#define MAP_SIZE (1024*1024UL)
#define PXI_GA_BASEADDR 0x00000000
#define PI acos(-1.0)

void pxi_reg_write(uintptr_t address, unsigned int val);

unsigned int pxi_reg_read(uintptr_t address);

#define MAX_PXI_SLOTS 18//16

typedef struct pxi_sys_t
{
    struct pxi_card_t r_cards[MAX_PXI_SLOTS];
    struct pxi_card_t w_cards[MAX_PXI_SLOTS];
    struct pxi_card_t io_cards[MAX_PXI_SLOTS];
    int n_cards;
    int n_wCards;
    int n_rCards;
    int n_IOCards;
    int *iActiveChnl;   //´æ´¢¸÷¸öÍ¨µÀÊÇ·ñ¼¤»î£¬²»¼¤»îµÄ²»²É¼¯
    int n_wchnl;        //ºóÐø»òÐíÐèÒª»»³ÉÊý×é
    int flag;
} pxi_sys_t;
int UpdateHardware();
void readDeviceCards();
void delete_directory(const char *path,bool deleteSelf);

int getHardwareOneSapmle();
int hardwareSapmle(void **data,int cardIndex,bool *isOk);


