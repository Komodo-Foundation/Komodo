/* Companion C for examples/vecffi.kmm — plain C structs passed and
 * returned by value, to exercise Komodo's `copy(x)` FFI marshalling. */
#include <stdint.h>

typedef struct { float x, y; } Vec2;            /* 8 B  : one SSE eightbyte  */
typedef struct { float x, y, z; } Vec3;         /* 12 B : two SSE eightbytes */
typedef struct { uint8_t r, g, b, a; } Rgba;    /* 4 B  : one INTEGER        */
typedef struct { int32_t id; float w; } Mix;    /* 8 B  : mixed → INTEGER    */
typedef struct { double a, b, c; int64_t n; } Big; /* 32 B : MEMORY class   */

Vec2   vf_scale2(Vec2 v, float k)      { Vec2 r = { v.x * k, v.y * k }; return r; }
float  vf_dot3(Vec3 a, Vec3 b)         { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3   vf_cross3(Vec3 a, Vec3 b)
{
    Vec3 r = { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    return r;
}
long   vf_pack(Rgba c)                 { return ((long)c.r << 24) | (c.g << 16) | (c.b << 8) | c.a; }
Rgba   vf_invert(Rgba c)               { Rgba r = { 255 - c.r, 255 - c.g, 255 - c.b, c.a }; return r; }
double vf_mix(Mix m)                   { return m.id * (double)m.w; }
double vf_big(long pre, Big b, long post)
{
    return pre + b.a + b.b + b.c + (double)b.n + post;
}
