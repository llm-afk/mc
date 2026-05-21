#ifndef UTILS_H
#define UTILS_H

#include "MainInclude.h"

typedef struct
{
    int16_t x1;   // n-1
    int16_t x2;   // n-2
    uint16_t init;
} Median3Filter_t;

typedef struct
{
    int16_t x1;   // n-1
    int16_t x2;   // n-2
    int16_t x3;   // n-3
    int16_t x4;   // n-4
    uint16_t init;
} Median5Filter_t;

typedef struct
{
    int16_t x1;   // n-1
    int16_t x2;   // n-2
    int16_t x3;   // n-3
    int16_t x4;   // n-4
    int16_t x5;   // n-5
    int16_t x6;   // n-6
    uint16_t init;
} Median7Filter_t;

#define M_PI    3.1415927410f
#define TWO_PI  6.2831854820f

#define CLAMP(x, min, max) (((x) > (max)) ? (max) : (((x) < (min)) ? (min) : (x)))

#define INT_ABS(x) ((x) < 0 ? -(x) : (x))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

uint32_t square(uint64_t x);
float logf(float x);

float Iq_To_Torque(float iq);
float Torque_To_Iq(float target_torque);

float imt_current_to_float(int16_t M, int16_t T, float rated_cur);

int16_t Filter_Median3(int16_t input, Median3Filter_t *f);
int16_t Filter_Median5(int16_t input, Median5Filter_t *f);
int16_t Filter_Median7(int16_t input, Median7Filter_t *f);

#endif
