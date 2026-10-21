/*
 * 7022 DA / 7021 AD 单通道闭环控制统一 Demo
 *
 * 已实机验证的物理链路：7022 J9 -> 7021 J2。两端均支持 PhotoMOS
 * 控制的 AC/DC 耦合；本程序每次启动显式配置，默认均为 AC。
 * 采样侧以 48 kHz 接收 ADC 数据，每 48 帧组成一个约 1 ms 的控制窗口；
 * 每个窗口调用一次 control_step()，输出侧以 1 kHz 发送一个 16 字节 DA 命令包。
 *
 * 交接边界：
 *   1. Si5319、PXI 同步、AD4134、XDMA、H2C 打包和安全退出属于已验证硬件层，
 *      软件算法开发时不应随意修改这些时序和协议常量。
 *   2. adc_window_feedback() 当前仅计算 48 点算术均值；任一端为 AC 时，
 *      该均值不适合作为非零直流设定值的反馈，应改为 RMS、峰峰值、
 *      同步解调、滤波状态或参考波形误差等交流特征。只有 J9/J2 均为 DC
 *      且已确认零偏安全后，才可评估非零直流均值控制。
 *   3. control_step() 是软件组主要替换入口。当前 P 控制仅用于验证闭环调用链，
 *      不代表已经完成针对实际对象的控制器设计或参数整定。
 *   4. 未显式指定 --enable-output 时，程序绝不允许非零 DA 输出。
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ======================== 已验证硬件寄存器与协议常量 ========================
 *
 * 下列地址、位定义和帧尺寸来自当前 7022/7021 FPGA 工程及实机测试。
 * 软件算法开发通常不需要修改；若 FPGA 寄存器表或 bitstream 发生变化，
 * 必须与硬件组重新核对后再更新。
 */
#define ADC_REG_TLAST       0x00040000u
#define ADC_SYNC_GPIO_BASE 0x00010000u
#define ADC_IIC_BASE_DEFAULT 0x00030000u

#define XIIC_IISR_OFFSET  0x020u
#define XIIC_RESETR_OFFSET 0x040u
#define XIIC_CR_OFFSET    0x100u
#define XIIC_SR_OFFSET    0x104u
#define XIIC_DTR_OFFSET   0x108u
#define XIIC_DRR_OFFSET   0x10Cu
#define XIIC_RFD_OFFSET   0x120u
#define XIIC_GPO_OFFSET   0x124u
#define XIIC_RESET_MASK   0x0000000Au
#define XIIC_CR_ENABLE_MASK 0x00000001u
#define XIIC_CR_TX_FIFO_RESET_MASK 0x00000002u
#define XIIC_SR_BUS_BUSY_MASK 0x00000004u
#define XIIC_SR_RX_FIFO_EMPTY_MASK 0x00000040u
#define XIIC_SR_TX_FIFO_EMPTY_MASK 0x00000080u
#define XIIC_INTR_ARB_LOST_MASK 0x00000001u
#define XIIC_INTR_TX_ERROR_MASK 0x00000002u
#define XIIC_INTR_TX_EMPTY_MASK 0x00000004u
#define XIIC_INTR_BNB_MASK 0x00000010u
#define XIIC_TX_DYN_START_MASK 0x00000100u
#define XIIC_TX_DYN_STOP_MASK 0x00000200u
#define XIIC_READ_OPERATION 1u
#define XIIC_RX_FIFO_DEPTH 16u

#define PCA9555_ADDR_7022_U15 0x21u
#define PCA9555_ADDR_7021_U11 0x23u
#define PCA9555_OUTPUT_PORT1  0x03u
#define PCA9555_CONFIG_PORT1  0x07u
#define J9_COUPLING_MASK      0x20u
#define J2_COUPLING_MASK      0x30u
#define COUPLING_IIC_TIMEOUT_MS 1000u
#define COUPLING_SETTLE_MS      100u

#define ADC_SPI_COUNT 4u
#define XSP_SRR_OFFSET 0x40u
#define XSP_CR_OFFSET  0x60u
#define XSP_SR_OFFSET  0x64u
#define XSP_DTR_OFFSET 0x68u
#define XSP_DRR_OFFSET 0x6Cu
#define XSP_SSR_OFFSET 0x70u
#define XSP_SRR_RESET_MASK 0x0000000Au
#define XSP_CR_ENABLE_MASK 0x00000002u
#define XSP_CR_MASTER_MODE_MASK 0x00000004u
#define XSP_CR_TRANS_INHIBIT_MASK 0x00000100u
#define XSP_SR_RX_EMPTY_MASK 0x00000001u
#define XSP_SR_TX_EMPTY_MASK 0x00000004u
#define XSP_SR_TX_FULL_MASK 0x00000008u
#define XGPIO_DATA_OFFSET 0x00u
#define XGPIO_TRI_OFFSET  0x04u
#define XGPIO_DATA2_OFFSET 0x08u
#define XGPIO_TRI2_OFFSET  0x0Cu

#define ADC_DIG_FILTER_SINC3_ALL          0xAAu

#define REG_CTRL          0x00u
#define REG_BOARD_ID      0x08u
#define REG_STATUS        0x0Cu
#define REG_H2C_RX_COUNT  0x24u
#define REG_H2C_ERR_COUNT 0x28u
#define REG_H2C_LAST_SEQ  0x2Cu
#define REG_H2C_LAST_DATA 0x30u
#define REG_MCLK_ALIVE    0x38u

#define CTRL_SYNC_ENABLE   0x00000001u
#define CTRL_DA_ENABLE     0x00000002u
#define CTRL_RUN_WITH_PXI  (CTRL_SYNC_ENABLE | CTRL_DA_ENABLE)
#define CTRL_CLEAR_STATUS  0x00000004u
#define STATUS_FIFO_OVERFLOW (1u << 2)
#define STATUS_FIFO_FULL     (1u << 3)
#define PKT_CMD 0xC2u


/*
 * FPGA H2C 控制包必须严格为 16 字节。 为什么要封控制包？
 * ctrl_data 的高、低两个 16 位字段写入同一个有符号 DA code；
 * FPGA 已验证会按 1 kHz trigger_event 消费一帧。
 */
struct __attribute__((packed)) h2c_packet {
    uint32_t timestamp;
    uint16_t seq;
    uint8_t dst_id;
    uint8_t pkt_cmd;
    uint32_t checksum;
    uint32_t ctrl_data;
};

_Static_assert(sizeof(struct h2c_packet) == 16u,
               "7022 H2C packet must be exactly 16 bytes");

static volatile sig_atomic_t stop_requested;
static int da_user_fd = -1;
static int da_h2c_fd = -1;
static uint16_t da_sequence;
static uint8_t da_board_id;

static void request_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static int32_t sign_extend_24(uint32_t word)
{
    uint32_t code = word & 0x00FFFFFFu;

    if ((code & 0x00800000u) != 0u) {
        code |= 0xFF000000u;
    }
    return (int32_t)code;
}

static double adc_code_to_volts(int32_t code, unsigned gain)
{
    double full_span;

    switch (gain) {
    case 1:
        full_span = 2.048 * 4.99 * 2.0;
        break;
    case 2:
        full_span = 2.048 * 4.99;
        break;
    case 10:
        full_span = 2.048 * 4.99 * 0.2;
        break;
    default:
        return NAN;
    }
    return (double)code * full_span / 16777216.0;
}

static int32_t packed_channel_code(const uint8_t *frame_data,
                                   unsigned words_per_frame,
                                   unsigned channel)
{
    size_t bytes_per_frame = (size_t)words_per_frame * sizeof(uint32_t);
    unsigned group = channel / 4u;
    unsigned position = channel % 4u;
    size_t sample_offset = (size_t)group * 16u + 4u + (size_t)position * 3u;
    uint32_t word;

    if (sample_offset + 3u > bytes_per_frame) {
        return 0;
    }
    word = (uint32_t)frame_data[sample_offset] |
            ((uint32_t)frame_data[sample_offset + 1u] << 8) |
            ((uint32_t)frame_data[sample_offset + 2u] << 16);
    return sign_extend_24(word);
}

static int reg_read(uint32_t offset, uint32_t *value)
{
    ssize_t n = pread(da_user_fd, value, sizeof(*value), (off_t)offset);

    if (n != (ssize_t)sizeof(*value)) {
        if (n >= 0) errno = EIO;
        fprintf(stderr, "ERROR: DA register read 0x%02" PRIX32 ": %s\n",
                offset, strerror(errno));
        return -1;
    }
    return 0;
}

