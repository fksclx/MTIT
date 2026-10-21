#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include "ad4134.h"
#include "../../xilinx/gpio/xgpio.h"
#include "../../xilinx/gpio/xgpio_I.h"
//#include "../../xilinx/comm/xparameters.h"
#include "../../xilinx/spi/xspi.h"
#define AD4134_GPO_RESET_MASk   0x0001
#define AD4134_GPO_START_MASk   0x0002
#define FREQ_GPO_BASEADDR     0x00050000
#define TLAST_GPO_BASEADDR     0x00040000
#define FIR_1024_BASEADDR      0x000C0000
#define AD4134_GPO_BASEADDR      0x00010000
#define ODRTRIG_GPO_BASEADDR     0x00060000
#define TRI_IO_BASEADDR      0x000A0000
#define ADC_ODR_STATUS 0x000E0000
#define REG_SIZE 4
void freqfir_select_1024(void *base_addr, int regVal) {
    uintptr_t base = (uintptr_t)base_addr;

    XGpio_WriteReg(base + FIR_1024_BASEADDR, XGPIO_TRI_OFFSET ,0x00);
    XGpio_WriteReg(base + FIR_1024_BASEADDR, XGPIO_DATA_OFFSET, regVal);
    printf("XGpio_WriteReg %d\n",  XGpio_ReadReg(base + FIR_1024_BASEADDR, XGPIO_DATA_OFFSET));
}
void adcODR_x16_status(void *base_addr, int regVal) {
    uintptr_t base = (uintptr_t)base_addr;

    XGpio_WriteReg(base + ADC_ODR_STATUS, XGPIO_TRI_OFFSET ,0x00);
    XGpio_WriteReg(base + ADC_ODR_STATUS, XGPIO_DATA_OFFSET, regVal);
    printf("adcODR_x16_status_WriteReg %d\n",  XGpio_ReadReg(base + ADC_ODR_STATUS, XGPIO_DATA_OFFSET));


}
static const int ad713x_output_data_frame[4][9][2] = {
    {
        {ADC_16_BIT_DATA, CRC_6},
        {ADC_24_BIT_DATA, CRC_6},
        {ADC_32_BIT_DATA, NO_CRC},
        {ADC_32_BIT_DATA, CRC_6},
        {ADC_16_BIT_DATA, NO_CRC},
        {ADC_24_BIT_DATA, NO_CRC},
        {ADC_24_BIT_DATA, CRC_8},
        {ADC_32_BIT_DATA, CRC_8},
        {INVALID}
    },
    {
        {ADC_16_BIT_DATA, NO_CRC},
        {ADC_16_BIT_DATA, CRC_6},
        {ADC_24_BIT_DATA, NO_CRC},
        {ADC_24_BIT_DATA, CRC_6},
        {ADC_16_BIT_DATA, CRC_8},
        {ADC_24_BIT_DATA, CRC_8},
        {INVALID}
    },
    {
        {ADC_16_BIT_DATA, NO_CRC},
        {ADC_16_BIT_DATA, CRC_6},
        {ADC_16_BIT_DATA, CRC_8},
        {INVALID}
    },
    {
        {ADC_16_BIT_DATA, NO_CRC},
        {ADC_16_BIT_DATA, CRC_6},
        {ADC_24_BIT_DATA, NO_CRC},
        {ADC_24_BIT_DATA, CRC_6},
        {ADC_16_BIT_DATA, CRC_8},
        {ADC_24_BIT_DATA, CRC_8},
        {INVALID}
    },
};


