#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <getopt.h>
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
#include <dirent.h>
#include <errno.h>
#include <stddef.h>


#include "../board/io/pca9555.h"
//#include "../board/io/timer_o.h"
//#include "../board/io/trig_set.h"
#include "../card/cards.h"
//#include "../rw_register.h"
#include "../board/adc/ad4134.h"
#include "../xilinx/gpio/xgpio_l.h"
#include "../xilinx/gpio/xgpio.h"

#define PXI_GA_BASEADDR (0x00000000)
#define PCA_I2C_ADDR_U23      (0x20)
#define PCA_I2C_ADDR_U6       (0x21)
#define PCA_I2C_ADDR_U36      (0x22)
#define PCA_I2C_ADDR_U49      (0x23)
#define PCA_I2C_ADDR2_U23     (0x24)
#define PCA_I2C_ADDR2_U6      (0x25)
#define PCA_I2C_ADDR2_U36     (0x26)
#define PCA_I2C_ADDR2_U49     (0x27)
#define REG_SIZE 4
void chann_close_7023(void* dev, int num)
{
    uint8_t pinset, pinnum, cid1;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR_U6;
        pinset = 0; pinnum = 1;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR_U6;
        pinset = 0; pinnum = 7;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR_U23;
        pinset = 0; pinnum = 2;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR_U23;
        pinset = 1; pinnum = 5;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR_U36;
        pinset = 0; pinnum = 1;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR_U36;
        pinset = 0; pinnum = 7;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR_U49;
        pinset = 0; pinnum = 2;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR_U49;
        pinset = 1; pinnum = 2;
        break;
    default :
        return;
    }

    pca9555_pin_set(dev, cid1, pinset, pinnum);
    usleep(10000);
    pca9555_pin_clr(dev, cid1, pinset, pinnum);
    usleep(10000);
}
void chann_open_7023(void* dev, int num)
{
    uint8_t pinset, pinnum, cid1;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR_U6;
        pinset = 1; pinnum = 6;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR_U6;
        pinset = 0; pinnum = 2;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR_U6;
        pinset = 1; pinnum = 3;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR_U23;
        pinset = 0; pinnum = 3;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR_U23;
        pinset = 1; pinnum = 3;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR_U36;
        pinset = 0; pinnum = 2;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR_U36;
        pinset = 1; pinnum = 3;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR_U49;
        pinset = 0; pinnum = 3;
        break;
    default :
        return;
    }

    pca9555_pin_set(dev, cid1, pinset, pinnum);
    usleep(10000);
    pca9555_pin_clr(dev, cid1, pinset, pinnum);
    usleep(10000);
}
void chann2_close_7023(void* dev, int num)
{
    uint8_t pinset, pinnum, cid1;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR2_U6;
        pinset = 0; pinnum = 1;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR2_U6;
        pinset = 0; pinnum = 7;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR2_U23;
        pinset = 0; pinnum = 2;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR2_U23;
        pinset = 1; pinnum = 5;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR2_U36;
        pinset = 0; pinnum = 1;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR2_U36;
        pinset = 0; pinnum = 7;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR2_U49;
        pinset = 0; pinnum = 2;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR2_U49;
        pinset = 1; pinnum = 2;
        break;
    default :
        return;
    }

    pca9555_pin_set(dev, cid1, pinset, pinnum);
    usleep(10000);
    pca9555_pin_clr(dev, cid1, pinset, pinnum);
    usleep(10000);
}
void chann2_open_7023(void* dev, int num)
{
    uint8_t pinset, pinnum, cid1;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR2_U6;
        pinset = 1; pinnum = 6;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR2_U6;
        pinset = 0; pinnum = 2;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR2_U6;
        pinset = 1; pinnum = 3;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR2_U23;
        pinset = 0; pinnum = 3;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR2_U23;
        pinset = 1; pinnum = 3;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR2_U36;
        pinset = 0; pinnum = 2;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR2_U36;
        pinset = 1; pinnum = 3;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR2_U49;
        pinset = 0; pinnum = 3;
        break;
    default :
        return;
    }

    pca9555_pin_set(dev, cid1, pinset, pinnum);
    usleep(10000);
    pca9555_pin_clr(dev, cid1, pinset, pinnum);
    usleep(10000);
}
void gain_set(void* dev, int num, int setValue0, int setValue1)
{
    uint8_t ps1, pn1, ps2, pn2, cid1, cid2;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR_U6;
        ps1 = 1; pn1 = 5;
        cid2 = PCA_I2C_ADDR_U6;
        ps2 = 1; pn2 = 4;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR_U6;
        ps1 = 0; pn1 = 4;
        cid2 = PCA_I2C_ADDR_U6;
        ps2 = 0; pn2 = 5;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR_U6;
        ps1 = 1; pn1 = 1;
        cid2 = PCA_I2C_ADDR_U23;
        ps2 = 0; pn2 = 0;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR_U23;
        ps1 = 0; pn1 = 5;
        cid2 = PCA_I2C_ADDR_U23;
        ps2 = 0; pn2 = 6;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR_U23;
        ps1 = 1; pn1 = 0;
        cid2 = PCA_I2C_ADDR_U36;
        ps2 = 1; pn2 = 4;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR_U36;
        ps1 = 0; pn1 = 4;
        cid2 = PCA_I2C_ADDR_U36;
        ps2 = 0; pn2 = 5;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR_U36;
        ps1 = 1; pn1 = 1;
        cid2 = PCA_I2C_ADDR_U49;
        ps2 = 0; pn2 = 0;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR_U49;
        ps1 = 0; pn1 = 5;
        cid2 = PCA_I2C_ADDR_U49;
        ps2 = 0; pn2 = 7;
        break;
    default :
        return;
    }

    pca9555_iepe_pin_set(dev, cid1, ps1, pn1, setValue0);  // A0
    pca9555_iepe_pin_set(dev, cid2, ps2, pn2, setValue1);  // A1
}
void gain2_set(void* dev, int num, int setValue0, int setValue1)
{
    uint8_t ps1, pn1, ps2, pn2, cid1, cid2;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR2_U6;
        ps1 = 1; pn1 = 5;
        cid2 = PCA_I2C_ADDR2_U6;
        ps2 = 1; pn2 = 4;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR2_U6;
        ps1 = 0; pn1 = 4;
        cid2 = PCA_I2C_ADDR2_U6;
        ps2 = 0; pn2 = 5;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR2_U6;
        ps1 = 1; pn1 = 1;
        cid2 = PCA_I2C_ADDR2_U23;
        ps2 = 0; pn2 = 0;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR2_U23;
        ps1 = 0; pn1 = 5;
        cid2 = PCA_I2C_ADDR2_U23;
        ps2 = 0; pn2 = 6;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR2_U23;
        ps1 = 1; pn1 = 0;
        cid2 = PCA_I2C_ADDR2_U36;
        ps2 = 1; pn2 = 4;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR2_U36;
        ps1 = 0; pn1 = 4;
        cid2 = PCA_I2C_ADDR2_U36;
        ps2 = 0; pn2 = 5;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR2_U36;
        ps1 = 1; pn1 = 1;
        cid2 = PCA_I2C_ADDR2_U49;
        ps2 = 0; pn2 = 0;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR2_U49;
        ps1 = 0; pn1 = 5;
        cid2 = PCA_I2C_ADDR2_U49;
        ps2 = 0; pn2 = 7;
        break;
    default :
        return;
    }

    pca9555_iepe_pin_set(dev, cid1, ps1, pn1, setValue0);  // A0
    pca9555_iepe_pin_set(dev, cid2, ps2, pn2, setValue1);  // A1
}
void icp_set_7023(void* dev, int num, int excit_src, int iftermCfg)
{
    uint8_t ps1, pn1, ps2, pn2, cid1, cid2;
    int setValue;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR_U6;
        ps1 = 0; pn1 = 0;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U6;
        ps2 = 1; pn2 = 7;
        setValue = iftermCfg;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR_U6;
        ps1 = 0; pn1 = 6;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U6;
        ps2 = 0; pn2 = 3;
        setValue = iftermCfg;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR_U23;
        ps1 = 0; pn1 = 1;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U6;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR_U23;
        ps1 = 0; pn1 = 7;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U23;
        ps2 = 0; pn2 = 4;
        setValue = iftermCfg;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR_U36;
        ps1 = 0; pn1 = 0;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U23;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR_U36;
        ps1 = 0; pn1 = 6;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U36;
        ps2 = 0; pn2 = 3;
        setValue = iftermCfg;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR_U49;
        ps1 = 0; pn1 = 1;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U36;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR_U49;
        ps1 = 1; pn1 = 4;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR_U49;
        ps2 = 0; pn2 = 4;
        setValue = iftermCfg;
        break;
    default :
        return;
    }

    pca9555_iepe_pin_set(dev, cid1, ps1, pn1, setValue);
    pca9555_iepe_pin_set(dev, cid2, ps2, pn2, setValue);
}
void icp_close_7023(void* dev, int num)
{
    uint8_t ps1, pn1, ps2, pn2, cid1, cid2;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR_U6;
        ps1 = 0; pn1 = 0;
        cid2 = PCA_I2C_ADDR_U6;
        ps2 = 1; pn2 = 7;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR_U6;
        ps1 = 0; pn1 = 6;
        cid2 = PCA_I2C_ADDR_U6;
        ps2 = 0; pn2 = 3;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR_U23;
        ps1 = 0; pn1 = 1;
        cid2 = PCA_I2C_ADDR_U6;
        ps2 = 1; pn2 = 2;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR_U23;
        ps1 = 0; pn1 = 7;
        cid2 = PCA_I2C_ADDR_U23;
        ps2 = 0; pn2 = 4;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR_U36;
        ps1 = 0; pn1 = 0;
        cid2 = PCA_I2C_ADDR_U23;
        ps2 = 1; pn2 = 2;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR_U36;
        ps1 = 0; pn1 = 6;
        cid2 = PCA_I2C_ADDR_U36;
        ps2 = 0; pn2 = 3;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR_U49;
        ps1 = 0; pn1 = 1;
        cid2 = PCA_I2C_ADDR_U36;
        ps2 = 1; pn2 = 2;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR_U49;
        ps1 = 1; pn1 = 4;
        cid2 = PCA_I2C_ADDR_U49;
        ps2 = 0; pn2 = 4;
        break;
    default :
        return;
    }

    pca9555_pin_clr(dev, cid1, ps1, pn1);
    pca9555_pin_clr(dev, cid2, ps2, pn2);
}
void RSE_set_7023(void* dev, int num, int iftermCfg)
{
    uint8_t ps2, pn2, cid2;
    int setValue;

    switch (num) {
    case 1:
        cid2 = PCA_I2C_ADDR_U6;
        ps2 = 1; pn2 = 7;
        setValue = iftermCfg;
        break;
    case 2:
        cid2 = PCA_I2C_ADDR_U6;
        ps2 = 0; pn2 = 3;
        setValue = iftermCfg;
        break;
    case 3:
        cid2 = PCA_I2C_ADDR_U6;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 4:
        cid2 = PCA_I2C_ADDR_U23;
        ps2 = 0; pn2 = 4;
        setValue = iftermCfg;
        break;
    case 5:
        cid2 = PCA_I2C_ADDR_U23;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 6:
        cid2 = PCA_I2C_ADDR_U36;
        ps2 = 0; pn2 = 3;
        setValue = iftermCfg;
        break;
    case 7:
        cid2 = PCA_I2C_ADDR_U36;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 8:
        cid2 = PCA_I2C_ADDR_U49;
        ps2 = 0; pn2 = 4;
        setValue = iftermCfg;
        break;
    default :
        return;
    }

    pca9555_iepe_pin_set(dev, cid2, ps2, pn2, setValue);
}
void RSE_close_7023(void* dev, int num)
{
    uint8_t ps2, pn2, cid2;

    switch (num) {
    case 1:
        cid2 = PCA_I2C_ADDR_U6;
        ps2 = 1; pn2 = 7;
        break;
    case 2:
        cid2 = PCA_I2C_ADDR_U6;
        ps2 = 0; pn2 = 3;
        break;
    case 3:
        cid2 = PCA_I2C_ADDR_U6;
        ps2 = 1; pn2 = 2;
        break;
    case 4:
        cid2 = PCA_I2C_ADDR_U23;
        ps2 = 0; pn2 = 4;
        break;
    case 5:
        cid2 = PCA_I2C_ADDR_U23;
        ps2 = 1; pn2 = 2;
        break;
    case 6:
        cid2 = PCA_I2C_ADDR_U36;
        ps2 = 0; pn2 = 3;
        break;
    case 7:
        cid2 = PCA_I2C_ADDR_U36;
        ps2 = 1; pn2 = 2;
        break;
    case 8:
        cid2 = PCA_I2C_ADDR_U49;
        ps2 = 0; pn2 = 4;
        break;
    default :
        return;
    }

    pca9555_pin_clr(dev, cid2, ps2, pn2);
}
void icp2_set_7023(void* dev, int num, int excit_src, int iftermCfg)
{
    uint8_t ps1, pn1, ps2, pn2, cid1, cid2;
    int setValue;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR2_U6;
        ps1 = 0; pn1 = 0;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR2_U6;
        ps2 = 1; pn2 = 7;
        setValue = iftermCfg;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR2_U6;
        ps1 = 0; pn1 = 6;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR2_U6;
        ps2 = 0; pn2 = 3;
        setValue = iftermCfg;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR2_U23;
        ps1 = 0; pn1 = 1;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR2_U6;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR2_U23;
        ps1 = 0; pn1 = 7;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR2_U23;
        ps2 = 0; pn2 = 4;
        setValue = iftermCfg;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR2_U36;
        ps1 = 0; pn1 = 0;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR2_U23;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR2_U36;
        ps1 = 0; pn1 = 6;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR2_U36;
        ps2 = 0; pn2 = 3;
        setValue = iftermCfg;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR2_U49;
        ps1 = 0; pn1 = 1;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR2_U36;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR2_U49;
        ps1 = 1; pn1 = 4;
        setValue = excit_src;
        cid2 = PCA_I2C_ADDR2_U49;
        ps2 = 0; pn2 = 4;
        setValue = iftermCfg;
        break;
    default :
        return;
    }

    pca9555_iepe_pin_set(dev, cid1, ps1, pn1, setValue);
    pca9555_iepe_pin_set(dev, cid2, ps2, pn2, setValue);
}
void icp2_close_7023(void* dev, int num)
{
    uint8_t ps1, pn1, ps2, pn2, cid1, cid2;

    switch (num) {
    case 1:
        cid1 = PCA_I2C_ADDR2_U6;
        ps1 = 0; pn1 = 0;
        cid2 = PCA_I2C_ADDR2_U6;
        ps2 = 1; pn2 = 7;
        break;
    case 2:
        cid1 = PCA_I2C_ADDR2_U6;
        ps1 = 0; pn1 = 6;
        cid2 = PCA_I2C_ADDR2_U6;
        ps2 = 0; pn2 = 3;
        break;
    case 3:
        cid1 = PCA_I2C_ADDR2_U23;
        ps1 = 0; pn1 = 1;
        cid2 = PCA_I2C_ADDR2_U6;
        ps2 = 1; pn2 = 2;
        break;
    case 4:
        cid1 = PCA_I2C_ADDR2_U23;
        ps1 = 0; pn1 = 7;
        cid2 = PCA_I2C_ADDR2_U23;
        ps2 = 0; pn2 = 4;
        break;
    case 5:
        cid1 = PCA_I2C_ADDR2_U36;
        ps1 = 0; pn1 = 0;
        cid2 = PCA_I2C_ADDR2_U23;
        ps2 = 1; pn2 = 2;
        break;
    case 6:
        cid1 = PCA_I2C_ADDR2_U36;
        ps1 = 0; pn1 = 6;
        cid2 = PCA_I2C_ADDR2_U36;
        ps2 = 0; pn2 = 3;
        break;
    case 7:
        cid1 = PCA_I2C_ADDR2_U49;
        ps1 = 0; pn1 = 1;
        cid2 = PCA_I2C_ADDR2_U36;
        ps2 = 1; pn2 = 2;
        break;
    case 8:
        cid1 = PCA_I2C_ADDR2_U49;
        ps1 = 1; pn1 = 4;
        cid2 = PCA_I2C_ADDR2_U49;
        ps2 = 0; pn2 = 4;
        break;
    default :
        return;
    }

    pca9555_pin_clr(dev, cid1, ps1, pn1);
    pca9555_pin_clr(dev, cid2, ps2, pn2);
}
void RSE2_set_7023(void* dev, int num, int iftermCfg)
{
    uint8_t ps2, pn2, cid2;
    int setValue;

    switch (num) {
    case 1:
        cid2 = PCA_I2C_ADDR2_U6;
        ps2 = 1; pn2 = 7;
        setValue = iftermCfg;
        break;
    case 2:
        cid2 = PCA_I2C_ADDR2_U6;
        ps2 = 0; pn2 = 3;
        setValue = iftermCfg;
        break;
    case 3:
        cid2 = PCA_I2C_ADDR2_U6;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 4:
        cid2 = PCA_I2C_ADDR2_U23;
        ps2 = 0; pn2 = 4;
        setValue = iftermCfg;
        break;
    case 5:
        cid2 = PCA_I2C_ADDR2_U23;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 6:
        cid2 = PCA_I2C_ADDR2_U36;
        ps2 = 0; pn2 = 3;
        setValue = iftermCfg;
        break;
    case 7:
        cid2 = PCA_I2C_ADDR2_U36;
        ps2 = 1; pn2 = 2;
        setValue = iftermCfg;
        break;
    case 8:
        cid2 = PCA_I2C_ADDR2_U49;
        ps2 = 0; pn2 = 4;
        setValue = iftermCfg;
        break;
    default :
        return;
    }

    pca9555_iepe_pin_set(dev, cid2, ps2, pn2, setValue);
}
void RSE2_close_7023(void* dev, int num)
{
    uint8_t ps2, pn2, cid2;

    switch (num) {
    case 1:
        cid2 = PCA_I2C_ADDR2_U6;
        ps2 = 1; pn2 = 7;
        break;
    case 2:
        cid2 = PCA_I2C_ADDR2_U6;
        ps2 = 0; pn2 = 3;
        break;
    case 3:
        cid2 = PCA_I2C_ADDR2_U6;
        ps2 = 1; pn2 = 2;
        break;
    case 4:
        cid2 = PCA_I2C_ADDR2_U23;
        ps2 = 0; pn2 = 4;
        break;
    case 5:
        cid2 = PCA_I2C_ADDR2_U23;
        ps2 = 1; pn2 = 2;
        break;
    case 6:
        cid2 = PCA_I2C_ADDR2_U36;
        ps2 = 0; pn2 = 3;
        break;
    case 7:
        cid2 = PCA_I2C_ADDR2_U36;
        ps2 = 1; pn2 = 2;
        break;
    case 8:
        cid2 = PCA_I2C_ADDR2_U49;
        ps2 = 0; pn2 = 4;
        break;
    default :
        return;
    }

    pca9555_pin_clr(dev, cid2, ps2, pn2);
}
void pxie_7023_CfgReset(struct pxi_card_t *p)
{
    void *user;

    if (p == NULL)
        return;
    user = p->user_base;

    ad4134_deinit(user);
    //printf("Reset ad4134_deinit in slot %d\n", p->phy_slot);

    for (int i = 0; i < 8; i++) {
        icp_close_7023(user, i + 1);
        RSE_close_7023(user, i + 1);
        icp2_close_7023(user, i + 1);
        RSE2_close_7023(user, i + 1);
    }

    munmap(p->user_base, 1024*1024UL);
    //printf("Reset PXI-7023 Configure in slot %d\n", p->phy_slot);
}

