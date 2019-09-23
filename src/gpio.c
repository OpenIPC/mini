#include "gpio.h"

#include <stdint.h>
#include <stdbool.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>

enum PIN_DIRECTION{
    INPUT,
    OUTPUT
};

struct Pin* find_pin(const uint8_t port, const uint8_t pin) {
    for (int i = 0; i < get_pins_hi3518EV200_size(); ++i) {
        struct Pin* p = &get_pins_hi3518EV200()[i];
        if (p->port == port && p->pin == pin) {
            return p;
        }
    }
    return 0;
}

bool pin_linux_to_port_pin(const uint8_t pin_number, uint8_t *port, uint8_t *pin) {
    *port = pin_number / 8;
    *pin = pin_number - (*port) * 8;
    return true;
}

bool set_u8(const uint32_t offset, const uint8_t value) {
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) { printf("set_u8 can't open /dev/mem \n"); return false; }
    volatile char *mmap_ptr = mmap(
            offset,                // Any adddress in our space will do
            1,                     // Map length
            PROT_WRITE,            // Enable reading & writting to mapped memory
            MAP_SHARED,            // Shared with other processes
            mem_fd,                // File to map
            0                      // Offset to base address
    );
    if (mmap_ptr == MAP_FAILED) {
        printf("set_u8 mmap_ptr mmap error %d\n", (int)mmap_ptr);
        printf("set_u8 Error: %s (%d)\n", strerror(errno), errno);
        close(mem_fd);
        return false;
    }
    mmap_ptr[0] = value;
    close(mem_fd);
    return true;
}

bool get_u8(const uint32_t offset, uint8_t *value) {
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) { printf("get_u8 can't open /dev/mem \n"); return false; }
    volatile char *mmap_ptr = mmap(
            offset,                // Any adddress in our space will do
            1,                     // Map length
            PROT_READ,             // Enable reading & writting to mapped memory
            MAP_SHARED,            // Shared with other processes
            mem_fd,                // File to map
            0                      // Offset to base address
    );
    if (mmap_ptr == MAP_FAILED) {
        printf("get_u8 mmap_ptr mmap error %d\n", (int)mmap_ptr);
        printf("get_u8 Error: %s (%d)\n", strerror(errno), errno);
        close(mem_fd);
        return false;
    }
    *value = mmap_ptr[0];
    close(mem_fd);
    return true;
}

bool set_u32(const uint32_t offset, const uint32_t value) {
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) { printf("set_u32 can't open /dev/mem \n"); return false; }
    volatile char *mmap_ptr = mmap(
            offset,              // Any adddress in our space will do
            4,                   // Map length
            PROT_WRITE,          // Enable reading & writting to mapped memory
            MAP_SHARED,          // Shared with other processes
            mem_fd,              // File to map
            0                    // Offset to base address
    );
    if (mmap_ptr == MAP_FAILED) {
        printf("set_u32 mmap_ptr mmap error %d\n", (int)mmap_ptr);
        printf("set_u32 Error: %s (%d)\n", strerror(errno), errno);
        close(mem_fd);
        return false;
    }
    mmap_ptr[0] = ((uint8_t *)&value)[0];
    mmap_ptr[1] = ((uint8_t *)&value)[1];
    mmap_ptr[2] = ((uint8_t *)&value)[2];
    mmap_ptr[3] = ((uint8_t *)&value)[3];
    close(mem_fd);
    return true;
}


bool get_u32(const uint32_t offset, uint32_t *value) {
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) { printf("get_u32 can't open /dev/mem \n"); return false; }
    volatile char *mmap_ptr = mmap(
            offset,                // Any adddress in our space will do
            4,                     // Map length
            PROT_READ,             // Enable reading & writting to mapped memory
            MAP_SHARED,            // Shared with other processes
            mem_fd,                // File to map
            0                      // Offset to base address
    );
    if (mmap_ptr == MAP_FAILED) {
        printf("get_u32 mmap_ptr mmap error %d\n", (int)mmap_ptr);
        printf("get_u32 Error: %s (%d)\n", strerror(errno), errno);
        close(mem_fd);
        return false;
    }
    uint32_t val = 0;
    val += mmap_ptr[0];
    val = val << 8;
    value += mmap_ptr[1];
    val = val << 8;
    val += mmap_ptr[2];
    val = val << 8;
    val += mmap_ptr[3];
    *value = val;
    close(mem_fd);
    return true;
}