static u16 ad4134_spi_trans(uintptr_t base, u16 buf, int i){
    u32 Control;
    u16 Buffer;
    long AD4134_SPI_BASEADDR;

    Buffer = buf;

    if(i == 0){
        AD4134_SPI_BASEADDR = 0x00020000;
    }else if(i == 1){
        AD4134_SPI_BASEADDR = 0x00070000;
    }else if(i == 2){
        AD4134_SPI_BASEADDR = 0x00080000;
    }else if(i == 3){
        AD4134_SPI_BASEADDR = 0x00090000;
    }

    XSpi_WriteReg(base + AD4134_SPI_BASEADDR , XSP_SRR_OFFSET, XSP_SRR_RESET_MASK);
    XSpi_WriteReg(base + AD4134_SPI_BASEADDR , XSP_SSR_OFFSET, 0x01);

    /*
         * Set up the device in enable master mode.
         */
    Control = XSpi_ReadReg(base + AD4134_SPI_BASEADDR , XSP_CR_OFFSET);
    Control |= (XSP_CR_MASTER_MODE_MASK);
    XSpi_WriteReg(base + AD4134_SPI_BASEADDR , XSP_CR_OFFSET, Control);

    XSpi_WriteReg(base + AD4134_SPI_BASEADDR , XSP_SSR_OFFSET, 0x00);

    /*
         * Fill up the transmitter with data, assuming the receiver can hold
         * the same amount of data.
         */
//    struct timeval start, end;
//    gettimeofday(&start, NULL);
    while ((XSpi_ReadReg(base + AD4134_SPI_BASEADDR , XSP_SR_OFFSET) &
            XSP_SR_TX_FULL_MASK) == 0) {
        //usleep(1000);
        XSpi_WriteReg(base + AD4134_SPI_BASEADDR , XSP_DTR_OFFSET, Buffer);
    }
//    gettimeofday(&end, NULL);
//    printf("while loop1 time ---sec: %ld, ---usec: %ld\n", end.tv_sec-start.tv_sec, end.tv_usec-start.tv_usec);
    /*
         * Enable the device.
         */
    Control = XSpi_ReadReg(base + AD4134_SPI_BASEADDR , XSP_CR_OFFSET);
    Control |= XSP_CR_ENABLE_MASK;
    Control &= ~XSP_CR_TRANS_INHIBIT_MASK;
    XSpi_WriteReg(base + AD4134_SPI_BASEADDR , XSP_CR_OFFSET, Control);

    /*
         * Initialize the buffer with zeroes so that it can be used to receive
         * data.
         */
    Buffer = 0;

    /*
         * Wait for the transmit FIFO to transition to empty before checking
         * the receive FIFO, this prevents a fast processor from seeing the
         * receive FIFO as empty
         */
//    struct timeval start2, end2;
//    gettimeofday(&start2, NULL);
    while (!(XSpi_ReadReg(base +AD4134_SPI_BASEADDR , XSP_SR_OFFSET) &
             XSP_SR_TX_EMPTY_MASK)){
        //usleep(1000);
    };
//    gettimeofday(&end2, NULL);
//    printf("while loop2 time ---sec: %ld, ---usec: %ld\n", end2.tv_sec-start2.tv_sec, end2.tv_usec-start2.tv_usec);
    /*
         * Transmitter is full, now receive the data just looped back until
         * the receiver is empty.
         */
//    struct timeval start3, end3;
//    gettimeofday(&start3, NULL);
    while ((XSpi_ReadReg(base + AD4134_SPI_BASEADDR , XSP_SR_OFFSET) &
            XSP_SR_RX_EMPTY_MASK) == 0) {
        //usleep(1000);
        Buffer = XSpi_ReadReg(base + AD4134_SPI_BASEADDR , XSP_DRR_OFFSET);
    }
//    gettimeofday(&end3, NULL);
//    printf("while loop3 time ---sec: %ld, ---usec: %ld\n", end3.tv_sec-start3.tv_sec, end3.tv_usec-start3.tv_usec);
    XSpi_WriteReg(base + AD4134_SPI_BASEADDR , XSP_SSR_OFFSET, 0x01);

    return Buffer;
}

u8 ad4134_reg_read(uintptr_t base, u8 addr, int i)
{
    u16 buf;

    addr |= 0x80;
    buf = (addr << 8) & 0xFF00;

    ad4134_spi_trans(base, buf, i);

    return ad4134_spi_trans(base, buf, i);
}

void ad4134_reg_write(uintptr_t base, u8 addr, u8 data, int i)
{
    u16 buf;

    buf = (addr << 8) & 0xFF00;
    buf = buf | data;

    ad4134_spi_trans(base, buf, i);
}


#define NO_OS_IS_ERR_VALUE(X) X
/******************************************************************************/
/************************** Functions Implementation **************************/
/******************************************************************************/


int32_t ad713x_spi_reg_read(uintptr_t base,
                            uint8_t reg_addr,
                            uint8_t *mask, int i)
{

    *mask = ad4134_reg_read(base, reg_addr, i);

    return 0;
}


int32_t ad713x_spi_reg_write(uintptr_t base,
                             uint8_t reg_addr,
                             uint8_t reg_data, int i)
{
    ad4134_reg_write(base, reg_addr, reg_data, i);
    return 0;
}


int32_t ad713x_spi_write_mask(uintptr_t base,
                              uint8_t reg_addr,
                              uint32_t mask,
                              uint8_t data, int i)
{
    uint8_t reg_data;
    int32_t ret;

    ret = ad713x_spi_reg_read(base, reg_addr, &reg_data, i);
    if(NO_OS_IS_ERR_VALUE(ret))
        return -1;
    reg_data &= ~mask;
    reg_data |= data;

    return ad713x_spi_reg_write(base, reg_addr, reg_data, i);
}

