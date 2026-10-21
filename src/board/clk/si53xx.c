#include "si53xx.h"
//#include "../../xdma/xdma.h"
//#include "../../xilinx/iic/xiic.h"
//#include "../../xilinx/iic/xiic_l.h"
//#include "../../xilinx/iic/xparameters.h"

#include <stdio.h>
#include <unistd.h>
#include <math.h>
//#include "usrDAQmx.h"
#include "freq.h"

//#define SI53XX_I2C_BASEADDR 0x00030000  // AD/DA I2C Address
#define SI53XX_7BIT_ADDR    0x68

#define SI53XX_GPO_RESET_MASK  0x00000001
#define CLK_FIN 10000000

int SI53XX_I2C_BASEADDR;
//#define SI53XX_GPO_CMODE_MASK  0x00000002
//#define SI53XX_GPO_CS_MASK     0x00000004
//#define SI53XX_GPO_A0_MASK     0x00000008
//#define SI53XX_GPO_A1_MASK     0x00000010
//#define SI53XX_GPO_A2_MASK     0x00000020
//#define SI53XX_GPO_SDI_MASK    0x00000040
uint64_t gcd(uint64_t a,uint64_t c)
{
    while(c!=0)
    {
        uint64_t t=a%c;
        a=c;
        c=t;
    }
    return a;
}

void si53xx_enable(uintptr_t base)
{
    XIic_ResetM(base + SI53XX_I2C_BASEADDR);
    usleep(1000);

    XIic_SetGpo(base + SI53XX_I2C_BASEADDR, SI53XX_GPO_RESET_MASK);
}

void si53xx_disable(uintptr_t base)
{
    XIic_SetGpo(base + SI53XX_I2C_BASEADDR, 0x0);
}

uint8_t si53xx_read(uintptr_t base, uint8_t addr)
{
    int ByteCount;
    uint8_t data;

    XIic_Send(base + SI53XX_I2C_BASEADDR, SI53XX_7BIT_ADDR, &addr, 1, XIIC_STOP);
    ByteCount = XIic_Recv(base + SI53XX_I2C_BASEADDR, SI53XX_7BIT_ADDR, &data, 1, XIIC_STOP);

    if(ByteCount != 1)
        return -1;

    return data;
}

int si53xx_write(uintptr_t base, uint8_t addr, uint8_t data)
{
    int ByteCount;
    uint8_t sendBuf[2];

    sendBuf[0] = addr;
    sendBuf[1] = data;

    ByteCount = XIic_Send(base + SI53XX_I2C_BASEADDR, SI53XX_7BIT_ADDR, sendBuf, 2, XIIC_STOP);
    if(ByteCount != 2)
        return -1;

    return 0;
}