bool get_bit(const uint8_t value, const uint8_t bit_pos) {
    const uint8_t bit = value >> bit_pos & 1;
    return bit == 1;
}

uint8_t set_bit(const uint8_t value, const uint8_t bit_pos, const bool bit) {
    if (bit_pos >= 8) { return value; }
    if (bit) { return value | (1 << bit_pos);} // set pin bit to 1
    else { return value & !(1 << bit_pos); } // set pin bit to 0
}

bool set_pin_dir(struct Pin *pin, const enum PIN_DIRECTION direction) {
    if (!set_u32(IOMUX + pin->muxctl, pin->e)) return false;

    const uint32_t gpio_port_addr = GPIO[pin->port];
    const uint32_t gpio_dir_addr = gpio_port_addr  + GPIO_DIR;
    enum PIN_DIRECTION gpio_port_direction;
    if(!get_u8(gpio_dir_addr, &gpio_port_direction)) return false;
    switch (direction) {
        case OUTPUT: { gpio_port_direction = set_bit(gpio_port_direction, pin->pin, true); break; } // set pin bit to 1 for output
        case INPUT: { gpio_port_direction = set_bit(gpio_port_direction, pin->pin, false); break; } // set pin bit to 0 for output
    }
    if (!set_u8(gpio_dir_addr, gpio_port_direction)) return false;
    return true;
}

bool get_pin(const uint8_t port, const uint8_t pin, bool *value) {
    struct Pin* p = find_pin(port, pin);
    if (!p) { return false; }

    if (!set_pin_dir(p, INPUT)) return false;

    uint32_t gpio_port_addr = GPIO[port];
    gpio_port_addr = gpio_port_addr + 0x03FC;
    uint8_t val;
    if(!get_u8(gpio_port_addr, &val)) return false;
    uint8_t bit = (val >> pin & 1);
    *value = (bit == 1);
    return true;
}

bool get_pin_linux(const uint8_t pin_number, bool *value) {
    uint8_t port, pin;
    if(!pin_linux_to_port_pin(pin_number, &port, &pin)) return false;
    if (!get_pin(port, pin, value)) return false;
    return true;
}

bool set_pin(struct Pin *pin, const bool bit) {
    if (!pin) return false;
    uint32_t gpio_port_addr = GPIO[pin->port];
    gpio_port_addr = gpio_port_addr + (1 << ( ((uint32_t)pin->pin) + 2));
    if (bit) {
        if(!set_u8(gpio_port_addr, 0xff)) return false;
    } else {
        if(!set_u8(gpio_port_addr, 0x00)) return false;
    }
    return true;
}

bool set_port_pin(const uint8_t port, const uint8_t pin, const bool bit) {
    struct Pin* p = find_pin(port, pin);
    if (!pin) return false;
    if(!set_pin_dir(p, OUTPUT)) return false;
    if(!set_pin(p, bit)) return false;
    return true;
}
bool set_pin_linux(const uint8_t pin_number, const bool bit) {
    uint8_t port, pin;
    if(!pin_linux_to_port_pin(pin_number, &port, &pin)) return false;
    if(!set_port_pin(port, pin, bit)) return false;
    return true;
}

// bit mask
// 0x0004 = 0000000100
// 0x0008 = 0000001000
// 0x0010 = 0000010000
// 0x0020 = 0000100000
// 0x0040 = 0001000000
// 0x0080 = 0010000000
// 0x0100 = 0100000000
// 0x0200 = 1000000000
// 0x03FC = 1111111100