void pxie_7023_deinit(struct pxi_card_t *p)
{
    void *user;
    if (p == NULL)
        return;
    if (p->h_dma >= 0)
        close(p->h_dma);
    user = p->user_base;
    ad4134_deinit(user);
    for (int i = 0; i < 8; i++)
    {
        icp_close_7023(user, i + 1);
        RSE_close_7023(user, i + 1);
        icp2_close_7023(user, i + 1);
        RSE2_close_7023(user, i + 1);
    }
}

int pxie_7023_CfgSet(struct pxi_card_t *p, int excit_src, int coupl_way, int termCfg, int gain)
{
    void *user_h;

    if (p == NULL)
        return -1;

    user_h = p->user_base;

    pca9555_init(user_h, PCA_I2C_ADDR_U23);
    pca9555_init(user_h, PCA_I2C_ADDR_U6);
    pca9555_init(user_h, PCA_I2C_ADDR_U36);
    pca9555_init(user_h, PCA_I2C_ADDR_U49);
    pca9555_init(user_h, PCA_I2C_ADDR2_U23);
    pca9555_init(user_h, PCA_I2C_ADDR2_U6);
    pca9555_init(user_h, PCA_I2C_ADDR2_U36);
    pca9555_init(user_h, PCA_I2C_ADDR2_U49);

    //printf("------>in slot %d excit_src %d, iftermCfg %d, coupl_way %d\n", p->phy_slot,excit_src, termCfg, coupl_way);
    for (int i = 0; i < 8; i++) {
        if(coupl_way == 1){
            chann_close_7023(user_h, i + 1);
            chann2_close_7023(user_h, i + 1);
        }else{
            chann_open_7023(user_h, i + 1);
            chann2_open_7023(user_h, i + 1);
        }
        if(gain == 2){
            gain_set(user_h, i + 1, 1, 1);
            gain2_set(user_h, i + 1, 1, 1);
        }else if(gain == 10){
            gain_set(user_h, i + 1, 0, 1);
            gain2_set(user_h, i + 1, 0, 1);
        }else{
            gain_set(user_h, i + 1, 0, 0);  //  A0:A1 : 00 -> 1, 10 -> 2, 01 -> 3, 11 -> 4
            gain2_set(user_h, i + 1, 0, 0);  // 00, 10-> double the gain; 01 -> ten times the gain;  11 -> double gain
        }
        if(excit_src == 1){
            if(termCfg != 1){
                printf("---------------------> ERROR TERMCFG SET!!! <------------------------\n");
                return -1;
            }else{
                icp_set_7023(user_h, i + 1, 1, 1);
                icp2_set_7023(user_h, i + 1, 1, 1);
            }
            if(coupl_way == 1){
                printf("--------------------->SET DC Couplung, ERROR AI Coupling SETTING!!! <------------------------\n");
                return -1;
            }
        }
        if(termCfg == 1 && excit_src != 1){
            RSE_set_7023(user_h, i + 1, 1);
            RSE2_set_7023(user_h, i + 1, 1);
        }
    }
//printf("-->in slot %d set cfg.\n", p->phy_slot);
    return 0;
}
void pxie_7023_odrtrig(struct pxi_card_t *p)
{
    void * user_h;
    if (p == NULL)
        return;

    user_h = p->user_base;
    odr_trigger(user_h);
    //printf("\n\n-->ODR TRIGGER.\n\n\n");

}
void pxie_7023_odrtrig_o(struct pxi_card_t *p)
{
    void * user_h;
    if (p == NULL)
        return;

    user_h = p->user_base;
    odr_trigger_o(user_h);
}
void pxie_7023_sync(struct pxi_card_t *p)
{
    void * user_h;
    if (p == NULL)
        return;

    user_h = p->user_base;
    sync_start(user_h);
}
void pxie_7023_syncIN(struct pxi_card_t *p)
{
    void * user_h;
    if (p == NULL)
        return;

    user_h = p->user_base;
    sync_input(user_h);
}
void pxie_7023_deinit_tmp(struct pxi_card_t *p)
{
    void * user;

    if (p == NULL)
        return;
    user = p->user_base;

    if (p->h_dma >= 0)
        close(p->h_dma);
}

