// C side of examples/ffi_callback_ext.km: functions that take callbacks
// with doubles, floats and more than six parameters.
#include <stdint.h>

double cb_integrate(double (*f)(double), double a, double b, int64_t n) {
    double h = (b - a) / (double)n, sum = 0.5 * (f(a) + f(b));
    for (int64_t i = 1; i < n; i++)
        sum += f(a + (double)i * h);
    return sum * h;
}

int64_t cb_sum8(int64_t (*f)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t)) {
    return f(1, 2, 3, 4, 5, 6, 7, 8);
}

double cb_mixed(double (*f)(int64_t, double, int64_t, double, int64_t, double, int64_t, double, int64_t, double)) {
    return f(1, 0.5, 2, 1.5, 3, 2.5, 4, 3.5, 5, 4.5);
}

float cb_f32(float (*f)(float, float)) {
    return f(1.5f, 2.25f);
}
