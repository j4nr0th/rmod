//
// Created by jan on 4.6.2023.
//
#include <stdio.h>
#include <math.h>
#include <time.h>
#define V_ORDER 16
#define N_VALUES (1u << V_ORDER)
#define MIN_V 0.1
#define MAX_V 0.9

static void print_double_array(const unsigned n, const double_t* ptr)
{
#ifndef NDEBUG
    for (unsigned i = 0; i < n; ++i)
    {
        printf("% 5.4f ", ptr[i]);
    }
    putchar('\n');
#endif
}

static float_t compute_log_fast(const float_t x)
{
    const union {float_t f; unsigned i;} both = {.f = x};

//    const double_t mantissa = (double_t)(both.i & ((1 << 23) - 1)) / (1 << 23);
//    const double_t exponent = (double_t)((int)((both.i & (0x7F800000)) >> 23) - 127);
//    return M_LN2 * (exponent + mantissa * (1) + 0.0430);

    const double_t lv2 = (double_t) both.i / (1 << 23) - 127 + 0.043;

    return (float_t)(lv2 * M_LN2);
}

static void print_duration(const struct timespec* ts_end, const struct timespec* ts_begin)
{
    printf("Time taken: %g\n", (double_t)(ts_end->tv_sec - ts_begin->tv_sec) + (double_t)(ts_end->tv_nsec - ts_begin->tv_nsec) / 1e9);
}

int main()
{
    double_t error[N_VALUES];
    double_t errorsq[N_VALUES];
    double_t x_vals[N_VALUES];
    for (unsigned i = 0; i < N_VALUES; ++i)
    {
        x_vals[i] = 1.0 - ((MAX_V - MIN_V) / (N_VALUES - 1) * (double_t)i + MIN_V);
    }

    print_double_array(N_VALUES, x_vals);
    putchar('\n');
    double_t exact[N_VALUES];
    struct timespec t0, t1;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t0);
    for (unsigned i = 0; i < N_VALUES; ++i)
    {
        exact[i] = log(x_vals[i]);
    }
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
    print_duration(&t1, &t0);
    print_double_array(N_VALUES, exact);
    putchar('\n');
    double_t approx[N_VALUES];
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t0);
    for (unsigned i = 0; i < N_VALUES; ++i)
    {
        approx[i] = (double_t)compute_log_fast((float_t)x_vals[i]);
    }
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
    print_duration(&t1, &t0);
    print_double_array(N_VALUES, approx);
    putchar('\n');

    for (unsigned i = 0; i < N_VALUES; ++i)
    {
        if (approx[i] != exact[i])
        {
            error[i] = fabs((approx[i] - exact[i]) / exact[i]) * 100.0;
        }
        else
        {
            error[i] = 0;
        }
        errorsq[i] = error[i] * error[i];
    }

    print_double_array(N_VALUES, error);
    putchar('\n');
    print_double_array(N_VALUES, errorsq);
    putchar('\n');
    double_t v = 0, v2 = 0;
    for (unsigned i = 0; i < N_VALUES; ++i)
    {
        v += error[i];
        v2 += error[i] * error[i];
    }
    printf("%.20g\n", v);
    printf("%.20g\n", v2);

    for (unsigned order = 0; order < V_ORDER; ++order)
    {
        const unsigned step_size = 1u << order;
        for (unsigned i = 0; i < (1u << (V_ORDER - order)); i += 2)
        {
            error[step_size * i] += error[step_size * (i + 1)];
            errorsq[step_size * i] += errorsq[step_size * (i + 1)];
        }
    }
    printf("%.20g\n", error[0]);
    printf("%.20g\n", errorsq[0]);

    double_t mean = error[0] / N_VALUES;
    double_t stdiv = sqrt((errorsq[0] - (mean * mean * N_VALUES)) / N_VALUES);
    printf("Measures of spred of error:\n\tAvg error: %g\n\tAvg stdiv: %g\n", mean, stdiv);

    return 0;
}