static int reg_write(uint32_t offset, uint32_t value)
{
    ssize_t n = pwrite(da_user_fd, &value, sizeof(value), (off_t)offset);

    if (n != (ssize_t)sizeof(value)) {
        if (n >= 0) errno = EIO;
        fprintf(stderr, "ERROR: DA register write 0x%02" PRIX32 ": %s\n",
                offset, strerror(errno));
        return -1;
    }
    return 0;
}
struct h2c_packet packet;
/* 将一个有符号 DA code 封装为已验证的 16 字节 H2C 包并完整写入 XDMA。 */
int send_da_sample(int chnl,uint16_t sample)
{
    if(chnl !=4)
        return 0 ;
    ssize_t n;
    packet.timestamp = da_sequence;
    packet.seq = ++da_sequence;
    packet.dst_id = da_board_id;
    packet.pkt_cmd = PKT_CMD;
    packet.checksum = 0xAAAAAAAAu;
    //这是什么意思？
    packet.ctrl_data = ((uint32_t)sample << 16) | sample;

    n = write(da_h2c_fd, &packet, sizeof(packet));
    if (n != (ssize_t)sizeof(packet)) {
        if (n >= 0) errno = EIO;
        fprintf(stderr, "ERROR: H2C write: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

static int sleep_ms(unsigned milliseconds)
{
    struct timespec delay;

    delay.tv_sec = (time_t)(milliseconds / 1000u);
    delay.tv_nsec = (long)(milliseconds % 1000u) * 1000000L;
    while (nanosleep(&delay, &delay) != 0) {
        if (errno != EINTR) return -1;
        if (stop_requested) return -1;
    }
    return 0;
}

static int sleep_ms_cleanup(unsigned milliseconds)
{
    struct timespec delay;

    delay.tv_sec = (time_t)(milliseconds / 1000u);
    delay.tv_nsec = (long)(milliseconds % 1000u) * 1000000L;
    while (nanosleep(&delay, &delay) != 0) {
        if (errno != EINTR) return -1;
    }
    return 0;
}

/* ============================ 7021 AD 硬件访问层 ============================ */
static int adc_bar_read32(int fd, uint32_t offset, uint32_t *value)
{
    ssize_t n = pread(fd, value, sizeof(*value), (off_t)offset);

    if (n != (ssize_t)sizeof(*value)) {
        if (n >= 0) errno = EIO;
        fprintf(stderr, "ERROR: ADC BAR read 0x%08" PRIX32 ": %s\n",
                offset, strerror(errno));
        return -1;
    }
    return 0;
}

static int adc_bar_write32(int fd, uint32_t offset, uint32_t value)
{
    ssize_t n = pwrite(fd, &value, sizeof(value), (off_t)offset);

    if (n != (ssize_t)sizeof(value)) {
        if (n >= 0) errno = EIO;
        fprintf(stderr, "ERROR: ADC BAR write 0x%08" PRIX32 ": %s\n",
                offset, strerror(errno));
        return -1;
    }
    return 0;
}

static int monotonic_deadline_ms(unsigned timeout_ms, struct timespec *deadline)
{
    if (clock_gettime(CLOCK_MONOTONIC, deadline) != 0) return -1;
    deadline->tv_sec += (time_t)(timeout_ms / 1000u);
    deadline->tv_nsec += (long)(timeout_ms % 1000u) * 1000000L;
    if (deadline->tv_nsec >= 1000000000L) {
        deadline->tv_sec += 1;
        deadline->tv_nsec -= 1000000000L;
    }
    return 0;
}

static int deadline_expired(const struct timespec *deadline)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 1;
    return now.tv_sec > deadline->tv_sec ||
            (now.tv_sec == deadline->tv_sec && now.tv_nsec >= deadline->tv_nsec);
}

static int iic_read32(int fd, uint32_t base, uint32_t offset, uint32_t *value)
{
    return adc_bar_read32(fd, base + offset, value);
}

static int iic_write32(int fd, uint32_t base, uint32_t offset, uint32_t value)
{
    return adc_bar_write32(fd, base + offset, value);
}

/* ========================== PCA9555 耦合控制层 ==========================
 *
 * 两块板都通过各自 user BAR 中的 AXI-IIC（base=0x00030000）访问 PCA9555。
 * 这里只做目标位的读改写，禁止整片初始化为零，否则会覆盖其他通道、IEPE
 * 或前端继电器状态。Si5319 仍使用后面的专用事务，不与此处读取流程混用。
 */
enum coupling_mode {
    COUPLING_AC = 0,
    COUPLING_DC = 1
};

struct coupling_endpoint {
    const char *name;
    uint8_t address;
    uint8_t mask;
};


static const char *coupling_mode_name(enum coupling_mode mode)
{
    return mode == COUPLING_DC ? "DC" : "AC";
}

static int iic_dump_status(int fd, uint32_t base, const char *operation)
{
    uint32_t control = 0u;
    uint32_t status = 0u;
    uint32_t interrupts = 0u;

    (void)iic_read32(fd, base, XIIC_CR_OFFSET, &control);
    (void)iic_read32(fd, base, XIIC_SR_OFFSET, &status);
    (void)iic_read32(fd, base, XIIC_IISR_OFFSET, &interrupts);
    fprintf(stderr,
            "ERROR: AXI IIC %s failed: CR=0x%08" PRIX32
            " SR=0x%08" PRIX32 " IISR=0x%08" PRIX32 "\n",
            operation, control, status, interrupts);
    return -1;
}

static int iic_wait_mask(int fd, uint32_t base, uint32_t offset,
                         uint32_t mask, int want_set, unsigned timeout_ms,
                         const char *description)
{
    struct timespec deadline;
    uint32_t value = 0u;

    if (monotonic_deadline_ms(timeout_ms, &deadline) != 0) return -1;
    for (;;) {
        if (iic_read32(fd, base, offset, &value) != 0) return -1;
        if (((value & mask) != 0u) == want_set) return 0;
        if (deadline_expired(&deadline)) {
            fprintf(stderr,
                    "ERROR: timeout waiting for AXI IIC %s "
                    "(reg=0x%03" PRIX32 ", value=0x%08" PRIX32 ")\n",
                    description, offset, value);
            return -1;
        }
        if (stop_requested || sleep_ms(1u) != 0) return -1;
    }
}

static int iic_clear_interrupts(int fd, uint32_t base)
{
    uint32_t interrupts;

    if (iic_read32(fd, base, XIIC_IISR_OFFSET, &interrupts) != 0)
        return -1;
    if (interrupts == 0u) return 0;
    return iic_write32(fd, base, XIIC_IISR_OFFSET, interrupts);
}

static int iic_wait_bus_free(int fd, uint32_t base, unsigned timeout_ms)
{
    return iic_wait_mask(fd, base, XIIC_SR_OFFSET,
                         XIIC_SR_BUS_BUSY_MASK, 0, timeout_ms, "bus free");
}

static int iic_wait_tx_empty(int fd, uint32_t base, unsigned timeout_ms)
{
    struct timespec deadline;

    if (monotonic_deadline_ms(timeout_ms, &deadline) != 0) return -1;
    for (;;) {
        uint32_t interrupts;
        uint32_t status;

        if (iic_read32(fd, base, XIIC_IISR_OFFSET, &interrupts) != 0 ||
                iic_read32(fd, base, XIIC_SR_OFFSET, &status) != 0) return -1;
        if ((interrupts & (XIIC_INTR_TX_ERROR_MASK |
                XIIC_INTR_ARB_LOST_MASK)) != 0u)
            return iic_dump_status(fd, base, "transmit");
        if ((interrupts & XIIC_INTR_TX_EMPTY_MASK) != 0u ||
                ((status & (XIIC_SR_TX_FIFO_EMPTY_MASK |
                        XIIC_SR_BUS_BUSY_MASK)) ==
                                XIIC_SR_TX_FIFO_EMPTY_MASK)) {
            return 0;
        }
        if (deadline_expired(&deadline))
            return iic_dump_status(fd, base, "TX-empty timeout");
        if (stop_requested || sleep_ms(1u) != 0) return -1;
    }
}

static int iic_wait_rx_data(int fd, uint32_t base, unsigned timeout_ms)
{
    struct timespec deadline;

    if (monotonic_deadline_ms(timeout_ms, &deadline) != 0) return -1;
    for (;;) {
        uint32_t interrupts;
        uint32_t status;

        if (iic_read32(fd, base, XIIC_SR_OFFSET, &status) != 0 ||
                iic_read32(fd, base, XIIC_IISR_OFFSET, &interrupts) != 0)
            return -1;
        if ((status & XIIC_SR_RX_FIFO_EMPTY_MASK) == 0u) return 0;
        if ((interrupts & (XIIC_INTR_TX_ERROR_MASK |
                XIIC_INTR_ARB_LOST_MASK)) != 0u)
            return iic_dump_status(fd, base, "receive");
        if (deadline_expired(&deadline))
            return iic_dump_status(fd, base, "RX-data timeout");
        if (stop_requested || sleep_ms(1u) != 0) return -1;
    }
}

static int iic_init(int fd, uint32_t base)
{
    uint32_t status;

    if (iic_write32(fd, base, XIIC_RESETR_OFFSET, XIIC_RESET_MASK) != 0 ||
            iic_write32(fd, base, XIIC_RFD_OFFSET, XIIC_RX_FIFO_DEPTH - 1u) != 0 ||
            iic_write32(fd, base, XIIC_CR_OFFSET, XIIC_CR_TX_FIFO_RESET_MASK) != 0 ||
            iic_write32(fd, base, XIIC_CR_OFFSET, XIIC_CR_ENABLE_MASK) != 0 ||
            iic_clear_interrupts(fd, base) != 0 ||
            iic_read32(fd, base, XIIC_SR_OFFSET, &status) != 0) {
        return -1;
    }
    if ((status & XIIC_SR_BUS_BUSY_MASK) != 0u)
        return iic_dump_status(fd, base, "initialization");
    return 0;
}

static int iic_write_bytes(int fd, uint32_t base, uint8_t address,
                           const uint8_t *data, size_t size,
                           unsigned timeout_ms)
{
    size_t index;

    if (size == 0u || size > 255u ||
            iic_wait_bus_free(fd, base, timeout_ms) != 0) return -1;
    if (iic_write32(fd, base, XIIC_DTR_OFFSET,
                    XIIC_TX_DYN_START_MASK | ((uint32_t)address << 1)) != 0 ||
            iic_clear_interrupts(fd, base) != 0) return -1;
    for (index = 0u; index < size; ++index) {
        uint32_t word = data[index];

        if (index + 1u == size) word |= XIIC_TX_DYN_STOP_MASK;
        if (iic_write32(fd, base, XIIC_DTR_OFFSET, word) != 0) return -1;
    }
    if (iic_wait_tx_empty(fd, base, timeout_ms) != 0 ||
            iic_wait_bus_free(fd, base, timeout_ms) != 0) {
        return iic_dump_status(fd, base, "write");
    }
    return 0;
}

static int pca9555_write_reg(int fd, uint32_t iic_base, uint8_t address,
                             uint8_t reg, uint8_t value,
                             unsigned timeout_ms)
{
    uint8_t data[2] = {reg, value};

    if (iic_write_bytes(fd, iic_base, address, data, sizeof(data),
                        timeout_ms) != 0) {
        fprintf(stderr, "ERROR: PCA9555 0x%02X write reg 0x%02X\n",
                address, reg);
        return -1;
    }
    return 0;
}

static int pca9555_read_reg(int fd, uint32_t iic_base, uint8_t address,
                            uint8_t reg, uint8_t *value,
                            unsigned timeout_ms)
{
    uint32_t word;

    if (iic_wait_bus_free(fd, iic_base, timeout_ms) != 0 ||
            iic_clear_interrupts(fd, iic_base) != 0 ||
            iic_write32(fd, iic_base, XIIC_DTR_OFFSET,
                        XIIC_TX_DYN_START_MASK | ((uint32_t)address << 1)) != 0 ||
                        iic_write32(fd, iic_base, XIIC_DTR_OFFSET, reg) != 0 ||
                        iic_write32(fd, iic_base, XIIC_DTR_OFFSET,
                                    XIIC_TX_DYN_START_MASK |
                                    ((uint32_t)address << 1) | XIIC_READ_OPERATION) != 0 ||
                                    iic_write32(fd, iic_base, XIIC_DTR_OFFSET,
                                                XIIC_TX_DYN_STOP_MASK | 1u) != 0 ||
                                                iic_wait_rx_data(fd, iic_base, timeout_ms) != 0 ||
                                                iic_read32(fd, iic_base, XIIC_DRR_OFFSET, &word) != 0 ||
                                                iic_wait_bus_free(fd, iic_base, timeout_ms) != 0) {
        fprintf(stderr, "ERROR: PCA9555 0x%02X read reg 0x%02X\n",
                address, reg);
        return iic_dump_status(fd, iic_base, "PCA9555 read");
    }
    *value = (uint8_t)word;
    return 0;
}

static int pca9555_update_mask(int fd, uint32_t iic_base, uint8_t address,
                               uint8_t reg, uint8_t mask, int set,
                               unsigned timeout_ms, const char *description)
{
    uint8_t before;
    uint8_t expected;
    uint8_t after;

    if (pca9555_read_reg(fd, iic_base, address, reg, &before,
                         timeout_ms) != 0) return -1;
    expected = set ? (uint8_t)(before | mask)
            : (uint8_t)(before & (uint8_t)~mask);
    if (pca9555_write_reg(fd, iic_base, address, reg, expected,
                          timeout_ms) != 0 ||
            pca9555_read_reg(fd, iic_base, address, reg, &after,
                             timeout_ms) != 0) return -1;
    if (after != expected) {
        fprintf(stderr,
                "ERROR: %s readback failed: PCA9555 0x%02X reg 0x%02X "
                "read=0x%02X expected=0x%02X\n",
                description, address, reg, after, expected);
        return -1;
    }
    printf("%s: PCA9555 0x%02X reg 0x%02X mask=0x%02X "
            "0x%02X -> 0x%02X\n",
            description, address, reg, mask, before, after);
    return 0;
}
static int read_configure_coupling(int fd, uint32_t iic_base,
                              const struct coupling_endpoint *endpoint,
                              enum coupling_mode mode)
{
    uint8_t before;
    if (pca9555_read_reg(fd, iic_base,  endpoint->address, PCA9555_OUTPUT_PORT1, &before,
                         COUPLING_IIC_TIMEOUT_MS) != 0) return -1;


       printf("%s: PCA9555 0x%02X reg 0x%02X 0x%02X \n",endpoint->name,
                endpoint->address, PCA9555_OUTPUT_PORT1,  before);

          if (pca9555_read_reg(fd, iic_base,  endpoint->address, PCA9555_CONFIG_PORT1, &before,
                               COUPLING_IIC_TIMEOUT_MS) != 0) return -1;



          printf("%s: PCA9555 0x%02X reg 0x%02X    0x%02X \n",
                 endpoint->name, endpoint->address, PCA9555_OUTPUT_PORT1,  before);
          return 0;
}
static int configure_coupling(int fd, uint32_t iic_base,
                              const struct coupling_endpoint *endpoint,
                              enum coupling_mode mode)
{
    /*
     * 先写输出锁存值，再把目标脚切为输出。若引脚此前是输入，可避免方向切换时
     * 短暂驱动旧锁存值，尤其避免请求 AC 时瞬间误闭合 PhotoMOS。
     */
    if (pca9555_update_mask(fd, iic_base, endpoint->address,
                            PCA9555_OUTPUT_PORT1, endpoint->mask,
                            1,//DC
                            COUPLING_IIC_TIMEOUT_MS, endpoint->name) != 0 ||
            pca9555_update_mask(fd, iic_base, endpoint->address,
                                PCA9555_CONFIG_PORT1, endpoint->mask, 0,
                                COUPLING_IIC_TIMEOUT_MS, endpoint->name) != 0) {
        return -1;
    }
    printf("%s coupling = %s verified\n",
           endpoint->name, coupling_mode_name(mode));
    return 0;
}

/* AD J2: U6 (0x21), P02 -> K10, P07 -> K11. Fixed DC pulse. */
static int configure_adc_j2_dc(int fd, uint32_t base)
{
    const uint8_t address = 0x21u;
    const uint8_t output_reg = 0x02u;
    const uint8_t config_reg = 0x06u;
    const uint8_t coil_mask = 0x84u;
    int result;

    /* Clear both coil latches before enabling their output drivers. */
    if (pca9555_update_mask(fd, base, address,
                           output_reg, coil_mask, 0,
                           COUPLING_IIC_TIMEOUT_MS, "AD J2 coil idle") != 0 ||
        pca9555_update_mask(fd, base, address,
                           config_reg, coil_mask, 0,
                           COUPLING_IIC_TIMEOUT_MS, "AD J2 coil direction") != 0)
        return -1;

    /* DC: P07 high, P02 low. Attempt release even if the pulse fails. */
    result = pca9555_update_mask(fd, base, address,
                                output_reg, 0x80u, 1,
                                COUPLING_IIC_TIMEOUT_MS, "AD J2 DC pulse");
    if (result == 0 && sleep_ms_cleanup(10u) != 0)
        result = -1;

    if (pca9555_update_mask(fd, base, address,
                           output_reg, coil_mask, 0,
                           COUPLING_IIC_TIMEOUT_MS, "AD J2 coil release") != 0)
        return -1;

    if (result != 0 || sleep_ms_cleanup(10u) != 0)
        return -1;

    printf("AD J2 DC switching pulse completed\n");
    return 0;
}

static int adc_spi_transfer(int fd, uint32_t base, uint16_t tx, uint16_t *rx)
{
    uint32_t control, status, word = 0;
    unsigned poll_count, fifo_words = 0;

    if (adc_bar_write32(fd, base + XSP_SRR_OFFSET, XSP_SRR_RESET_MASK) != 0 ||
            adc_bar_write32(fd, base + XSP_SSR_OFFSET, 1u) != 0 ||
            adc_bar_read32(fd, base + XSP_CR_OFFSET, &control) != 0) {
        return -1;
    }
    control |= XSP_CR_MASTER_MODE_MASK;
    if (adc_bar_write32(fd, base + XSP_CR_OFFSET, control) != 0 ||
            adc_bar_write32(fd, base + XSP_SSR_OFFSET, 0u) != 0) {
        return -1;
    }

    for (;;) {
        if (adc_bar_read32(fd, base + XSP_SR_OFFSET, &status) != 0) return -1;
        if ((status & XSP_SR_TX_FULL_MASK) != 0u) break;
        if (fifo_words >= 256u) {
            fprintf(stderr, "ERROR: ADC SPI TX FIFO did not become full at 0x%08" PRIX32 "\n", base);
            return -1;
        }
        if (adc_bar_write32(fd, base + XSP_DTR_OFFSET, tx) != 0) return -1;
        ++fifo_words;
    }

    if (adc_bar_read32(fd, base + XSP_CR_OFFSET, &control) != 0) return -1;
    control |= XSP_CR_ENABLE_MASK;
    control &= ~XSP_CR_TRANS_INHIBIT_MASK;
    if (adc_bar_write32(fd, base + XSP_CR_OFFSET, control) != 0) return -1;

    for (poll_count = 0; poll_count < 100000u; ++poll_count) {
        if (adc_bar_read32(fd, base + XSP_SR_OFFSET, &status) != 0) return -1;
        if ((status & XSP_SR_TX_EMPTY_MASK) != 0u) break;
    }
    if (poll_count == 100000u) {
        fprintf(stderr, "ERROR: ADC SPI TX timeout at 0x%08" PRIX32 "\n", base);
        return -1;
    }

    for (;;) {
        if (adc_bar_read32(fd, base + XSP_SR_OFFSET, &status) != 0) return -1;
        if ((status & XSP_SR_RX_EMPTY_MASK) != 0u) break;
        if (adc_bar_read32(fd, base + XSP_DRR_OFFSET, &word) != 0) return -1;
    }
    if (adc_bar_write32(fd, base + XSP_SSR_OFFSET, 1u) != 0) return -1;
    *rx = (uint16_t)word;
    return 0;
}

static int adc_spi_write_reg(int fd, uint32_t base, uint8_t reg, uint8_t value)
{
    uint16_t ignored;
    return adc_spi_transfer(fd, base, (uint16_t)(((uint16_t)reg << 8) | value), &ignored);
}

static int adc_spi_read_reg(int fd, uint32_t base, uint8_t reg, uint8_t *value)
{
    uint16_t rx;
    uint16_t command = (uint16_t)(((uint16_t)(reg | 0x80u)) << 8);

    if (adc_spi_transfer(fd, base, command, &rx) != 0 ||
            adc_spi_transfer(fd, base, command, &rx) != 0) {
        return -1;
    }
    *value = (uint8_t)(rx & 0xFFu);
    return 0;
}

/*
 * 使 ADC 数据通路保持在复位/停止状态。
 * 初始化前调用可建立确定状态；退出时再次调用，避免 C2H/FIFO 继续产数。
 */
static int prepare_adc_capture(int fd)
{
    struct timespec settle = {0, 1000000L};

    if (adc_bar_write32(fd, ADC_SYNC_GPIO_BASE + XGPIO_TRI2_OFFSET, 0u) != 0 ||
            adc_bar_write32(fd, ADC_SYNC_GPIO_BASE + XGPIO_DATA2_OFFSET, 0xFFu) != 0 ||
            adc_bar_write32(fd, ADC_SYNC_GPIO_BASE + XGPIO_TRI_OFFSET, 0u) != 0 ||
            adc_bar_write32(fd, ADC_SYNC_GPIO_BASE + XGPIO_DATA_OFFSET, 0xFFu) != 0) {
        return -1;
    }
    (void)nanosleep(&settle, NULL);
    return 0;
}

/* 按已验证顺序释放 ADC/FIFO reset，并启动同步采集。 */
static int start_adc_capture(int fd)
{
    struct timespec settle = {0, 1000000L};

    if (adc_bar_write32(fd, ADC_SYNC_GPIO_BASE + XGPIO_DATA2_OFFSET, 0u) != 0)
        return -1;
    (void)nanosleep(&settle, NULL);
    if (adc_bar_write32(fd, ADC_SYNC_GPIO_BASE + XGPIO_DATA_OFFSET, 0u) != 0)
        return -1;
    (void)nanosleep(&settle, NULL);
    if (adc_bar_write32(fd, ADC_SYNC_GPIO_BASE + XGPIO_TRI_OFFSET, 0xFFu) != 0)
        return -1;
    (void)nanosleep(&settle, NULL);
    return 0;
}

/*
 * 配置四颗 AD4134，并严格等待 PLL 状态达到 0x05。
 * 0x04 表示缺少有效 ODR/DCLK，不能作为成功状态；因此调用本函数前，
 * 7022 必须已通过 CTRL[0] 向 PXI 输出 48 kHz ODR。
 */
static int initialize_adc_devices(int fd)
{
    static const uint32_t bases[ADC_SPI_COUNT] = {
                                                  0x00020000u, 0x00070000u, 0x00080000u, 0x00090000u
    };
    struct timespec settle = {0, 1000000L};
    unsigned adc;

    for (adc = 0; adc < ADC_SPI_COUNT; ++adc) {
        uint8_t config, config1, pll = 0u;
        uint32_t base = bases[adc];
        unsigned pll_wait_ms;

        if (adc_spi_write_reg(fd, base, 0x00u, 0x98u) != 0) return -1;
        (void)nanosleep(&settle, NULL);
        if (adc_spi_read_reg(fd, base, 0x02u, &config) != 0) return -1;
        if (adc_spi_write_reg(fd, base, 0x02u, (uint8_t)(config | 0x01u)) != 0) return -1;
        if (adc_spi_read_reg(fd, base, 0x10u, &config1) != 0) return -1;
        if (adc_spi_write_reg(fd, base, 0x10u, (uint8_t)(config1 | 0x02u)) != 0 ||
                adc_spi_write_reg(fd, base, 0x11u, 0x20u) != 0 ||
                adc_spi_write_reg(fd, base, 0x01u, 0x82u) != 0 ||
                adc_spi_write_reg(fd, base, 0x24u, 0x12u) != 0) {
            return -1;
        }
        for (pll_wait_ms = 0u; pll_wait_ms < 500u; ++pll_wait_ms) {
            if (adc_spi_read_reg(fd, base, 0x15u, &pll) != 0) return -1;
            if (pll == 0x05u) break;
            if (sleep_ms(1u) != 0) return -1;
        }
        printf("ADC%u PLL status = 0x%02X %s (waited %u ms)\n",
               adc, pll, pll == 0x05u ? "PASS" : "FAIL", pll_wait_ms);
        if (pll != 0x05u) {
            fprintf(stderr,
                    "ERROR: ADC%u PLL did not lock; verify PXI 48 kHz ODR "
                    "routing and 7022 CTRL[0]\n", adc);
            return -1;
        }
    }

    return 0;
}

/*
 * TLAST 计数采用“帧数减一”。48 帧控制窗口必须写入并回读 47，
 * 对应一次 C2H 传输 48 × 64 = 3072 字节。
 */
static int configure_adc_tlast(int adc_user_fd, unsigned frames)
{
    uint32_t tlast_count = frames - 1u;
    uint32_t readback = 0;
    ssize_t n = pwrite(adc_user_fd, &tlast_count, sizeof(tlast_count),
                       (off_t)ADC_REG_TLAST);

    if (n != (ssize_t)sizeof(tlast_count)) {
        if (n >= 0) errno = EIO;
        fprintf(stderr, "ERROR: configure ADC TLAST at 0x%08X: %s\n",
                ADC_REG_TLAST, strerror(errno));
        return -1;
    }
    n = pread(adc_user_fd, &readback, sizeof(readback),
              (off_t)ADC_REG_TLAST);
    if (n != (ssize_t)sizeof(readback) || readback != tlast_count) {
        if (n >= 0 && n != (ssize_t)sizeof(readback)) errno = EIO;
        fprintf(stderr,
                "ERROR: ADC TLAST readback expected=%" PRIu32
                " actual=%" PRIu32 "%s%s\n",
                tlast_count, readback,
                n == (ssize_t)sizeof(readback) ? "" : ": ",
                        n == (ssize_t)sizeof(readback) ? "" : strerror(errno));
        return -1;
    }
    printf("ADC TLAST count = %" PRIu32 " verified\n", readback);
    return 0;
}

int read_exact_timeout(int fd, void *buffer, size_t size, int timeout_ms)
{
    uint8_t *cursor = buffer;
    size_t completed = 0;
    struct timespec start;
    int64_t timeout_ns = (int64_t)timeout_ms * 1000000LL;

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        fprintf(stderr, "ERROR: clock_gettime: %s\n", strerror(errno));
        return -1;
    }

    while (completed < size && !stop_requested) {
        struct pollfd poll_fd;
        struct timespec now;
        int64_t elapsed_ns;
        int64_t remaining_ns;
        int poll_timeout_ms;
        ssize_t n;
        int rc;

        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
            fprintf(stderr, "ERROR: clock_gettime: %s\n", strerror(errno));
            return -1;
        }
        elapsed_ns = ((int64_t)now.tv_sec - (int64_t)start.tv_sec) * 1000000000LL +
                (int64_t)now.tv_nsec - (int64_t)start.tv_nsec;
        remaining_ns = timeout_ns - elapsed_ns;
        if (remaining_ns <= 0) {
            fprintf(stderr, "ERROR: ADC C2H timeout after %d ms (%zu/%zu bytes)\n",
                    timeout_ms, completed, size);
            return -1;
        }
        poll_timeout_ms = (int)((remaining_ns + 999999LL) / 1000000LL);

        poll_fd.fd = fd;
        poll_fd.events = POLLIN;
        poll_fd.revents = 0;
        rc = poll(&poll_fd, 1, poll_timeout_ms);
        if (rc == 0) {
            fprintf(stderr, "ERROR: ADC C2H timeout after %d ms (%zu/%zu bytes)\n",
                    timeout_ms, completed, size);
            return -1;
        }
        if (rc < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "ERROR: poll ADC C2H: %s\n", strerror(errno));
            return -1;
        }
        n = read(fd, cursor + completed, size - completed);
        if (n > 0) {
            completed += (size_t)n;
        } else if (n == 0) {
            fprintf(stderr, "ERROR: ADC C2H returned EOF\n");
            return -1;
        } else if (errno == EINTR || errno == EAGAIN || errno == 512) {
            if (errno == 512) {
                struct timespec retry_delay = {0, 1000000L};
                (void)nanosleep(&retry_delay, NULL);
            }
        } else {
            fprintf(stderr, "ERROR: read ADC C2H: %s (errno=%d)\n",
                    strerror(errno), errno);
            return -1;
        }
    }
    return completed == size ? 0 : -1;
}