struct n_para{
    uint64_t N1_HS;
    uint64_t NC1_LS;
    uint64_t N2_HS;
    uint64_t N2_LS;
};
int si53_n1n2(uint64_t N1, uint64_t N2, uint64_t re, struct n_para *npara){
    uint32_t N_HS;
    uint64_t N1_HS,NC1_LS,N2_HS, N2_LS;

    N2 = N2*re;
    N1 = N1*re;

    for( N_HS = 4; N_HS < 12; N_HS++){
        if((N1%N_HS == 0) && ((N1/N_HS)%2==0)){
            N1_HS = N_HS;
            NC1_LS = (N1/N_HS);
            break;
        }
    }
    if(N_HS == 12){
        printf("N1_HS failed\n");
        return -1;
    }
    for(N_HS= 4; N_HS < 12; N_HS++){
        if((N2%N_HS == 0) && ((N2/N_HS)%2==0)){
            N2_HS = N_HS;
            N2_LS = (N2/N_HS);
            break;
        }
    }
    if(N_HS == 12){
        printf("N2_HS failed\n");
        return -1;
    }

    npara->N1_HS = N1_HS;
    npara->N2_HS = N2_HS;
    npara->N2_LS = N2_LS;
    npara->NC1_LS = NC1_LS;

    return 0;
}
int si53freqcal(uint32_t fin, uint32_t fout, struct reg_data *reg_data){
    uint64_t N1,N2, rgcd, fosc = 0, N1s;
    uint64_t re = 0;
    uint64_t N1_HS,NC1_LS,N2_HS, N2_LS;
    u8 coef2,coef4;
    u32 coef1, coef5,coef3;
    struct n_para npara;
    uint64_t n3;

    for(n3 = 5; n3 < 5001; n3++){ //n3 = 5; n3 < 2001; n3++
        if(n3 == 5000){
            printf("fout %lld, n2 %lld \n",fout, N2);
        }
        rgcd = gcd((uint64_t)((uint64_t)fout*n3), (uint64_t)fin);

        N2 = fout*n3/rgcd;
        N1 = fin/rgcd;

        for(re=1; re<5000; re++){
            N1s = N1;
            fosc = N1s * re * fout;
            if(fosc > 4850000000 && fosc < 5670000000) {

                printf("n3 %d, rgcd %lld, N2 %lld, N1 %lld, re %lld, fosc %lld\n", n3, rgcd, N2, N1, re, fosc);
                if(si53_n1n2(N1,N2, re,&npara) == 0){
                    break;
                }
            }
        }
        if(re == 5000){
            continue;
        }
        //printf("n3 %d, rgcd %lld, N2 %lld, N1 %lld, re %lld, fosc %lld\n", n3, rgcd, N2, N1, re, fosc);
        break;
    }
    if(n3 == 5001){
        return -1;
    }
    N1_HS = npara.N1_HS;
    N2_HS =npara.N2_HS;
    N2_LS = npara.N2_LS ;
    NC1_LS = npara.NC1_LS;
    printf("N1_HS %lld, NC1_LS %lld, N2_HS %lld, N2_LS %lld, N3 %lld\n", N1_HS, NC1_LS, N2_HS, N2_LS, n3);

    coef1 = NC1_LS - 1;
    coef2 = N1_HS - 4;
    coef3 = N2_LS - 1;
    coef4 = N2_HS - 4;
    coef5 = n3 - 1;
    //printf("coef1 0x%x , coef2 0x%x,coef3 0x%4x , coef4 0x%x,coef5 0x%x \n", coef1, coef2,coef3, coef4, coef5);

    reg_data->reg_27 =  (coef2 * 32)& 0xFF;
    reg_data->reg_29 = (coef1 >>16)&0xFF;
    reg_data->reg_31 = (coef1 >>8)&0xFF;
    reg_data->reg_33 = coef1& 0xFF;
    reg_data->reg_35 = ((coef3 >>16)&0xFF) + ((coef4 * 32)& 0xFF);
    reg_data->reg_37 = (coef3 >>8)&0xFF;
    reg_data->reg_39 = coef3&0xFF;
    reg_data->reg_41 = (coef5 >>16)&0xFF;
    reg_data->reg_43 = (coef5 >>8)&0xFF;
    reg_data->reg_45 = coef5& 0xFF;

    return 0;

}
int si53xx_initR(void * dev, uint32_t fout, uint32_t select_ad) //void *base_addr
{
    u8 addr, data;
    u8 reg_map_usr[62];
    u8 *reg_tab;
    uintptr_t base = (uintptr_t)dev;
    struct reg_data reg_usr;

    for (int i = 0; i < 62; i = i + 1) {
        reg_map_usr[i] = reg_map_26_2144ADC[i];
    }

    if(select_ad == 0){
        reg_map_usr[9] = 0x2A;
        SI53XX_I2C_BASEADDR = 0x00030000;
        printf("si53 output clk set CMOS\n");
    }
    else
        SI53XX_I2C_BASEADDR = 0x00030000;

    printf("si53 fout %lld\n", fout);

    int ret = si53freqcal(CLK_FIN, fout, &reg_usr);
    if(ret < 0){
        printf("PLL failed, input frequence failure\n");
        return -1;
    }

    reg_map_usr[3] = 0xA2;
    reg_map_usr[27] = reg_usr.reg_27;
    //printf("reg_map_usr[26] 0x%x \n", reg_map_usr[27]);
    reg_map_usr[29] = reg_usr.reg_29;
    //printf("reg_map_usr[29] 0x%x \n", reg_map_usr[29]);
    reg_map_usr[31] = reg_usr.reg_31;
    //printf("reg_map_usr[31] 0x%x \n", reg_map_usr[31]);
    reg_map_usr[33] = reg_usr.reg_33;
    //printf("reg_map_usr[32] 0x%x \n", reg_map_usr[33]);
    reg_map_usr[35] = reg_usr.reg_35;
    //printf("reg_map_usr[32] 0x%x \n", reg_map_usr[35]);
    reg_map_usr[37] = reg_usr.reg_37;
    //printf("reg_map_usr[32] 0x%x \n", reg_map_usr[37]);
    reg_map_usr[39] = reg_usr.reg_39;
    //printf("reg_map_usr[32] 0x%x \n", reg_map_usr[39]);
    reg_map_usr[41] = reg_usr.reg_41;
    reg_map_usr[43] = reg_usr.reg_43;
    reg_map_usr[45] = reg_usr.reg_45;
    //printf("reg_map_usr[32] 0x%x \n", reg_map_usr[45]);

//    for(int i = 0; i < 62; i = i + 2){
//        printf("%d, 0x%x \n", reg_map_usr[i], reg_map_usr[i+1]);
//    }

    reg_tab = reg_map_usr;
    //reg_tab =reg_map_26_2144ADC;

    si53xx_enable(base);
    usleep(1000);

    for(int i = 0; i < 62; i = i + 2) {
        addr = reg_tab[i];
        data = reg_tab[i+1];

        si53xx_write(base, addr, data);
    }

    /* wait for PLL lock */
    usleep(400000);

    addr = 130;
    data = si53xx_read(base, addr);
    if(data != 0){
        printf("SI53xx PLL failed! %u\n", data);
    }
    else
        printf("SI53xx PLL locked! %u\n", data);

    return 0;
}

void si53xx_term(void *base_addr)
{
    uintptr_t base = (uintptr_t)base_addr;
    si53xx_disable(base);
}