int32_t ad713x_set_power_mode(uintptr_t base,
                              enum ad713x_power_mode mode, int i)
{
    if (mode == LOW_POWER)
        return ad713x_spi_write_mask(base, AD713X_REG_DEVICE_CONFIG,
                                     AD713X_DEV_CONFIG_PWR_MODE_MSK, 0, i);
    else if (mode == HIGH_POWER)
        return ad713x_spi_write_mask(base, AD713X_REG_DEVICE_CONFIG,
                                     AD713X_DEV_CONFIG_PWR_MODE_MSK,
                                     1, i);

    return -1;
}


int32_t ad713x_set_out_data_frame(uintptr_t base,
                                  enum ad713x_adc_data_len adc_data_len,
                                  enum ad713x_crc_header crc_header, int i)
{
    uint8_t id;
    uint8_t ic = 0;

    id = 1;

    while (ad713x_output_data_frame[id][ic][0] != INVALID) {
        if((adc_data_len == ad713x_output_data_frame[id][ic][0]) &&
                (crc_header == ad713x_output_data_frame[id][ic][1])) {
            return ad713x_spi_write_mask(base,
                                         AD713X_REG_DATA_PACKET_CONFIG,
                                         AD713X_DATA_PACKET_CONFIG_FRAME_MSK,
                                         AD713X_DATA_PACKET_CONFIG_FRAME_MODE(ic), i);
        }
        ic++;
    }

    return -1;
}


int32_t ad713x_dout_format_config(uintptr_t base,
                                  enum ad713x_doutx_format format, int i)
{
    return ad713x_spi_write_mask(base, AD713X_REG_DIGITAL_INTERFACE_CONFIG,
                                 AD713X_DIG_INT_CONFIG_FORMAT_MSK,
                                 AD713X_DIG_INT_CONFIG_FORMAT_MODE(format), i);
}


int32_t ad713x_mag_phase_clk_delay(uintptr_t base,
                                   bool clk_delay_en, int i)
{
    int32_t ret;
    int8_t ic;
    int8_t temp_clk_delay;

    if (clk_delay_en)
        temp_clk_delay = DELAY_2_CLOCKS;
    else
        temp_clk_delay = DELAY_NONE;

    for (ic = CH3; ic >= 0; ic--) {
        ret = ad713x_spi_write_mask(base, AD713X_REG_MPC_CONFIG,
                                    AD713X_MPC_CLKDEL_EN_CH_MSK(ic),
                                    AD713X_MPC_CLKDEL_EN_CH_MODE(temp_clk_delay, ic), i);
        if (NO_OS_IS_ERR_VALUE(ret))
            return -1;
    }

    return 0;
}

int32_t ad713x_mag_phase_clk_delay_chan(uintptr_t base,
                                        enum ad713x_channels chan,
                                        enum ad717x_mpc_clkdel mode, int i)
{
    return ad713x_spi_write_mask(base, AD713X_REG_MPC_CONFIG,
                                 AD713X_MPC_CLKDEL_EN_CH_MSK(chan),
                                 AD713X_MPC_CLKDEL_EN_CH_MODE(mode, chan), i);
}

int32_t ad713x_dig_filter_sel_ch(uintptr_t base,
                                 enum ad713x_dig_filter_sel filter,
                                 enum ad713x_channels ch, int i)
{
    return ad713x_spi_write_mask(base, AD713X_REG_CHAN_DIG_FILTER_SEL,
                                 AD713X_DIGFILTER_SEL_CH_MSK(ch),
                                 AD713X_DIGFILTER_SEL_CH_MODE(filter, ch), i);
}

int32_t ad713x_clkout_output_en(uintptr_t base, bool enable, int i)
{
    return ad713x_spi_write_mask(base, AD713X_REG_DEVICE_CONFIG1,
                                 AD713X_DEV_CONFIG1_CLKOUT_EN_MSK,
                                 enable ? AD713X_DEV_CONFIG1_CLKOUT_EN_MSK : 0, i);
}

int32_t ad713x_ref_gain_correction_en(uintptr_t base, bool enable, int i)
{
    return ad713x_spi_write_mask(base, AD713X_REG_DEVICE_CONFIG1,
                                 AD713X_DEV_CONFIG1_REF_GAIN_CORR_EN_MSK,
                                 enable ? AD713X_DEV_CONFIG1_REF_GAIN_CORR_EN_MSK : 0, i);
}

