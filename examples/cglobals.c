// C side of examples/ffi_extern_var.km: global variables of several widths.
#include <stdint.h>

int32_t cg_counter = -5;
double cg_scale = 1.5;
uint8_t cg_flag = 200;
const char *cg_name = "native";
float cg_f32 = 2.5f;
int64_t cg_big = 1099511627776LL;   // 2^40

// lets the Komodo side prove that its writes reached the C variables
int64_t cg_total(void) {
    return (int64_t)cg_counter * 1000 + (int64_t)(cg_scale * 10.0) + cg_flag;
}
