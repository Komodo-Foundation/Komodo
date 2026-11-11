// C side of examples/ffi_sret.km: structs larger than 16 bytes returned
// by value (System V MEMORY class: the caller passes a hidden pointer).
#include <stdint.h>

typedef struct { double a, b, c; int64_t n; } Big;                  // 32 bytes
typedef struct { int32_t id; float w; int64_t big; double d; uint8_t tag; } Mixed;  // 32 bytes

Big sr_make(int64_t n, double scale) {
    Big r = { n * scale, n * scale * 2.0, 0.25, n };
    return r;
}

// by value in AND out
Big sr_add(Big x, Big y) {
    Big r = { x.a + y.a, x.b + y.b, x.c + y.c, x.n + y.n };
    return r;
}

Mixed sr_mixed(int32_t id, double d) {
    Mixed m = { id, 1.5f, (int64_t)id * 1000000000LL, d, 200 };
    return m;
}
