#include "diag.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int i2c_is_valid_addr(uint8_t addr) {
    return (addr >= I2C_ADDR_MIN && addr <= I2C_ADDR_MAX) ? 1 : 0;
}

static int open_i2c_bus(uint8_t bus_id, int *out_fd) {
    if (!out_fd) {
        return -1;
    }

    char dev_path[32];
    int n = snprintf(dev_path, sizeof(dev_path), "/dev/i2c-%u", (unsigned int)bus_id);
    if (n < 0 || (size_t)n >= sizeof(dev_path)) {
        fprintf(stderr, "Error: invalid I2C bus id %u\n", (unsigned int)bus_id);
        return -1;
    }

    int flags = O_RDWR;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif

    int fd = open(dev_path, flags);
    if (fd < 0) {
        fprintf(stderr, "Error: open %s failed: %s\n", dev_path, strerror(errno));
        return -1;
    }

    *out_fd = fd;
    return 0;
}

int i2c_scan_bus(uint8_t bus_id) {
    int fd = -1;
    if (open_i2c_bus(bus_id, &fd) != 0) {
        return -1;
    }

    printf("Scanning I2C bus %u (0x%02X..0x%02X):\n",
           (unsigned int)bus_id, I2C_ADDR_MIN, I2C_ADDR_MAX);

    unsigned int found = 0;
    for (uint8_t addr = I2C_ADDR_MIN; addr <= I2C_ADDR_MAX; addr++) {
        if (ioctl(fd, I2C_SLAVE, (unsigned long)addr) < 0) {
            continue;
        }
        uint8_t dummy = 0;
        if (read(fd, &dummy, 1) >= 0) {
            printf("  [FOUND] 0x%02X\n", (unsigned int)addr);
            found++;
        }
    }

    if (close(fd) != 0) {
        fprintf(stderr, "Error: close failed on bus %u: %s\n",
                (unsigned int)bus_id, strerror(errno));
        return -1;
    }

    printf("Scan complete. Active devices found: %u\n", found);
    return 0;
}

int i2c_read_reg16(uint8_t bus_id, uint8_t dev_addr, uint8_t reg_addr, uint16_t *out_val) {
    if (!out_val || !i2c_is_valid_addr(dev_addr)) {
        fprintf(stderr, "Error: invalid arguments to i2c_read_reg16\n");
        return -1;
    }

    int fd = -1;
    if (open_i2c_bus(bus_id, &fd) != 0) {
        return -1;
    }

    if (ioctl(fd, I2C_SLAVE, (unsigned long)dev_addr) < 0) {
        fprintf(stderr, "Error: ioctl I2C_SLAVE failed (addr 0x%02X, bus %u): %s\n",
                (unsigned int)dev_addr, (unsigned int)bus_id, strerror(errno));
        close(fd);
        return -1;
    }

    if (write(fd, &reg_addr, 1) != 1) {
        fprintf(stderr, "Error: write reg 0x%02X to 0x%02X failed: %s\n",
                (unsigned int)reg_addr, (unsigned int)dev_addr, strerror(errno));
        close(fd);
        return -1;
    }

    uint8_t buf[2] = {0, 0};
    ssize_t bytes_read = read(fd, buf, 2);
    if (bytes_read != 2) {
        fprintf(stderr, "Error: read 2 bytes from reg 0x%02X failed (got %zd): %s\n",
                (unsigned int)reg_addr, bytes_read, strerror(errno));
        close(fd);
        return -1;
    }

    if (close(fd) != 0) {
        fprintf(stderr, "Error: close I2C fd failed: %s\n", strerror(errno));
        return -1;
    }

    *out_val = (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
    return 0;
}
