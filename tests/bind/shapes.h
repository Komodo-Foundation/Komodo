#ifndef SHAPES_H
#define SHAPES_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define SH_VERSION_MAJOR 2
#define SH_MAX_SHAPES 0x40
#define SH_FLAG_FILL (1 << 3)
#define SH_FLAG_ALL (SH_FLAG_FILL | 1)
#define SH_NEG (-5)
#define SH_NAME "shapes"
#define SH_PI 3.25
#define SH_SQUARE(x) ((x) * (x))

enum ShapeKind { SHAPE_CIRCLE, SHAPE_RECT = 5, SHAPE_TRI };
typedef enum { COLOR_RED = 1, COLOR_GREEN, COLOR_BLUE = SHAPE_TRI + 10 } Color;

typedef struct Point { double x; double y; } Point;
typedef struct { float w; float h; int32_t id; } Size;
typedef struct Canvas Canvas;
typedef struct Palette *PaletteRef;
typedef int (*visit_fn)(int64_t value, const char *label);

Canvas *canvas_new(int64_t w, int64_t h);
void canvas_free(Canvas *c);
int64_t canvas_area(const Canvas *c);
int canvas_neg(int x);
const char *canvas_name(const Canvas *c);
bool canvas_is_empty(const Canvas *c);
double point_dist(Point a, Point b);
Point point_scale(Point p, double k);
float size_area(Size s);
int64_t sum_array(const int64_t *xs, int64_t n);
void fill_squares(int64_t *out, int64_t n);
size_t shapes_count(enum ShapeKind k);
int64_t each(visit_fn cb, int64_t n);
void *raw_alloc(size_t n);
void raw_free(void *p);
PaletteRef palette_new(void);
int palette_size(PaletteRef p);
void palette_free(PaletteRef p);
int shape_log(const char *fmt, ...);
long double shape_ld(long double x);
static inline int sh_twice(int x) { return 2 * x; }
extern int sh_counter;
#endif
