#pragma once

#include <stdint.h>
#include <stdbool.h>


#define ISP_BASE 0x20580000
static const uint32_t ISP_VERSION = ISP_BASE + 0x20080;

#define SC_BASE 0x20050000
static const uint32_t SCSYSID0 = SC_BASE + 0x0EE0;
static const uint32_t SCSYSID1 = SC_BASE + 0x0EE4;
static const uint32_t SCSYSID2 = SC_BASE + 0x0EE8;
static const uint32_t SCSYSID3 = SC_BASE + 0x0EEC;

static const uint32_t IOMUX = 0x200F0000;

static const uint32_t GPIO_DIR = 0x0400;

#define GPIO0 0x20140000
#define GPIO1 0x20150000
#define GPIO2 0x20160000
#define GPIO3 0x20170000
#define GPIO4 0x20180000
#define GPIO5 0x20190000
#define GPIO6 0x201A0000
#define GPIO7 0x201B0000
#define GPIO8 0x201C0000

static const uint32_t GPIO[] = {GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO6, GPIO7, GPIO8};

struct Pin {
    uint8_t port;
    uint8_t pin;
    uint32_t muxctl;
    bool gpio;
    uint8_t e;
    char desc[128];
};

struct Pin* get_pins_hi3518EV200();
uint8_t get_pins_hi3518EV200_size();

bool pin_linux_to_port_pin(const uint8_t pin_number, uint8_t *port, uint8_t *pin);


bool set_pin_linux(const uint8_t pin_number, const bool bit);
bool get_pin_linux(const uint8_t pin_number, bool *value);