int pxie_7023_init(struct pxi_card_t *p, int samplingRate, int* cfg)
{
    void * user_h;
    int ret = 0, initret = 0;

    if (p == NULL)
    {
        printf("7023 p==NULL!\n");
        return -1;
    }

    user_h = p->user_base;




    pca9555_init(user_h, PCA_I2C_ADDR_U23);
    pca9555_init(user_h, PCA_I2C_ADDR_U6);
    pca9555_init(user_h, PCA_I2C_ADDR_U36);
    pca9555_init(user_h, PCA_I2C_ADDR_U49);
    pca9555_init(user_h, PCA_I2C_ADDR2_U23);
    pca9555_init(user_h, PCA_I2C_ADDR2_U6);
    pca9555_init(user_h, PCA_I2C_ADDR2_U36);
    pca9555_init(user_h, PCA_I2C_ADDR2_U49);

    int coupl_way=0;//1 DC 0AC 0是ICP
    int gain=0;//0 1bei 2 10bei(暂时0）
    int excit_src=0;//0正常采集 1是icp
    int termCfg=1;//1单端 0插分

    for (int i = 0; i <16; i++)
    {
        switch (cfg[i]) {
        case 1:  //ICP
            coupl_way=0;
            excit_src=1;
            termCfg=1;
            printf("icp\n");
            break;
        case 2: //AC单端
            coupl_way=0;
            excit_src=0;
            termCfg=1;
            printf("ac\n");
            break;
        case 3: //AC差分
            coupl_way=0;
            excit_src=0;
            termCfg=0;
            printf("ac\n");
            break;
        case 4: //DC单端
            coupl_way=1;
            excit_src=0;
            termCfg=1;
            printf("dc\n");
            break;
        case 5: //DC差分
            coupl_way=1;
            excit_src=0;
            termCfg=0;
            printf("dc\n");
            break;
        default :
            printf("unknow cfg !\n");
            return -2;
        }

        printf("------>in slot %d channel %d excit_src %d, iftermCfg %d, coupl_way %d\n", p->phy_slot, i+1,excit_src, termCfg, coupl_way);
        if(coupl_way == 1)
        {
            chann_close_7023(user_h, i + 1);
            chann2_close_7023(user_h, i + 1);
        }else{
            chann_open_7023(user_h, i + 1);
            chann2_open_7023(user_h, i + 1);
        }
        if(gain == 2){
            gain_set(user_h, i + 1, 1, 1);
            gain2_set(user_h, i + 1, 1, 1);
        }else if(gain == 10){
            gain_set(user_h, i + 1, 0, 1);
            gain2_set(user_h, i + 1, 0, 1);
        }else{
            gain_set(user_h, i + 1, 0, 0);  //  A0:A1 : 00 -> 1, 10 -> 2, 01 -> 3, 11 -> 4
            gain2_set(user_h, i + 1, 0, 0);  // 00, 10-> double the gain; 01 -> ten times the gain;  11 -> double gain
        }
        if(excit_src == 1)//ICP
        {
            if(termCfg != 1){
                printf("---------------------> ERROR TERMCFG SET!!! <------------------------\n");
                return -1;
            }else //ICP 固定单端
            {
                icp_set_7023(user_h, i + 1, 1, 1);
                icp2_set_7023(user_h, i + 1, 1, 1);
            }
            if(coupl_way == 1)
            {
                printf("--------------------->SET DC Couplung, ERROR AI Coupling SETTING!!! <------------------------\n");
                return -1;
            }
        }
        if(termCfg == 1 && excit_src != 1)
        {
            RSE_set_7023(user_h, i + 1, 1);
            RSE2_set_7023(user_h, i + 1, 1);
        }
    }


    for(int i = 0; i < 4; i++)
    {
        int j = 0;
        for(j = 0; j < 100; j++)
        {
         ret = ad4134_init(user_h, i);
         if(ret == 0 )
         {
             break;
         }
        }
        if(j == 100){
            printf("card adc PLL fauiled in slot %d\n", p->phy_slot);
            initret = -1;
        }
    }
    fifo_clear(user_h);
    int h_dma;
    char dma_name[256];
    sprintf(dma_name, "/dev/xdma%d_c2h_0", p->card_num);
    h_dma = open(dma_name, O_RDWR);
    if (h_dma < 0)
        return -1;
    p->h_dma = h_dma;
    return initret;
}
int pxie_7023_read(struct pxi_card_t *p, void *buf, size_t size)
{
    if (p == NULL)
        return -1;
    return read(p->h_dma, buf, size);
}


int pxie_7023_write(struct pxi_card_t *p, void *buf, size_t size)
{
    return 0;
}



