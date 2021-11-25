#include "chipid.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

void chip_id() {
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        printf("can't open /dev/mem \n");
        return;
    }

    uint32_t SC_CTRL_base = 0x20050000;
    uint32_t SCSYSID0 = 0xEE0;
    uint32_t SCSYSID1 = 0xEE4;
    uint32_t SCSYSID2 = 0xEE8;
    uint32_t SCSYSID3 = 0xEEC;

    volatile char *sc_ctrl_map = mmap(
        NULL,               // Any adddress in our space will do
        SCSYSID0 + 4 * 100, // Map length
        PROT_READ,          // Enable reading & writting to mapped memory
        MAP_SHARED,         // Shared with other processes
        mem_fd,             // File to map
        SC_CTRL_base        // Offset to base address
    );
    if (sc_ctrl_map == MAP_FAILED) {
        printf("sc_ctrl_map mmap error %d\n", (int)sc_ctrl_map);
        printf("Error: %s (%d)\n", strerror(errno), errno);
        close(mem_fd);
        return;
    }

    close(mem_fd);

    uint32_t chip_id = 0;
    char *ptr = (char *)&chip_id;
    ptr[0] = *(volatile char *)(sc_ctrl_map + SCSYSID0);
    ptr[1] = *(volatile char *)(sc_ctrl_map + SCSYSID1);
    ptr[2] = *(volatile char *)(sc_ctrl_map + SCSYSID2);
    ptr[3] = *(volatile char *)(sc_ctrl_map + SCSYSID3);
    printf("chip_id: %X", chip_id);

    uint32_t SCSYSID0_reg = ((volatile uint32_t *)(sc_ctrl_map + SCSYSID0))[0];
    char SCSYSID0_chip_id = ((char *)&SCSYSID0_reg)[3];
    printf("    SCSYSID0 chip_id: ");
    switch (SCSYSID0_chip_id) {
    case 1:
        printf("Hi3516CV200\n");
        break;
    case 2:
        printf("Hi3518EV200\n");
        break;
    case 3:
        printf("Hi3518EV201\n");
        break;
    default:
        printf("reserved value %d\n", SCSYSID0_chip_id);
    }
}

void isp_version() {
    int mem_fd = open("/dev/mem", O_RDONLY | O_SYNC);
    if (mem_fd < 0) {
        printf("can't open /dev/mem \n");
        return;
    }

    const uint32_t base_address = 0x20580000;
    void *isp_version_map = mmap(
        NULL,                       // Any adddress in our space will do
        0x20080 + sizeof(uint32_t), // Map length
        PROT_READ,   // Enable reading & writting to mapped memory
        MAP_PRIVATE, // Shared with other processes
        mem_fd,      // File to map
        base_address // Offset to base address
    );
    close(mem_fd);
    if (isp_version_map == MAP_FAILED) {
        printf("isp_version_map mmap error %d\n", (int)isp_version_map);
        printf("Error: %s (%d)\n", strerror(errno), errno);
        return;
    }

    uint32_t isp_version_register =
        ((volatile uint32_t *)(isp_version_map + 0x20080))[0];
    printf("isp_version_register: 0x%08X", isp_version_register);

    // 0b_0000_0000_0001_0000_0000_0000_0000_0000
    // 0b_1111_1111_1111_0000_0000_0000_0000_0000
    // 0b_0000_0000_0000_1111_0000_0000_0000_0000
    // 0b_0000_0000_0000_0000_1111_1111_1111_1111

    uint32_t version =
        (isp_version_register & 0b11111111111100000000000000000000) >> 20;
    uint32_t build =
        (isp_version_register & 0b00000000000011110000000000000000) >>
        16; // 0b00000000_00001111_00000000_00000000
    uint32_t sn =
        isp_version_register &
        0b00000000000000001111111111111111; // 0b00000000_00000000_11111111_11111111
    printf("    version: ");
    switch (version) {
    case 1:
        printf("V100");
        break;
    case 2:
        printf("V200");
        break;
    case 3:
        printf("V300");
        break;
    case 4:
        printf("V400");
        break;
    case 5:
        printf("V500");
        break;
    case 6:
        printf("V600");
        break;
    case 7:
        printf("V700");
        break;
    default:
        printf("unknown %d", version);
    }

    printf("    build: ");
    switch (build) {
    case 1:
        printf("BO1");
        break;
    case 2:
        printf("BO2");
        break;
    case 3:
        printf("BO3");
        break;
    case 4:
        printf("BO4");
        break;
    case 5:
        printf("BO5");
        break;
    case 6:
        printf("BO6");
        break;
    case 7:
        printf("BO7");
        break;
    default:
        printf("unknown %d", build);
    }

    printf("    sequence number: %d\n", sn);
}