struct reg_pair {
    uint8_t reg;
    uint8_t value;
};

static const struct reg_pair si5319_config_24m576[] = {
                                                       {  0, 0x14}, {  2, 0x82}, {  3, 0x15}, {  5, 0xED},
                                                       {  6, 0x2A}, {  8, 0x00}, { 10, 0x00}, { 11, 0x40},
                                                       { 19, 0x2C}, { 20, 0x3E}, { 22, 0xDF}, { 23, 0x1F},
                                                       { 24, 0x3F}, { 25, 0x20}, { 31, 0x00}, { 32, 0x00},
                                                       { 33, 0x27}, { 40, 0x40}, { 41, 0x07}, { 42, 0xFF},
                                                       { 43, 0x00}, { 44, 0x00}, { 45, 0x18}, { 46, 0x00},
                                                       { 47, 0x00}, { 48, 0x18}, {131, 0x1F}, {132, 0x02},
                                                       {138, 0x0F}, {139, 0xFF}, {136, 0x40}
};

/* ======================== 控制周期、算法和记录结构 ========================
 *
 * 当前固定为 48 kHz / 48 帧，即约 1 kHz 控制更新率。
 * 若软件组改变窗口长度，必须同时核对 TLAST、C2H 字节数、算法 dt 和 DA 更新率。
 */