const static struct Pin hi3518EV200_pins[] = {
    { .gpio = true, .port = 0, .pin = 6, .muxctl = 0x008, .e = 0b0, .desc = "muxctrl_reg02=008 000: GPIO0_6, 001: FLASH_TRIG, 010: SFC_EMMC_BOOT_MODE, 011: SPI1_CSN1, 100: VI_VS_BT1120"},
    { .gpio = true,  .port = 0, .pin = 5, .muxctl = 0x004, .e = 0b1, .desc = "muxctrl_reg01=004 0: SENSOR_RSTN, 1: GPIO0_5"},
    { .gpio = true,  .port = 0, .pin = 7, .muxctl = 0x00C, .e = 0b0, .desc = "muxctrl_reg03=00C 000: GPIO0_7, 001: SHUTTER_TRIG, 010: SFC_DEVICE_MODE, 100: VI_HS_BT1120"},
    { .gpio = false, .port = 2, .pin = 0, .muxctl = 0x010, .e = 0b0, .desc = "muxctrl_reg04=010 000: GPIO2_0, 001: RMII_CLK, 011: VO_CLK, 100: SDIO1_CCLK_OUT"},
    { .gpio = false, .port = 2, .pin = 1, .muxctl = 0x014, .e = 0b0, .desc = "muxctrl_reg05=014 000: GPIO2_1, 001: RMII_TX_EN, 011: VO_VS, 100: SDIO1_CARD_DETECT"},
    { .gpio = false, .port = 2, .pin = 2, .muxctl = 0x018, .e = 0b0, .desc = "muxctrl_reg06=018 000: GPIO2_2, 001: RMII_TXD0, 011: VO_DATA5, 100: SDIO1_CWPR"},
    { .gpio = false, .port = 2, .pin = 3, .muxctl = 0x01C, .e = 0b0, .desc = "muxctrl_reg07=01C 000: GPIO2_3, 001: RMII_TXD1, 011: VO_DE, 100: SDIO1_CDATA1"},
    { .gpio = false, .port = 2, .pin = 4, .muxctl = 0x020, .e = 0b0, .desc = "muxctrl_reg08=020 000: GPIO2_4, 001: RMII_RX_DV, 011: VO_DATA7, 100: SDIO1_CDATA0"},
    { .gpio = false, .port = 2, .pin = 5, .muxctl = 0x024, .e = 0b0, .desc = "muxctrl_reg09=024 000: GPIO2_5, 001: RMII_RXD0, 011: VO_DATA2, 100: SDIO1_CDATA3"},
    { .gpio = false, .port = 2, .pin = 6, .muxctl = 0x028, .e = 0b0, .desc = "muxctrl_reg10=028 000: GPIO2_6, 001: RMII_RXD1, 011: VO_DATA3, 100: SDIO1_CCMD"},
    { .gpio = false, .port = 2, .pin = 7, .muxctl = 0x02C, .e = 0b0, .desc = "muxctrl_reg11=02C 000: GPIO2_7, 001: EPHY_RST, 010: BOOT_SEL, 011: VO_HS, 100: SDIO1_CARD_POWER_EN"},
    { .gpio = true,  .port = 0, .pin = 3, .muxctl = 0x030, .e = 0b0, .desc = "muxctrl_reg12=030 00: GPIO0_3, 01: SPI1_CSN1, 11: VO_DATA0"},
    { .gpio = false, .port = 3, .pin = 0, .muxctl = 0x034, .e = 0b0, .desc = "muxctrl_reg13=034 000: GPIO3_0, 001: EPHY_CLK, 011: VO_DATA1, 100: SDIO1_CDATA2"},
//        //  Pin { gpio: false, port: 3, pin: 1, muxctl: 0x038, e: 0b0,  desc: "muxctrl_reg14=038 00: GPIO3_1, 01: MDCK, 10: BOOTROM_SEL, 11: VO_DATA6"},
//        //  Pin { gpio: false, port: 3, pin: 2, muxctl: 0x03C, e: 0b0,  desc: "muxctrl_reg15=03C 00: GPIO3_2, 01: MDIO, 11: VO_DATA4"},
    { .gpio = true, .port = 3, .pin = 3, .muxctl = 0x040, .e = 0b0, .desc = "muxctrl_reg16=040 00: GPIO3_3, 01: SPI0_SCLK, 10: I2C0_SCL"},
    { .gpio = true, .port = 3, .pin = 4, .muxctl = 0x044, .e = 0b0, .desc = "muxctrl_reg17=044 00: GPIO3_4, 01: SPI0_SDO, 10: I2C0_SDA"},
    { .gpio = true, .port = 3, .pin = 5, .muxctl = 0x048, .e = 0b0, .desc = "muxctrl_reg18=048 0: GPIO3_5, 1: SPI0_SDI"},
    { .gpio = true, .port = 3, .pin = 6, .muxctl = 0x04C, .e = 0b0, .desc = "muxctrl_reg19=04C 0: GPIO3_6, 1: SPI0_CSN"},
    { .gpio = true,  .port = 3, .pin = 7, .muxctl = 0x050, .e = 0b0, .desc = "muxctrl_reg20=050 00: GPIO3_7, 01: SPI1_SCLK, 10: I2C1_SCL"},
    { .gpio = true,  .port = 4, .pin = 0, .muxctl = 0x054, .e = 0b0, .desc = "muxctrl_reg21=054 00: GPIO4_0, 01: SPI1_SDO, 10: I2C1_SDA"},
    { .gpio = true,  .port = 4, .pin = 1, .muxctl = 0x058, .e = 0b0, .desc = "muxctrl_reg22=058 0: GPIO4_1, 1: SPI1_SDI"},
    { .gpio = true,  .port = 4, .pin = 2, .muxctl = 0x05C, .e = 0b0, .desc = "muxctrl_reg23=05C 0: GPIO4_2, 1: SPI1_CSN0"},
    { .gpio = true,  .port = 4, .pin = 3, .muxctl = 0x060, .e = 0b0, .desc = "muxctrl_reg24=060 0: GPIO4_3, 1: I2C2_SDA"},
    { .gpio = true,  .port = 4, .pin = 4, .muxctl = 0x064, .e = 0b0, .desc = "muxctrl_reg25=064 0: GPIO4_4, 1: I2C2_SCL"},
    { .gpio = true,  .port = 4, .pin = 5, .muxctl = 0x068, .e = 0b0, .desc = "muxctrl_reg26=068 0: GPIO4_5, 1: USB_OVRCUR"},
    { .gpio = true,  .port = 4, .pin = 6, .muxctl = 0x06C, .e = 0b0, .desc = "muxctrl_reg27=06C 0: GPIO4_6, 1: USB_PWREN"},
    { .gpio = true,  .port = 0, .pin = 0, .muxctl = 0x070, .e = 0b0, .desc = "muxctrl_reg28=070 000: GPIO0_0, 001: IR_IN, 010: TEMPER_DQ"},
    { .gpio = true,  .port = 0, .pin = 1, .muxctl = 0x074, .e = 0b0, .desc = "muxctrl_reg29=074 000: GPIO0_1, 010: TEMPER_DQ"},
    { .gpio = true,  .port = 0, .pin = 2, .muxctl = 0x078, .e = 0b0, .desc = "muxctrl_reg30=078 000: GPIO0_2, 010: TEMPER_DQ"},
    { .gpio = true,  .port = 1, .pin = 0, .muxctl = 0x07C, .e = 0b0, .desc = "muxctrl_reg31=07C 000: GPIO1_0, 001: VI_DATA13, 011: I2S_BCLK_TX, 100: PWM0"},
    { .gpio = true,  .port = 1, .pin = 1, .muxctl = 0x080, .e = 0b0, .desc = "muxctrl_reg32=080 000: GPIO1_1, 001: VI_DATA10, 011: I2S_SD_TX, 100: UART1_TXD"},
    { .gpio = true,  .port = 1, .pin = 2, .muxctl = 0x080, .e = 0b0, .desc = "muxctrl_reg33=080 000: GPIO1_2, 001: VI_DATA12, 011: I2S_MCLK, 100: UART1_CTSN"},
    { .gpio = true,  .port = 1, .pin = 3, .muxctl = 0x088, .e = 0b0, .desc = "muxctrl_reg34=088 000: GPIO1_3, 001: VI_DATA11, 011: I2S_WS_TX, 100: UART2_RXD"},
    { .gpio = true,  .port = 1, .pin = 4, .muxctl = 0x08C, .e = 0b0, .desc = "muxctrl_reg35=08C 000: GPIO1_4, 001: VI_DATA15, 010: VI_VS_SEN, 011: I2S_WS_RX, 100: UART1_RXD"},
    { .gpio = true,  .port = 1, .pin = 5, .muxctl = 0x090, .e = 0b0, .desc = "muxctrl_reg36=090 000: GPIO1_5, 001: VI_DATA14, 010: VI_HS_SEN, 011: I2S_BCLK_RX, 100: UART1_RTSN"},
    { .gpio = true,  .port = 1, .pin = 6, .muxctl = 0x094, .e = 0b0, .desc = "muxctrl_reg37=094 000: GPIO1_6, 001: VI_DATA9, 011: I2S_SD_RX, 100: UART2_TXD"},
    { .gpio = true,  .port = 1, .pin = 7, .muxctl = 0x098, .e = 0b0, .desc = "muxctrl_reg38=098 000: GPIO1_7, 001: SDIO0_CARD_POWER_EN"},
    { .gpio = true,  .port = 4, .pin = 7, .muxctl = 0x09C, .e = 0b0, .desc = "muxctrl_reg39=09C 0: GPIO4_7, 1: SDIO0_CARD_DETECT"},
    { .gpio = true,  .port = 5, .pin = 0, .muxctl = 0x0A0, .e = 0b0, .desc = "muxctrl_reg40=0A0 0: GPIO5_0, 1: SDIO0_CWPR"},
    { .gpio = true,  .port = 5, .pin = 1, .muxctl = 0x0A4, .e = 0b0, .desc = "muxctrl_reg41=0A4 0: GPIO5_1, 1: SDIO0_CCLK_OUT"},
    { .gpio = true,  .port = 5, .pin = 2, .muxctl = 0x0A8, .e = 0b0, .desc = "muxctrl_reg42=0A8 0: GPIO5_2, 1: SDIO0_CCMD"},
    { .gpio = true,  .port = 5, .pin = 3, .muxctl = 0x0AC, .e = 0b0, .desc = "muxctrl_reg43=0AC 0: GPIO5_3, 1: SDIO0_CDATA0"},
    { .gpio = true,  .port = 5, .pin = 4, .muxctl = 0x0B0, .e = 0b10,.desc = "muxctrl_reg44=0B0 00: TEST_CLK, 01: SDIO0_CDATA1, 10: GPIO5_4"},
    { .gpio = true,  .port = 5, .pin = 5, .muxctl = 0x0B4, .e = 0b0, .desc = "muxctrl_reg45=0B4 0: GPIO5_5, 1: SDIO0_CDATA2"},
    { .gpio = true,  .port = 5, .pin = 6, .muxctl = 0x0B8, .e = 0b0, .desc = "muxctrl_reg46=0B8 0: GPIO5_6, 1: SDIO0_CDATA3"},
    { .gpio = true,  .port = 5, .pin = 7, .muxctl = 0x0BC, .e = 0b0, .desc = "muxctrl_reg47=0BC 00: GPIO5_7, 01: EMMC_DATA6, 10: I2S_SD_TX, 11: UART1_RTSN"},
    { .gpio = true,  .port = 6, .pin = 0, .muxctl = 0x0C0, .e = 0b0, .desc = "muxctrl_reg48=0C0 00: GPIO6_0, 01: EMMC_DATA5, 10: I2S_WS_TX, 11: UART1_RXD"},
    { .gpio = true,  .port = 6, .pin = 1, .muxctl = 0x0C4, .e = 0b0, .desc = "muxctrl_reg49=0C4 00: GPIO6_1, 01: EMMC_DATA7, 10: I2S_MCLK, 11: UART1_CTSN"},
    { .gpio = true,  .port = 6, .pin = 2, .muxctl = 0x0C8, .e = 0b0, .desc = "muxctrl_reg50=0C8 00: GPIO6_2, 01: EMMC_DS, 10: I2S_SD_RX, 11: UART1_TXD"},
    { .gpio = true,  .port = 6, .pin = 3, .muxctl = 0x0CC, .e = 0b0, .desc = "muxctrl_reg51=0CC 00: GPIO6_3, 01: EMMC_DATA1, 11: UART2_RXD"},
    { .gpio = true,  .port = 6, .pin = 4, .muxctl = 0x0D0, .e = 0b0, .desc = "muxctrl_reg52=0D0 00: GPIO6_4, 01: EMMC_DATA2, 10: I2S_BCLK_TX, 11: UART2_TXD"},
    { .gpio = true,  .port = 6, .pin = 5, .muxctl = 0x0D4, .e = 0b0, .desc = "muxctrl_reg53=0D4 00: GPIO6_5, 01: JTAG_TRSTN, 10: SPI1_CSN1, 11: I2S_MCLK"},
    { .gpio = true,  .port = 6, .pin = 6, .muxctl = 0x0D8, .e = 0b0, .desc = "muxctrl_reg54=0D8 000: GPIO6_6, 001: JTAG_TCK, 010: SPI1_SCLK ,011: I2S_WS_TX 100: I2C1_SCL"},
    { .gpio = true,  .port = 6, .pin = 7, .muxctl = 0x0DC, .e = 0b0, .desc = "muxctrl_reg55=0DC 00: GPIO6_7, 01: JTAG_TMS, 10: SPI1_CSN0, 11: I2S_SD_TX"},
    { .gpio = true,  .port = 7, .pin = 0, .muxctl = 0x0E0, .e = 0b0, .desc = "muxctrl_reg56=0E0 000: GPIO7_0, 001: JTAG_TDO, 010: SPI1_SDO, 011: I2S_SD_RX, 100: I2C1_SDA"},
    { .gpio = true,  .port = 7, .pin = 1, .muxctl = 0x0E4, .e = 0b0, .desc = "muxctrl_reg57=0E4 00: GPIO7_1, 01: JTAG_TDI, 10: SPI1_SDI, 11: I2S_BCLK_TX"},
    { .gpio = true,  .port = 7, .pin = 2, .muxctl = 0x0E8, .e = 0b1, .desc = "muxctrl_reg58=0E8 00: PMC_PWM, 01: GPIO7_2, 10: PWM0"},
    { .gpio = true,  .port = 7, .pin = 3, .muxctl = 0x0EC, .e = 0b1, .desc = "muxctrl_reg59=0EC 0: PWM1, 1: GPIO7_3"},
    { .gpio = true,  .port = 7, .pin = 4, .muxctl = 0x0F0, .e = 0b1, .desc = "muxctrl_reg60=0F0 00: PWM2, 01: GPIO7_4, 10: SPI1_CSN1"},
    { .gpio = true,  .port = 7, .pin = 5, .muxctl = 0x0F4, .e = 0b1, .desc = "muxctrl_reg61=0F4 0: PWM3, 1: GPIO7_5"},
    { .gpio = true,  .port = 7, .pin = 6, .muxctl = 0x0F8, .e = 0b1, .desc = "muxctrl_reg62=0F8 0: SAR_ADC_CH0, 1: GPIO7_6"},
    { .gpio = true,  .port = 7, .pin = 7, .muxctl = 0x0FC, .e = 0b1, .desc = "muxctrl_reg63=0FC 0: SAR_ADC_CH1, 1: GPIO7_7"},
    { .gpio = true,  .port = 8, .pin = 0, .muxctl = 0x100, .e = 0b1, .desc = "muxctrl_reg64=100 0: SAR_ADC_CH2, 1: GPIO8_0"},
    { .gpio = true,  .port = 8, .pin = 1, .muxctl = 0x104, .e = 0b1, .desc = "muxctrl_reg65=104 0: SAR_ADC_CH3, 1: GPIO8_1"},
};

struct Pin* get_pins_hi3518EV200() { return hi3518EV200_pins; }
uint8_t get_pins_hi3518EV200_size() { return sizeof(hi3518EV200_pins)/sizeof(hi3518EV200_pins[0]); }