static int32_t ad713x_remove_gpio(uintptr_t base, int i)
{
    int32_t ret;

    return 0;
}

static int32_t ad713x_init_chan_bw(uintptr_t base, int i)
{
    int8_t ic;
    int32_t ret;

    //        for (i = CH3; i >= 0; i--) {
    //            ret = ad713x_wideband_bw_sel(dev, i, 0);
    //            if (NO_OS_IS_ERR_VALUE(ret))
    //                return -1;
    //        }

    return 0;
}
int odr_trigger(void *base_addr){

    uintptr_t base = (uintptr_t)base_addr;
    XGpio_WriteReg(base + ODRTRIG_GPO_BASEADDR , XGPIO_DATA_OFFSET ,0x0);

    //XSpi_WriteReg(dev, TRI_IO_BASEADDR + XGPIO_TRI_OFFSET 0xFE);

    return 0;
}
int odr_trigger_o(void *base_addr){
    uintptr_t base = (uintptr_t)base_addr;
    XGpio_WriteReg(base +  ODRTRIG_GPO_BASEADDR , XGPIO_DATA_OFFSET,0xFF);

    //XSpi_WriteReg(dev, TRI_IO_BASEADDR + XGPIO_TRI_OFFSET 0xFF);

    return 0;
}
int ad4134_deinit(void *base_addr){
    uintptr_t base = (uintptr_t)base_addr;
    XGpio_WriteReg(base +  AD4134_GPO_BASEADDR , XGPIO_TRI2_OFFSET, 0x0);
    XGpio_WriteReg(base +  AD4134_GPO_BASEADDR , XGPIO_DATA2_OFFSET ,0xFF);

    return 0;
}
int fifo_clear(void *base_addr){
    uintptr_t base = (uintptr_t)base_addr;
    printf("fifo clear############################\n");
    XGpio_WriteReg(base+ AD4134_GPO_BASEADDR , XGPIO_TRI2_OFFSET ,0x0);
    XGpio_WriteReg(base+ AD4134_GPO_BASEADDR , XGPIO_DATA2_OFFSET, 0x0);

    return 0;
}
int write_freq_high(void *base_addr, int64_t freq_in){
    uintptr_t base = (uintptr_t)base_addr;
    XGpio_WriteReg(base+ FREQ_GPO_BASEADDR, 0x0 ,freq_in);

  //  unsigned int b= XGpio_ReadReg(base+ FREQ_GPO_BASEADDR, 0x0);
  //  printf("XGpio_ReadReg  %u \n",b);


    return 0;
}
int write_freq_low(void *base_addr, int64_t freq_in){
    uintptr_t base = (uintptr_t)base_addr;
    XGpio_WriteReg(base+ FREQ_GPO_BASEADDR , 0x4 ,freq_in);

    return 0;
}
int write_tlast(void *base_addr, int64_t tlast){
    uintptr_t base = (uintptr_t)base_addr;

    XGpio_WriteReg(base+ TLAST_GPO_BASEADDR , 0x0 ,tlast);
    return 0;
}
int sync_start(void *base_addr){
    uintptr_t base = (uintptr_t)base_addr;
    //usleep(1000);
    XGpio_WriteReg(base+ AD4134_GPO_BASEADDR , XGPIO_TRI_OFFSET, 0x0);
    XGpio_WriteReg(base+ AD4134_GPO_BASEADDR , XGPIO_DATA_OFFSET ,0xFF);

    //usleep(1000);
    XGpio_WriteReg(base+ AD4134_GPO_BASEADDR, XGPIO_TRI_OFFSET ,0x0);
    XGpio_WriteReg(base+ AD4134_GPO_BASEADDR , XGPIO_DATA_OFFSET ,0x0);

    //usleep(1000);
    XGpio_WriteReg(base+ AD4134_GPO_BASEADDR , XGPIO_TRI_OFFSET ,0xFF);

    return 0;
}
int sync_input(void *base_addr){
    uintptr_t base = (uintptr_t)base_addr;
    XGpio_WriteReg(base + AD4134_GPO_BASEADDR , XGPIO_TRI_OFFSET, 0xFF);

    return 0;
}
int32_t ad4134_init(void *base_addr, int i)
{
    uintptr_t base = (uintptr_t)base_addr;
    int32_t ret;
    uint8_t data;
//    struct timeval start, end;
//    gettimeofday(&start, NULL);

    ad713x_spi_reg_write(base,0x0,0x98,i);   // soft reset

//    ret = ad713x_spi_reg_read(dev, AD713X_REG_CHIP_TYPE, &data, i);
////printf("data reg1 0x%x\n", data);
//    if (NO_OS_IS_ERR_VALUE(ret))
//        goto error_gpio;

    //printf("data reg2 0x%x\n", data);
    data |= AD713X_DEV_CONFIG_PWR_MODE_MSK;
    ret = ad713x_spi_reg_write(base, AD713X_REG_DEVICE_CONFIG, data, i);
    if (NO_OS_IS_ERR_VALUE(ret))
        goto error_gpio;

//    ret = ad713x_spi_reg_read(dev, AD713X_REG_DEVICE_CONFIG, &data, i);

//    ret = ad713x_clkout_output_en(dev, true, i);
//    if (NO_OS_IS_ERR_VALUE(ret))
//        goto error_gpio;

    ret = ad713x_ref_gain_correction_en(base, true, i);
    if (NO_OS_IS_ERR_VALUE(ret))
        goto error_gpio;

    ad713x_spi_reg_write(base,0x11,0x20,i);  //24 no crc
    ad713x_spi_reg_write(base, 0x1,0x82,i);
    ad713x_spi_reg_write(base,0x24,0x12,i);

    //        ret = ad713x_set_out_data_frame(dev, ADC_24_BIT_DATA,
    //                NO_CRC);  //ADC_24_BIT_DATA  ADC_16_BIT_DATA  CRC_6   NO_CRC
    //        if (NO_OS_IS_ERR_VALUE(ret))
    //            goto error_gpio;

    //        ret = ad713x_dout_format_config(dev, SINGLE_CH_DC);
    //        if (NO_OS_IS_ERR_VALUE(ret))
    //            goto error_gpio;

//            ret = ad713x_mag_phase_clk_delay(dev, false , i);   // zhuxiao 12.11.2024
//            if (NO_OS_IS_ERR_VALUE(ret))
//                goto error_gpio;

    //	ret = ad713x_init_chan_bw(dev);
    //	if (NO_OS_IS_ERR_VALUE(ret))
    //		goto error_gpio;

    //sleep(1);
    //ret = ad713x_spi_reg_dump(dev, i);
    //usleep(100);
    int mask = 0;
    ad713x_spi_reg_read(base,0x15,&mask,i);
  //  printf("nr.%d pll registeris %d\n",i, mask);
   if(mask != 5 && mask!=1)
    {
         ad713x_spi_reg_read(base,0x15,&mask,i);
         printf("nr.%d PLL failed %d\n",i,mask);
         goto error_gpio;
    }else{
         printf("nr.%d PLL success %d\n",i, mask);
    }
//   gettimeofday(&end, NULL);
//   printf("ad4134_init_intern time ---sec: %ld, ---usec: %ld\n", end.tv_sec-start.tv_sec, end.tv_usec-start.tv_usec);
//   j=0;
//      while(j<30){
//      for(i=0;i<1;i++)
//      {
//          slave = 0x0001 << i;
//          ad713x_spi_reg_read(dev, 0x15, &mask,i);
//          printf("--> AD4134- 0x%02x DeviceStatus: 0x%02x\n", i,reg);
//      }
//      j++;
//      }

    return 0;

error_gpio:
    return -2;
error_dev:


    return -1;
}