#define SI5319_ADDR 0x68u
#define DA_IIC_BASE 0x00030000u
#define ADC_FRAME_BYTES 64u
#define FRAMES_PER_TICK 48u
#define ADC_WINDOW_BYTES (ADC_FRAME_BYTES * FRAMES_PER_TICK)
#define FEEDBACK_FRAME_CHANNEL 2u
#define DEFAULT_OUTPUT_LIMIT 1024
#define DEFAULT_STATUS_EVERY 0u
#define LOOP_LOG_CAPACITY 4096u


/*
 * 控制器跨周期状态。软件组实现 PI/PID、滤波器、观测器或状态机时，
 * 可在此增加状态，禁止在实时循环内动态分配内存。
 */
struct pid_state {
    double integral;
    double previous_error;
    int has_previous;
};

/*
 * control_step() 的标量输入。
 * feedback 当前是 48 点算术均值；任一端为 AC 耦合时，正式控制应改为
 * 合适的交流特征；J9/J2 均为 DC 时才可评估非零直流 setpoint。
 * 若算法需要 RMS、同相/正交分量、参考相位或滤波结果，可扩展本结构。
 */
struct control_input {
    uint64_t tick;
    double feedback;
    double dt_sec;
};

struct control_output {
    double error;
    double p_term;
    double i_term;
    double d_term;
    double unclamped;
    double command;
    int saturated;
};

