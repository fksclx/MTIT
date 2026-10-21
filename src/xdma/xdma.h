
#ifndef XDMA_H_
#define XDMA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

unsigned int pcie_reg_read(unsigned int address);
void pcie_reg_write(unsigned int address,unsigned int val);

void pcie_deinit();
int pcie_init();

int test_dma(char *devname, uint64_t addr, uint64_t aperture,
            uint64_t size, uint64_t offset, uint64_t count,
            char *ofname,int eop_flush);

#ifdef __cplusplus
}
#endif

#endif /* XDMA_H_ */

