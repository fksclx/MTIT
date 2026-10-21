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


/* ltoh: little to host */
/* htol: little to host */
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
#define MAP_MASK (MAP_SIZE - 1)
#define FPGA_DDR_START_ADDR 0x00000000
#define MAX_BYTES_PER_TRANSFER 0x800000

static void * user_base;
static int h_user;
//static int h_event0;
//static int h_event1;
//static int h_event2;
//static int h_event3;

//static unsigned int  user_irq_ack;


static void *mmap_control(int device,long mapsize)
{
    void *vir_addr;
    vir_addr = mmap(0, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED, device, 0);
    return vir_addr;
}

void pcie_reg_write(unsigned int address,unsigned int val)
{
    unsigned int writeval = htoll(val);
    *((unsigned int *)(user_base+address)) = writeval;
}

unsigned int pcie_reg_read(unsigned int address)
{
    unsigned int read_result = *((unsigned int *)(user_base+address));
    read_result = ltohl(read_result);
    return read_result;
}

int pcie_init()
{
    char* user_name = "/dev/xdma0_user";

    h_user = open(user_name, O_RDWR | O_SYNC);
    if(h_user < 0) { printf("open user error\n"); return -3; };

    user_base = mmap_control(h_user,MAP_SIZE);
    return 1;
}

void pcie_deinit()
{
    //user_irq_ack = 0x00000000;
    //write_device(h_user, 0x04, 4, (unsigned char *)&user_irq_ack);//clear irq
    close(h_user);
}

ssize_t read_to_buffer(char *fname, int fd, char *buffer, uint64_t size,
            uint64_t base);
ssize_t write_from_buffer(char *fname, int fd, char *buffer, uint64_t size,
            uint64_t base);
void timespec_sub(struct timespec *t1, struct timespec *t2);

int test_dma(char *devname, uint64_t addr, uint64_t aperture,
            uint64_t size, uint64_t offset, uint64_t count,
            char *ofname, int eop_flush)
{
    ssize_t rc = 0;
    size_t out_offset = 0;
    size_t bytes_done = 0;
    uint64_t i;
    uint64_t apt_loop = aperture ? (size + aperture - 1) / aperture : 0;
    char *buffer = NULL;
    char *allocated = NULL;
    struct timespec ts_start, ts_end;
    int out_fd = -1;
    int fpga_fd;
    long total_time = 0;
    float result;
    float avg_time = 0;
    int underflow = 0;

    int verbose = 0;

    /*
     * use O_TRUNC to indicate to the driver to flush the data up based on
     * EOP (end-of-packet), streaming mode only
     */
    if (eop_flush)
        fpga_fd = open(devname, O_RDWR | O_TRUNC);
    else
        fpga_fd = open(devname, O_RDWR);

    if (fpga_fd < 0) {
                fprintf(stderr, "unable to open device %s, %d.\n",
                        devname, fpga_fd);
        perror("open device");
                return -EINVAL;
        }

    /* create file to write data to */
    if (ofname) {
        out_fd = open(ofname, O_RDWR | O_CREAT | O_TRUNC | O_NONBLOCK,//O_SYNC,
                0666);
        if (out_fd < 0) {
                        fprintf(stderr, "unable to open output file %s, %d.\n",
                                ofname, out_fd);
            perror("open output file");
                        rc = -EINVAL;
                        goto out;
                }
    }

    posix_memalign((void **)&allocated, 4096 /*alignment */ , size + 4096);
    if (!allocated) {
        fprintf(stderr, "OOM %lu.\n", size + 4096);
        rc = -ENOMEM;
        goto out;
    }

    buffer = allocated + offset;
    if (verbose)
    fprintf(stdout, "host buffer 0x%lx, %p.\n", size + 4096, buffer);

    for (i = 0; i < count; i++) {
        rc = clock_gettime(CLOCK_MONOTONIC, &ts_start);
        if (apt_loop) {
            uint64_t j;
            uint64_t len = size;
            char *buf = buffer;

            bytes_done = 0;
            for (j = 0; j < apt_loop; j++, len -= aperture, buf += aperture) {
                uint64_t bytes = (len > aperture) ? aperture : len,
                rc = read_to_buffer(devname, fpga_fd, buf,
                        bytes, addr);
                if (rc < 0)
                    goto out;

                if (!underflow && rc < bytes)
                    underflow = 1;
                bytes_done += rc;
            }
        } else {
            rc = read_to_buffer(devname, fpga_fd, buffer, size, addr);
            if (rc < 0)
                goto out;
            bytes_done = rc;

            if (!underflow && bytes_done < size)
                underflow = 1;
        }
        clock_gettime(CLOCK_MONOTONIC, &ts_end);


        /* subtract the start time from the end time */
        timespec_sub(&ts_end, &ts_start);
        total_time += ts_end.tv_nsec;
        /* a bit less accurate but side-effects are accounted for */
        if (verbose)
        fprintf(stdout,
            "#%lu: CLOCK_MONOTONIC %ld.%09ld sec. read %ld/%ld bytes\n",
            i, ts_end.tv_sec, ts_end.tv_nsec, bytes_done, size);

        /* file argument given? */
        if (out_fd >= 0) {
            rc = write_from_buffer(ofname, out_fd, buffer,
                     bytes_done, out_offset);
            if (rc < 0 || rc < bytes_done)
                goto out;
            out_offset += bytes_done;
        }
    }


        avg_time = (float)total_time/(float)count;
        result = ((float)size)*1000/avg_time;
        if (verbose)
            printf("** Avg time device %s, total time %ld nsec, avg_time = %f, size = %lu, BW = %f \n",
                devname, total_time, avg_time, size, result);
        printf("%s ** Average BW = %lu, %f\n", devname, size, result);
        rc = 0;

out:
    close(fpga_fd);
    if (out_fd >= 0)
        close(out_fd);
    free(allocated);

    return rc;
}

