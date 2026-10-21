#include <stdio.h>
#include "pxi.h"
#include "../../xdma/xdma.h"
#include "../../xilinx/gpio/xgpio.h"
#include "../../xilinx/comm/xparameters.h"

#define PXI_GA_BASEADDR     XPAR_GPIO_0_BASEADDR

int pxi_init()
{
    int status, slot;
    int slot_gpio_chn;

    status = pcie_init();
    if (status < 0)
        return -1;

    printf("PXIe card initialized.\n");

    slot_gpio_chn = 1;
    slot = XGpio_ReadReg(
            PXI_GA_BASEADDR,
            ((slot_gpio_chn - 1) * XGPIO_CHAN_OFFSET) + XGPIO_DATA_OFFSET);

    printf("PXIe Slot: %d\n", slot);

    return 0;
}

void pxi_term()
{
    pcie_deinit();
}