/*
 * 每周期轻量时间戳记录。实时循环仅写内存，不写 CSV；后续延时测试可在
 * 退出后导出并结合 48 kHz 原始样本做阶跃边沿检测或互相关分析。
 */
struct loop_sample {
    uint64_t tick;
    uint64_t c2h_start_ns;
    uint64_t c2h_done_ns;
    uint64_t control_done_ns;
    uint64_t h2c_done_ns;
    double feedback;
    int16_t command;
    uint32_t flags;
};

struct loop_stats {
    uint64_t ticks;
    uint64_t deadline_misses;
    uint64_t saturated;
    double minimum_us;
    double maximum_us;
    double sum_us;
};

static uint64_t monotonic_ns(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0u;
    return (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
}

/* ======================= Si5319 24.576 MHz 时钟层 =======================
 *
 * Si5319 不能复用 PCA9555 的 repeated-start 组合读取事务。本实现严格沿用
 * 已实机验证的 si5319_init.c：先发送寄存器地址并 STOP，再单独读取 1 字节；
 * reset 后还必须将 AXI-IIC GPO 写为 1。
 */
static int si_iic_wait_mask(uint32_t offset, uint32_t mask, int want_set,
                            unsigned timeout_ms, const char *description)
{
    struct timespec deadline;
    uint32_t value = 0u;

    if (monotonic_deadline_ms(timeout_ms, &deadline) != 0) return -1;
    for (;;) {
        if (iic_read32(da_user_fd, DA_IIC_BASE, offset, &value) != 0)
            return -1;
        if (((value & mask) != 0u) == want_set) return 0;
        if (deadline_expired(&deadline)) {
            fprintf(stderr,
                    "ERROR: timeout waiting for Si5319 I2C %s "
                    "(reg=0x%03" PRIX32 ", value=0x%08" PRIX32 ")\n",
                    description, offset, value);
            return -1;
        }
        if (stop_requested || sleep_ms(1u) != 0) return -1;
    }
}

static int si_iic_clear_status(uint32_t mask)
{
    uint32_t current;

    if (iic_read32(da_user_fd, DA_IIC_BASE, XIIC_IISR_OFFSET,
                   &current) != 0) return -1;
    return iic_write32(da_user_fd, DA_IIC_BASE, XIIC_IISR_OFFSET,
                       current & mask);
}

static int si_iic_reset(void)
{
    uint32_t status;
    uint32_t interrupts;
    uint32_t gpo;

    if (iic_write32(da_user_fd, DA_IIC_BASE, XIIC_RESETR_OFFSET,
                    XIIC_RESET_MASK) != 0 ||
            sleep_ms(1u) != 0 ||
            iic_write32(da_user_fd, DA_IIC_BASE, XIIC_RFD_OFFSET,
                        XIIC_RX_FIFO_DEPTH - 1u) != 0 ||
                        iic_write32(da_user_fd, DA_IIC_BASE, XIIC_CR_OFFSET,
                                    XIIC_CR_TX_FIFO_RESET_MASK) != 0 ||
                                    iic_write32(da_user_fd, DA_IIC_BASE, XIIC_CR_OFFSET,
                                                XIIC_CR_ENABLE_MASK) != 0 ||
                                                iic_write32(da_user_fd, DA_IIC_BASE, XIIC_GPO_OFFSET, 1u) != 0 ||
                                                iic_read32(da_user_fd, DA_IIC_BASE, XIIC_SR_OFFSET,
                                                           &status) != 0 ||
                                                           iic_read32(da_user_fd, DA_IIC_BASE, XIIC_IISR_OFFSET,
                                                                      &interrupts) != 0 ||
                                                                      iic_read32(da_user_fd, DA_IIC_BASE, XIIC_GPO_OFFSET,
                                                                                 &gpo) != 0) return -1;

    printf("Si5319 AXI IIC reset: SR=0x%08" PRIX32
           " IISR=0x%08" PRIX32 " GPO=0x%08" PRIX32 "\n",
           status, interrupts, gpo);
    return 0;
}

static int si_iic_wait_tx_event(unsigned timeout_ms)
{
    struct timespec deadline;
    uint32_t interrupts = 0u;
    uint32_t status = 0u;

    if (monotonic_deadline_ms(timeout_ms, &deadline) != 0) return -1;
    for (;;) {
        if (iic_read32(da_user_fd, DA_IIC_BASE, XIIC_IISR_OFFSET,
                       &interrupts) != 0 ||
                iic_read32(da_user_fd, DA_IIC_BASE, XIIC_SR_OFFSET,
                           &status) != 0) return -1;
        if ((interrupts & XIIC_INTR_TX_EMPTY_MASK) != 0u) return 0;
        if ((interrupts & (XIIC_INTR_ARB_LOST_MASK |
                XIIC_INTR_TX_ERROR_MASK)) != 0u)
            return 1;
        if ((interrupts & XIIC_INTR_BNB_MASK) != 0u &&
                (status & XIIC_SR_BUS_BUSY_MASK) == 0u)
            return 1;
        if (deadline_expired(&deadline)) {
            fprintf(stderr,
                    "ERROR: timeout waiting for Si5319 I2C TX empty: "
                    "IISR=0x%08" PRIX32 " SR=0x%08" PRIX32 "\n",
                    interrupts, status);
            return -1;
        }
        if (stop_requested || sleep_ms(1u) != 0) return -1;
    }
}

static int si_iic_send(uint8_t address, const uint8_t *data, size_t count)
{
    size_t index;
    uint32_t interrupts;

    if (count == 0u || count > 255u) {
        errno = EINVAL;
        return -1;
    }
    if (si_iic_wait_mask(XIIC_SR_OFFSET, XIIC_SR_BUS_BUSY_MASK, 0,
                         1000u, "bus free") != 0 ||
            si_iic_clear_status(XIIC_INTR_TX_EMPTY_MASK |
                                XIIC_INTR_TX_ERROR_MASK |
                                XIIC_INTR_ARB_LOST_MASK |
                                XIIC_INTR_BNB_MASK) != 0 ||
                                iic_write32(da_user_fd, DA_IIC_BASE, XIIC_DTR_OFFSET,
                                            XIIC_TX_DYN_START_MASK | ((uint32_t)address << 1)) != 0 ||
                                            si_iic_clear_status(XIIC_INTR_BNB_MASK) != 0) return -1;

    for (index = 0u; index < count; ++index) {
        int event = si_iic_wait_tx_event(1000u);
        uint32_t word = data[index];

        if (event != 0)
        {
            printf("event !=0 %d \n",event);
            return event > 0 ? 0 : -1;
        }
        if (index + 1u == count) word |= XIIC_TX_DYN_STOP_MASK;
        if (si_iic_clear_status(XIIC_INTR_TX_EMPTY_MASK) != 0 ||
                iic_write32(da_user_fd, DA_IIC_BASE, XIIC_DTR_OFFSET,
                            word) != 0) return -1;
    }

    if (si_iic_wait_mask(XIIC_SR_OFFSET, XIIC_SR_BUS_BUSY_MASK, 0,
                         1000u, "STOP") != 0 ||
            iic_read32(da_user_fd, DA_IIC_BASE, XIIC_IISR_OFFSET,
                       &interrupts) != 0) return -1;
    if ((interrupts & (XIIC_INTR_ARB_LOST_MASK |
            XIIC_INTR_TX_ERROR_MASK)) != 0u)
    {
        printf("erroe! %x %d\n",interrupts,da_user_fd);
        return 0;
    }
    return (int)count;
}

static int si_iic_recv_one(uint8_t address, uint8_t *data)
{
    struct timespec deadline;
    uint32_t interrupts = 0u;
    uint32_t status = 0u;
    uint32_t value;

    if (si_iic_wait_mask(XIIC_SR_OFFSET, XIIC_SR_BUS_BUSY_MASK, 0,
                         1000u, "bus free") != 0 ||
            si_iic_clear_status(XIIC_INTR_TX_EMPTY_MASK |
                                XIIC_INTR_TX_ERROR_MASK |
                                XIIC_INTR_ARB_LOST_MASK |
                                XIIC_INTR_BNB_MASK) != 0 ||
                                iic_write32(da_user_fd, DA_IIC_BASE, XIIC_DTR_OFFSET,
                                            XIIC_TX_DYN_START_MASK |
                                            ((uint32_t)address << 1) | XIIC_READ_OPERATION) != 0 ||
                                            si_iic_clear_status(XIIC_INTR_BNB_MASK) != 0 ||
                                            iic_write32(da_user_fd, DA_IIC_BASE, XIIC_DTR_OFFSET,
                                                        XIIC_TX_DYN_STOP_MASK | 1u) != 0 ||
                                                        monotonic_deadline_ms(1000u, &deadline) != 0) return -1;

    for (;;) {
        if (iic_read32(da_user_fd, DA_IIC_BASE, XIIC_SR_OFFSET,
                       &status) != 0 ||
                iic_read32(da_user_fd, DA_IIC_BASE, XIIC_IISR_OFFSET,
                           &interrupts) != 0) return -1;
        if ((status & XIIC_SR_RX_FIFO_EMPTY_MASK) == 0u) {
            if (iic_read32(da_user_fd, DA_IIC_BASE, XIIC_DRR_OFFSET,
                           &value) != 0 ||
                    si_iic_wait_mask(XIIC_SR_OFFSET,
                                     XIIC_SR_BUS_BUSY_MASK, 0,
                                     1000u, "read STOP") != 0) return -1;
            *data = (uint8_t)value;
            return 1;
        }
        if ((interrupts & XIIC_INTR_ARB_LOST_MASK) != 0u) return 0;
        if ((interrupts & XIIC_INTR_TX_ERROR_MASK) != 0u &&
                (interrupts & XIIC_INTR_BNB_MASK) == 0u) return 0;
        if (deadline_expired(&deadline)) {
            fprintf(stderr,
                    "ERROR: timeout waiting for Si5319 I2C receive byte\n");
            return -1;
        }
        if (stop_requested || sleep_ms(1u) != 0) return -1;
    }
}

static int si5319_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t bytes[2] = {reg, value};
    int sent = si_iic_send(SI5319_ADDR, bytes, sizeof(bytes));

    return sent == (int)sizeof(bytes) ? 0 : -1;
}

