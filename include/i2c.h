/*
 * i2c.h - I2C transport abstraction layer
 *
 * Design goals:
 *   - Atomic register transactions (Repeated START, not STOP+START)
 *   - Context-based API (open once, reuse fd)
 *   - Pluggable backends (Linux /dev/i2c, mock for tests)
 *   - Backward compatible with legacy i2c_read_reg16() API
 */
#ifndef I2C_H
#define I2C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef I2C_ADDR_MIN
#define I2C_ADDR_MIN 0x08u
#endif
#ifndef I2C_ADDR_MAX
#define I2C_ADDR_MAX 0x77u
#endif

typedef struct i2c_transport {
    int  (*read)(void *priv, uint8_t addr, uint8_t reg, uint8_t *buf, size_t len);
    int  (*write)(void *priv, uint8_t addr, uint8_t reg, const uint8_t *buf, size_t len);
    int  (*probe)(void *priv, uint8_t addr);
    void (*destroy)(void *priv);
} i2c_transport_t;

typedef struct {
    const i2c_transport_t *ops;
    void *priv;
    uint8_t bus_id;
    uint32_t retries;
    uint32_t retry_delay_us;
} i2c_ctx_t;

int  i2c_ctx_open(i2c_ctx_t *ctx, uint8_t bus_id);
int  i2c_ctx_open_mock(i2c_ctx_t *ctx);
void i2c_ctx_close(i2c_ctx_t *ctx);

int  i2c_read_reg (i2c_ctx_t *ctx, uint8_t addr, uint8_t reg, uint8_t *buf, size_t len);
int  i2c_write_reg(i2c_ctx_t *ctx, uint8_t addr, uint8_t reg, const uint8_t *buf, size_t len);
int  i2c_probe    (i2c_ctx_t *ctx, uint8_t addr);
int  i2c_scan_bus_ex(i2c_ctx_t *ctx, uint8_t *addrs, size_t max, size_t *found);
int  i2c_is_valid_addr(uint8_t addr);

int  i2c_scan_bus(uint8_t bus_id);
int  i2c_read_reg16(uint8_t bus_id, uint8_t dev_addr, uint8_t reg_addr, uint16_t *out_val);

void i2c_mock_reset(void);
void i2c_mock_set_present(uint8_t addr, int present);
void i2c_mock_set_reg(uint8_t addr, uint8_t reg, uint8_t value);
int  i2c_mock_get_reg(uint8_t addr, uint8_t reg, uint8_t *value);
void i2c_mock_set_nack_count(int count);
void i2c_mock_set_read_fail_count(int count);
void i2c_mock_set_delay_us(unsigned int us);

#ifdef __cplusplus
}
#endif
#endif
