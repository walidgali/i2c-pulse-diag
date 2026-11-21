# I2C Layer

Transport abstraction used by `i2c-pulse-diag`.

## Why a context API?

Older code opened `/dev/i2c-N` per read and used separate `write()` + `read()`.
Two problems:

1. **Non-atomic transactions.** Between write and read the bus is free. A second
   master may interleave, and some sensors reset their register pointer on STOP,
   silently returning data from register 0.
2. **Syscall overhead.** Three extra syscalls per read.

The new API uses `ioctl(I2C_RDWR)` with an atomic message set:
`START + addr_W + reg + Repeated_START + addr_R + N bytes + STOP`.

## Quick start

```c
#include "i2c.h"

i2c_ctx_t ctx;
if (i2c_ctx_open(&ctx, 1) != 0) { perror("open bus"); return 1; }

uint8_t buf[2];
if (i2c_read_reg(&ctx, 0x48, 0x02, buf, sizeof(buf)) == 0) {
    uint16_t raw = (uint16_t)((buf[0] << 8) | buf[1]);
}
i2c_ctx_close(&ctx);