static int si5319_read_reg(uint8_t reg, uint8_t *value)
{
    int sent = si_iic_send(SI5319_ADDR, &reg, 1u);
    int received;

    if (sent != 1)
    {
        printf("send error! %d\n",sent);
        return -1;
    }
    received = si_iic_recv_one(SI5319_ADDR, value);
    return received == 1 ? 0 : -1;
}

/*
 * 完成 Si5319 配置、PLL 状态和 MCLK_ALIVE 检查。
 * MCLK_ALIVE 预期约为 24.576 MHz / 256 = 96000 counts/s。
 */
#define SERVO_CTRL_OFFSET       0x00000000u
static int initialize_si5319_24m576(void)
{
    uint8_t value;
    uint32_t first;
    uint32_t second;
    uint32_t delta;
    size_t index;
    const double expected = 24576000.0 / 256.0;
    double deviation;

    if (reg_write(SERVO_CTRL_OFFSET, 0u) != 0)
    {
        printf("Befor !Initializing Si5319 profile 24m576...\n");
    }
    printf("Initializing Si5319 profile 24m576...\n");
    if (si_iic_reset() != 0 )
    {
        printf("ERROR: iic_reset\n");
        return -1;
    }

    sleep_ms(1000);
    if(    si5319_read_reg(130u, &value) != 0)
    {
        fprintf(stderr,
                "ERROR: no valid response from Si5319 at 7-bit address 0x68\n");
        return -1;
    }

    printf("Si5319 register 130 initially = 0x%02X\n", value);

    for (index = 0u;
            index < sizeof(si5319_config_24m576) / sizeof(si5319_config_24m576[0]);
            ++index) {
        if (si5319_write_reg(si5319_config_24m576[index].reg,
                             si5319_config_24m576[index].value) != 0) {
            fprintf(stderr, "ERROR: Si5319 config failed at entry %zu\n", index);
            return -1;
        }
    }

    sleep_ms(1000);
    if (si5319_read_reg(130u, &value) != 0)
        return -1;
    printf("Si5319 register 130 = 0x%02X\n", value);
    if (value != 0u) {
        fprintf(stderr, "ERROR: Si5319 PLL is not locked\n");
        return -1;
    }

    if (reg_read(REG_MCLK_ALIVE, &first) != 0 || sleep_ms(1000u) != 0 ||
            reg_read(REG_MCLK_ALIVE, &second) != 0) return -1;
    delta = second - first;
    deviation = fabs((double)delta - expected) / expected;
    printf("MCLK_ALIVE delta=%" PRIu32 "/s, expected %.1f/s\n",
           delta, expected);
    if (delta == 0u || deviation > 0.05) {
        fprintf(stderr, "ERROR: 24.576 MHz MCLK validation failed\n");
        return -1;
    }
    return 0;
}

