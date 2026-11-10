#include <stdint.h>
uint8_t  w_inc8(uint8_t x)            { return (uint8_t)(x + 1); }
int32_t  w_neg32(int32_t x)           { return (int32_t)(0u - (uint32_t)x); }
uint32_t w_add32(uint32_t a, uint32_t b) { return a + b; }
uint64_t w_big(uint64_t x)            { return x + 1; }
float    w_half(float x)              { return x / 2.0f; }
