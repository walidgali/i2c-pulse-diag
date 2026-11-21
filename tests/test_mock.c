#include <stdio.h>
#include <assert.h>
#include "i2c.h"

int main(void) {
    i2c_ctx_t ctx;
    
    // نستدعي الدالة أولاً، ثم نتحقق من نتيجتها (هذا هو الأسلوب الآمن)
    int ret = i2c_ctx_open_mock(&ctx);
    assert(ret == 0);
    if (ret != 0) return 1;
    
    i2c_mock_set_reg(0x48, 0x02, 0x12);
    i2c_mock_set_reg(0x48, 0x03, 0x34);
    
    uint8_t buf[2] = {0};
    
    // نستدعي القراءة، ثم نتحقق
    ret = i2c_read_reg(&ctx, 0x48, 0x02, buf, 2);
    assert(ret == 0);
    if (ret != 0) { i2c_ctx_close(&ctx); return 1; }
    
    printf("Read: 0x%02X%02X (Expected: 0x1234)\n", buf[0], buf[1]);
    
    assert(buf[0] == 0x12 && buf[1] == 0x34);
    printf("✅ Test passed successfully!\n");
    
    i2c_ctx_close(&ctx);
    return 0;
}