/*
 * 校验一个 1 ms ADC 窗口的四个 marker，并提取物理 J2/frame channel 2。
 *
 * 当前输出 feedback 为 48 个 signed 24-bit 原始码的算术均值，仅用于验证
 * 采集链路和标量控制接口。任一端为 AC 耦合时，非零直流均值会随时间
 * 衰减；仅在 J9/J2 均为 DC 且确认零偏安全后，才可评估直流均值控制。
 *
 * 软件组可在此处实现或调用特征提取：
 *   - 去直流后的 RMS / 峰峰值 / 包络；
 *   - 与参考波形同步的 I/Q 解调；
 *   - 带通/低通滤波后的状态量；
 *   - 48 点参考波形与采样波形的逐点误差。
 * 若算法需要完整原始窗口，也可扩展 control_input，而不是修改底层帧解析规则。
 */
static int adc_window_feedback(const uint8_t *window, double *feedback,
                               int32_t *minimum, int32_t *maximum)
{
    int64_t sum = 0;
    unsigned frame;

    *minimum = INT32_MAX;
    *maximum = INT32_MIN;
    for (frame = 0u; frame < FRAMES_PER_TICK; ++frame) {
        const uint8_t *frame_data = window + (size_t)frame * ADC_FRAME_BYTES;
        unsigned group;
        int32_t raw;

        for (group = 0u; group < 4u; ++group) {
            size_t offset = (size_t)group * 16u;
            if (frame_data[offset] != 0xFFu ||
                    frame_data[offset + 1u] != 0xFFu ||
                    frame_data[offset + 2u] != 0xFFu ||
                    frame_data[offset + 3u] != 0xFFu) {
                fprintf(stderr,
                        "ERROR: ADC frame %u group %u marker mismatch: "
                        "%02X%02X%02X%02X\n",
                        frame, group, frame_data[offset],
                        frame_data[offset + 1u], frame_data[offset + 2u],
                        frame_data[offset + 3u]);
                return -1;
            }
        }
        raw = packed_channel_code(frame_data, 16,
                                  FEEDBACK_FRAME_CHANNEL);
        if (raw < *minimum) *minimum = raw;
        if (raw > *maximum) *maximum = raw;
        sum += raw;
    }
    *feedback = (double)sum / (double)FRAMES_PER_TICK;
    return 0;
}

static void loop_stats_add(struct loop_stats *stats, double loop_us,
                           int saturated)
{
    if (stats->ticks == 0u || loop_us < stats->minimum_us)
        stats->minimum_us = loop_us;
    if (stats->ticks == 0u || loop_us > stats->maximum_us)
        stats->maximum_us = loop_us;
    stats->sum_us += loop_us;
    ++stats->ticks;
    if (loop_us > 1000.0) ++stats->deadline_misses;
    if (saturated) ++stats->saturated;
}

/*
 * 检查 DA 接收计数、协议错误、最后命令和 FIFO 状态。
 * 调试打印不应放在默认实时路径；status_every=0 时只在退出阶段调用。
 */
static int read_da_status(uint32_t expected_err, int require_zero)
{
    uint32_t rx_count;
    uint32_t err_count;
    uint32_t last_seq;
    uint32_t last_data;
    uint32_t status;

    if (reg_read(REG_H2C_RX_COUNT, &rx_count) != 0 ||
            reg_read(REG_H2C_ERR_COUNT, &err_count) != 0 ||
            reg_read(REG_H2C_LAST_SEQ, &last_seq) != 0 ||
            reg_read(REG_H2C_LAST_DATA, &last_data) != 0 ||
            reg_read(REG_STATUS, &status) != 0) return -1;
    printf("DA status: RX=%" PRIu32 " ERR=%" PRIu32
           " LAST_SEQ=%" PRIu32 " LAST_DATA=0x%08" PRIX32
           " STATUS=0x%08" PRIX32 "\n",
           rx_count, err_count, last_seq & 0xFFFFu, last_data, status);
    if (err_count != expected_err ||
            (status & (STATUS_FIFO_OVERFLOW | STATUS_FIFO_FULL)) != 0u ||
            (require_zero && last_data != 0u)) {
        fprintf(stderr, "ERROR: DA H2C/FIFO status check failed\n");
        return -1;
    }
    return 0;
}

/*
 * 统一安全退出：先按约 1 kHz 发送 zero_tail 个零命令，再关闭 DA 和 PXI 同步。
 * 即使收到 SIGINT/SIGTERM，清理专用延时也不会被 stop_requested 提前跳过。
 * --restore-ctrl 只恢复非运行配置位，绝不在退出时重新开启 DA/SYNC。
 */
static int safe_stop_da(unsigned zero_tail, int output_enabled,
                        uint32_t saved_ctrl, int restore_ctrl)
{
    unsigned index;
    int result = 0;

    if (output_enabled && da_h2c_fd >= 0) {
        for (index = 0u; index < zero_tail; ++index) {
            if (send_da_sample(0,0) != 0) {
                result = -1;
                break;
            }
            if (sleep_ms_cleanup(1u) != 0) result = -1;
        }
    }
    if (da_user_fd >= 0) {
        uint32_t final_ctrl = restore_ctrl ? saved_ctrl : 0u;
        final_ctrl &= ~(CTRL_CLEAR_STATUS | CTRL_RUN_WITH_PXI);
        if (reg_write(REG_CTRL, final_ctrl) != 0) {
            fprintf(stderr, "WARNING: failed to set final DA CTRL\n");
            result = -1;
        }
    }
    return result;
}
/*
 * ================================ 主程序时序 ================================
 *
 * 启动顺序不可随意调整：
 *   1. 打开 7022 BAR/H2C，关闭旧运行状态；
 *   2. 初始化 Si5319，并在 DA 关闭时配置 J9 耦合；
 *   3. 打开 7021 BAR，在采集停止时配置 J2 耦合并等待模拟前端稳定；
 *   4. CTRL=0x01，仅开启 PXI 48 kHz ODR，DA 保持关闭；
 *   5. 初始化四颗 AD4134，配置 TLAST=47，再启动 C2H；
 *   6. 仅在 --enable-output 时预装零包并切换 CTRL=0x03；
 *   7. 每 3072 字节 ADC 窗口调用一次算法并最多发送一个 DA 包；
 *   8. 退出时复位 ADC、发送 DA 零尾、关闭 DA/PXI；耦合保持本次选择。
 sudo ./demo_da_ad_pid_loop
 \ --da-coupling dc \ --adc-coupling dc \ --duration-sec 5 \ --setpoint 0 \ --kp 0 \ --output-min -64 \ --output-max 64 \ --status-every 100 \ --enable-output
 */

int adc_user_fd = -1;
int adc_fd = -1;
int read1()
{
    struct coupling_endpoint da_coupling_endpoint = {
                                                        "7022 J9", PCA9555_ADDR_7022_U15, J9_COUPLING_MASK
       };
       struct coupling_endpoint adc_coupling_endpoint = {
                                                         "7021 J2", PCA9555_ADDR_7021_U11, J2_COUPLING_MASK
       };
       enum coupling_mode da_coupling = COUPLING_DC;
       enum coupling_mode adc_coupling = COUPLING_DC;

       if (read_configure_coupling(da_user_fd, DA_IIC_BASE,&da_coupling_endpoint, da_coupling) != 0)
       {

       }

           if (   read_configure_coupling(adc_user_fd, ADC_IIC_BASE_DEFAULT, &adc_coupling_endpoint, adc_coupling) != 0)
           {

           }
}
int main1()
{
    const char *da_user_device = "/dev/xdma0_user";
    const char *da_h2c_device = "/dev/xdma0_h2c_0";
    const char *adc_user_device = "/dev/xdma1_user";
    const char *adc_device = "/dev/xdma1_c2h_0";
    struct coupling_endpoint da_coupling_endpoint = {
                                                     "7022 J9", PCA9555_ADDR_7022_U15, J9_COUPLING_MASK
    };
    struct coupling_endpoint adc_coupling_endpoint = {
                                                      "7021 J2", PCA9555_ADDR_7021_U11, J2_COUPLING_MASK
    };
    enum coupling_mode da_coupling = COUPLING_DC;
    enum coupling_mode adc_coupling = COUPLING_DC;


    struct loop_stats stats = {0};
    struct loop_sample loop_log[LOOP_LOG_CAPACITY];
    struct sigaction action;
    uint8_t *window = NULL;

    unsigned adc_gain = 1u;

    unsigned c2h_timeout_ms = 20u;
    unsigned status_every = DEFAULT_STATUS_EVERY;
    unsigned zero_tail = 10;
    uint32_t saved_ctrl = 0u;
    uint32_t err_before = 0u;
    uint32_t status = 0u;
    uint64_t tick = 0u;
    uint64_t tick_limit;
    int saved_ctrl_valid = 0;
    int da_status_valid = 0;
    int adc_capture_started = 0;
    int enable_output = 1;
    int output_enabled = 0;
    int restore_ctrl = 0;
    int result = 1;


    (void)loop_log;


    memset(&action, 0, sizeof(action));
    action.sa_handler = request_stop;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) != 0 ||
            sigaction(SIGTERM, &action, NULL) != 0) {
        fprintf(stderr, "ERROR: install signal handlers: %s\n", strerror(errno));
        return 1;
    }

    tick_limit = 5000;


    da_user_fd = open(da_user_device, O_RDWR | O_SYNC);
    if (da_user_fd < 0) {
        fprintf(stderr, "ERROR: open %s: %s\n", da_user_device, strerror(errno));
        goto out;
    }

    // ????????????????   DA关闭
    if (reg_read(REG_CTRL, &saved_ctrl) != 0) goto out;
    saved_ctrl &= ~CTRL_CLEAR_STATUS;
    saved_ctrl_valid = 1;
    if (reg_write(REG_CTRL, 0u) != 0) goto out;

    da_h2c_fd = open(da_h2c_device, O_WRONLY);
    if (da_h2c_fd < 0) {
        fprintf(stderr, "ERROR: open %s: %s\n", da_h2c_device, strerror(errno));
        goto out;
    }


    //初始化48K
    if (initialize_si5319_24m576() != 0) goto out;

    if (pca9555_write_reg(da_user_fd, DA_IIC_BASE, PCA9555_ADDR_7022_U15, 0x02, 0x00, 1000) != 0 ||
        pca9555_write_reg(da_user_fd, DA_IIC_BASE, PCA9555_ADDR_7022_U15, 0x03, 0x00, 1000) != 0 ||
        pca9555_write_reg(da_user_fd, DA_IIC_BASE, PCA9555_ADDR_7022_U15, 0x06, 0x00, 1000) != 0 ||
        pca9555_write_reg(da_user_fd, DA_IIC_BASE, PCA9555_ADDR_7022_U15, 0x07, 0x00, 1000) != 0)
        goto out;
    //写dc配置
    if (configure_coupling(da_user_fd, DA_IIC_BASE,
                           &da_coupling_endpoint, da_coupling) != 0)
        goto out;

    adc_user_fd = open(adc_user_device, O_RDWR | O_SYNC);
    if (adc_user_fd < 0) {
        fprintf(stderr, "ERROR: open %s: %s\n", adc_user_device, strerror(errno));
        goto out;
    }

    //写dc配置