int32_t ad713x_remove(uintptr_t base, int i)
{

}

int32_t ad713x_spi_reg_dump(uintptr_t base, int i)
{
    int32_t ret;
    uint8_t reg_data;
    uint8_t reg_addr;

    for (reg_addr = 0x0; reg_addr <= 0x7; reg_addr++) {
        ret = ad713x_spi_reg_read(base, reg_addr, &reg_data, i);
        if (NO_OS_IS_ERR_VALUE(ret)) {
            printf("REG%x: error read\r\n", reg_addr);
            return -1;
        } else
            printf("REG%x: 0x%08x\r\n", reg_addr, reg_data);
    }
    for (reg_addr = 0xA; reg_addr <= 0x42; reg_addr++) {
        ret = ad713x_spi_reg_read(base, reg_addr, &reg_data, i);
        if (NO_OS_IS_ERR_VALUE(ret)) {
            printf("REG%x: error read\r\n", reg_addr);
            return -1;
        } else
            printf("REG%x: 0x%08x\r\n", reg_addr, reg_data);
    }
    for (reg_addr = 0x47; reg_addr <= 0x48; reg_addr++) {
        ret = ad713x_spi_reg_read(base, reg_addr, &reg_data, i);
        if (NO_OS_IS_ERR_VALUE(ret)) {
            printf("REG%x: error read\r\n", reg_addr);
            return -1;
        } else
            printf("REG%x: 0x%08x\r\n", reg_addr, reg_data);
    }

    return 0;
}
