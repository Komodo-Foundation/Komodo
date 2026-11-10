#include "shapes.h"
#include <stdlib.h>
#include <stdio.h>
struct Canvas { int64_t w, h; };
struct Palette { int n; };
int sh_counter = 41;
Canvas *canvas_new(int64_t w, int64_t h){ Canvas*c=malloc(sizeof *c); c->w=w; c->h=h; return c; }
void canvas_free(Canvas *c){ free(c); }
int64_t canvas_area(const Canvas *c){ return c->w*c->h; }
int canvas_neg(int x){ return -x; }
const char *canvas_name(const Canvas *c){ return c->w==c->h ? "square" : "rect"; }
bool canvas_is_empty(const Canvas *c){ return c->w*c->h==0; }
double point_dist(Point a, Point b){ double dx=a.x-b.x, dy=a.y-b.y; return dx*dx+dy*dy; }
Point point_scale(Point p, double k){ Point r={p.x*k,p.y*k}; return r; }
float size_area(Size s){ return s.w*s.h + s.id; }
int64_t sum_array(const int64_t *xs, int64_t n){ int64_t s=0; for(int64_t i=0;i<n;i++) s+=xs[i]; return s; }
void fill_squares(int64_t *out, int64_t n){ for(int64_t i=0;i<n;i++) out[i]=(i+1)*(i+1); }
size_t shapes_count(enum ShapeKind k){ return k==SHAPE_RECT ? 7 : 3; }
int64_t each(visit_fn cb, int64_t n){ int64_t t=0; for(int64_t i=1;i<=n;i++) t+=cb(i,"x"); return t; }
void *raw_alloc(size_t n){ return malloc(n); }
void raw_free(void *p){ free(p); }
PaletteRef palette_new(void){ PaletteRef p=malloc(sizeof *p); p->n=3; return p; }
int palette_size(PaletteRef p){ return p->n; }
void palette_free(PaletteRef p){ free(p); }
int shape_log(const char *fmt, ...){ return 0; }
long double shape_ld(long double x){ return x; }