//    if (iic_init(adc_user_fd, ADC_IIC_BASE_DEFAULT) != 0 ||
//            configure_coupling(adc_user_fd, ADC_IIC_BASE_DEFAULT,
//                               &adc_coupling_endpoint, adc_coupling) != 0)
//        goto out;
    if (iic_init(adc_user_fd, ADC_IIC_BASE_DEFAULT) != 0)
         goto out;

    if (configure_adc_j2_dc(adc_user_fd, ADC_IIC_BASE_DEFAULT) != 0)
        goto out;

    if (sleep_ms(COUPLING_SETTLE_MS) != 0) goto out;

    //??????????????
    if (reg_write(REG_CTRL, CTRL_CLEAR_STATUS) != 0 ||
            reg_write(REG_CTRL, CTRL_SYNC_ENABLE) != 0 ||
            reg_read(REG_CTRL, &status) != 0 ||
            (status & CTRL_RUN_WITH_PXI) != CTRL_SYNC_ENABLE) {
        fprintf(stderr,
                "ERROR: PXI synchronization control did not latch: "
                "CTRL=0x%08" PRIX32 "\n", status);
        goto out;
    }
    printf("PXI synchronization enabled: CTRL=0x%08" PRIX32
           " (DA output disabled)\n", status);

    //??????????
    if (sleep_ms(100) != 0 ||
            reg_read(REG_BOARD_ID, &status) != 0) goto out;
    da_board_id = (uint8_t)(status & 0xFFu);
    if (reg_read(REG_H2C_ERR_COUNT, &err_before) != 0 ||
            reg_read(REG_STATUS, &status) != 0) goto out;
    da_status_valid = 1;
    if ((status & (STATUS_FIFO_OVERFLOW | STATUS_FIFO_FULL)) != 0u) {
        fprintf(stderr, "ERROR: DA FIFO is not ready, status=0x%08" PRIX32 "\n", status);
        goto out;
    }

    adc_fd = open(adc_device, O_RDONLY | O_NONBLOCK);
    if (adc_fd < 0) {
        fprintf(stderr, "ERROR: open %s: %s\n", adc_device, strerror(errno));
        goto out;
    }

    //配置
    if (initialize_adc_devices(adc_user_fd) != 0 ||
            prepare_adc_capture(adc_user_fd) != 0 ||
            configure_adc_tlast(adc_user_fd, FRAMES_PER_TICK) != 0 ||
            start_adc_capture(adc_user_fd) != 0) goto out;
    adc_capture_started = 1;
    //启用da

    uint32_t ctrl_readback = 0u;


 //   if (send_da_sample(0) != 0 ||
  if( reg_write(REG_CTRL, CTRL_RUN_WITH_PXI) != 0) {
        fprintf(stderr, "ERROR: DA/PXI run control write failed\n");
        goto out;
    }
    return 0;
    output_enabled = 1;
    if (reg_read(REG_CTRL, &ctrl_readback) != 0 ||
            (ctrl_readback & CTRL_RUN_WITH_PXI) != CTRL_RUN_WITH_PXI) {
        fprintf(stderr,
                "ERROR: DA/PXI run control did not latch: CTRL=0x%08" PRIX32 "\n",
                ctrl_readback);
        goto out;
    }

    return 0;
    /*
     * 实时主循环由 C2H 数据到达驱动，不额外 sleep(1 ms)。
     * 每轮固定读取 48 帧；算法和 H2C 写入必须在下一窗口到达前完成。
     */
//    while (!stop_requested && tick < tick_limit) {
//        struct control_input input;
//        struct control_output output;
//        struct loop_sample *sample = &loop_log[tick % LOOP_LOG_CAPACITY];
//        int32_t minimum;
//        int32_t maximum;
//        int16_t command = 0;
//        uint64_t start_ns = monotonic_ns();
//        uint64_t done_ns;
//        double loop_us;
//
//        memset(sample, 0, sizeof(*sample));
//        sample->tick = tick;
//        sample->c2h_start_ns = start_ns;
//
//
//        //这里好像是计算平均？
//        /* 先校验整帧 marker，再提取 J2 特征；禁止绕过 marker 检查直接用数据。 */
//        if (adc_window_feedback(window, &input.feedback,
//                                &minimum, &maximum) != 0) goto out;
//        sample->c2h_done_ns = monotonic_ns();
//
//        input.tick = tick;
//        input.dt_sec = 0.001;
//
//        sample->control_done_ns = monotonic_ns();
//        /*
//         * dry-run 仍执行算法，但不写 H2C；只有 --enable-output 才允许命令离开主机。
//         * lround() 将算法浮点输出量化为 int16 DA code，正式算法需考虑该量化效应。
//         */
//        if (enable_output) command = (int16_t)lround(output.command);
//        if (enable_output && send_da_sample(command) != 0) goto out;
//        sample->h2c_done_ns = done_ns = monotonic_ns();
//        sample->feedback = input.feedback;
//        sample->command = command;
//        loop_us = (double)(done_ns - start_ns) / 1000.0;
//        loop_stats_add(&stats, loop_us, output.saturated);
//
//        if (status_every != 0u && tick != 0u &&
//                tick % status_every == 0u) {
//            printf("tick=%" PRIu64 " feedback=%.3f (%.6f V est) "
//                   "range=[%" PRId32 ",%" PRId32 "] error=%.3f "
//                   "command=%d loop=%.3f us\n",
//                   tick, input.feedback,
//                   adc_code_to_volts((int32_t)lround(input.feedback), adc_gain),
//                   minimum, maximum, output.error, command, loop_us);
//            //??
//            if (read_da_status(err_before, 0) != 0) goto out;
//        }
//        ++tick;
//    }

    result = 0;

    out:
    if (adc_capture_started && adc_user_fd >= 0 &&
            prepare_adc_capture(adc_user_fd) != 0) {
        fprintf(stderr, "WARNING: failed to reset ADC capture outputs\n");
        if (result == 0) result = 1;
    }
    if (safe_stop_da(zero_tail, output_enabled, saved_ctrl,
                     restore_ctrl && saved_ctrl_valid) != 0 && result == 0)
        result = 1;
    if (da_status_valid && da_user_fd >= 0 && da_h2c_fd >= 0 &&
            read_da_status(err_before, output_enabled) != 0 && result == 0)
        result = 1;
    if (stats.ticks > 0u) {
        printf("Loop summary: ticks=%" PRIu64 " avg=%.3f us min=%.3f us "
               "max=%.3f us >1ms=%" PRIu64 " saturated=%" PRIu64 "\n",
               stats.ticks, stats.sum_us / (double)stats.ticks,
               stats.minimum_us, stats.maximum_us,
               stats.deadline_misses, stats.saturated);
    }
    free(window);
    if (adc_fd >= 0) close(adc_fd);
    if (adc_user_fd >= 0) close(adc_user_fd);
    if (da_h2c_fd >= 0) close(da_h2c_fd);
    if (da_user_fd >= 0) close(da_user_fd);
    return result;
}
