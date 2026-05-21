#include "utils.h"

/**
 * @brief 计算 uint64_t 的整数平方根
 * @param x 输入的64位无符号整数
 * @return 返回 uint32_t 的平方根（取整）
 */
uint32_t square(uint64_t x)
{
    uint64_t res = 0;      // 最终结果
    uint64_t bit = (uint64_t)1 << 62; // 从最高位开始，每次试探

    // 调整 bit 到 <= x
    while (bit > x)
        bit >>= 2;

    while (bit != 0) {
        if (x >= res + bit) {
            x -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }

    return (uint32_t)res;
}

float logf(float x)
{
    float y = (x-1)/(x+1);
    float y2 = y*y;
    return 2*(y + y*y2/3 + y*y2*y2/5);  // 5阶展开
}

/**
 * @brief  计算电流模长并转换为 float（上报用，极速）
 *         approx sqrt(M^2 + T^2)
 *
 * @param  M  M轴电流（定点）
 * @param  T  T轴电流（定点）
 * @param  rated_cur  电机额定电流(A)
 *
 * @return 电流值(A)，float
 */
#pragma CODE_SECTION(imt_current_to_float, "ramfuncs");
float imt_current_to_float(int16_t M, int16_t T, float rated_cur)
{
    int32_t absM, absT;

    // -------- 安全 abs，避免 -32768 溢出 --------
    if (M == (int16_t)0x8000)
        absM = 32767;
    else
        absM = (M >= 0) ? M : -M;

    if (T == (int16_t)0x8000)
        absT = 32767;
    else
        absT = (T >= 0) ? T : -T;

    // -------- 可选：限幅（强烈建议） --------
    if (absM > 32767) absM = 32767;
    if (absT > 32767) absT = 32767;

    // -------- 近似模长 --------
    int32_t maxv, minv;
    if (absM > absT) {
        maxv = absM;
        minv = absT;
    } else {
        maxv = absT;
        minv = absM;
    }

    // mag ≈ max + 0.375 * min
    int32_t mag = maxv + ((minv * 3) >> 3);

    // -------- 符号（保持你原逻辑） --------
    if ((M + T) < 0)
        mag = -mag;

    // rated_cur *= 1.41421356f;
    // return (float)mag * rated_cur / 40960.0f;
    return (float)-mag * rated_cur * 3.4526703e-5f;
}

/* ========================================================================= *
 * 电机非线性力矩与电流转换查表 (基于多台电机平滑去噪拟合)
 * 数组长度: 79 (覆盖 0.0A ~ 39.0A)
 * 步长: 0.5A 
 * ========================================================================= */
static const float ESTIMATED_TORQUE_LUT[79] = {
    0.000f,  0.568f,  1.135f,  1.703f,  2.270f,  2.838f,  3.405f,  3.973f,  4.540f,  5.108f,
    5.675f,  6.243f,  6.810f,  7.378f,  7.945f,  8.513f,  9.080f,  9.648f, 10.215f, 10.783f,
   11.350f, 11.918f, 12.485f, 13.053f, 13.620f, 14.179f, 14.730f, 15.272f, 15.805f, 16.330f,
   16.847f, 17.355f, 17.855f, 18.346f, 18.827f, 19.299f, 19.762f, 20.215f, 20.658f, 21.093f,
   21.518f, 21.933f, 22.338f, 22.733f, 23.118f, 23.493f, 23.858f, 24.213f, 24.558f, 24.893f,
   25.218f, 25.536f, 25.846f, 26.148f, 26.443f, 26.731f, 27.011f, 27.283f, 27.548f, 27.806f,
   28.056f, 28.298f, 28.533f, 28.761f, 28.981f, 29.193f, 29.398f, 29.596f, 29.786f, 29.968f,
   30.143f, 30.312f, 30.474f, 30.630f, 30.780f, 30.924f, 31.061f, 31.192f, 31.317f
};

#define LUT_SIZE       79
#define MAX_LUT_IQ     39.0f
#define MAX_LUT_TORQUE 31.317f

// 兜底外推斜率 (39.0A时，单步斜率为0.250 Nm/A)
#define TAIL_SLOPE_TQ_PER_A 0.250f 
// 反向外推斜率 (1 / 0.250 = 4.000)
#define TAIL_SLOPE_A_PER_TQ 4.000f

/**
 * @brief  正向映射：q轴电流(A) -> 估算实际力矩(Nm)
 * @note   算法复杂度 O(1)，支持正反转，带有界外线性兜底
 */
#pragma CODE_SECTION(Iq_To_Torque, "ramfuncs");
float Iq_To_Torque(float iq) {
    // 1. 提符号并取绝对值 (利用三目运算极速化)
    float sign = (iq < 0.0f) ? -1.0f : 1.0f;
    float abs_iq = (iq < 0.0f) ? -iq : iq;
    float abs_torque;

    // 2. 判断是否在查表范围内 (< 的严格判断避免了 index+1 造成数组越界)
    if (abs_iq < MAX_LUT_IQ) {
        float f_index = abs_iq * 2.0f;           // 因为步长是0.5, 乘2就是索引
        uint32_t index = (uint32_t)f_index;      // 硬件向下取整
        float weight = f_index - (float)index;   // 小数部分即为权重
        
        // 极速线性插值
        abs_torque = ESTIMATED_TORQUE_LUT[index] + weight * (ESTIMATED_TORQUE_LUT[index+1] - ESTIMATED_TORQUE_LUT[index]);
    } else {
        // 3. 超出查表上限时的平滑外推兜底
        abs_torque = MAX_LUT_TORQUE + (abs_iq - MAX_LUT_IQ) * TAIL_SLOPE_TQ_PER_A;
    }

    return abs_torque * sign;
}

/**
 * @brief  反向补偿：目标期望力矩(Nm) -> 需要下发的q轴电流(A)
 * @note   算法复杂度 O(log N)，支持正反转，包含深度饱和兜底
 */
#pragma CODE_SECTION(Torque_To_Iq, "ramfuncs");
float Torque_To_Iq(float target_torque)
{
    float sign = (target_torque < 0.0f) ? -1.0f : 1.0f;
    float abs_torque = (target_torque < 0.0f) ? -target_torque : target_torque;

    if (abs_torque <= 0.0f) {
        return 0.0f;
    }

    float abs_iq;

    if (abs_torque < MAX_LUT_TORQUE) {
        int left = 0, right = LUT_SIZE - 1, mid;
        while (left <= right) {
            mid = (left + right) >> 1;
            if (ESTIMATED_TORQUE_LUT[mid] < abs_torque) {
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }

        int index = right;
        if (index < 0) index = 0;
        if (index >= LUT_SIZE - 1) index = LUT_SIZE - 2;

        float torque_diff = ESTIMATED_TORQUE_LUT[index+1] - ESTIMATED_TORQUE_LUT[index];
        float weight = (abs_torque - ESTIMATED_TORQUE_LUT[index]) / torque_diff;
        abs_iq = (index + weight) * 0.5f;
    } else {
        abs_iq = MAX_LUT_IQ + (abs_torque - MAX_LUT_TORQUE) * TAIL_SLOPE_A_PER_TQ;
    }

    return abs_iq * sign;
}

#pragma CODE_SECTION(sort2, "ramfuncs");
static inline void sort2(int16_t *a, int16_t *b)
{
    int16_t t;
    if (*a > *b)
    {
        t = *a;
        *a = *b;
        *b = t;
    }
}

//midian 3

#pragma CODE_SECTION(median3, "ramfuncs");
static inline int16_t median3(int16_t a, int16_t b, int16_t c)
{
    sort2(&a, &b);
    sort2(&b, &c);
    sort2(&a, &b);
    return b;
}

#pragma CODE_SECTION(Filter_Median3, "ramfuncs");
int16_t Filter_Median3(int16_t input, Median3Filter_t *f)
{
    int16_t out;

    if(f->init == 0)
    {
        f->x1 = input;
        f->x2 = input;
        f->init = 1;
        return input;
    }

    out = median3(f->x2, f->x1, input);

    // 历史必须是 input（原始数据）
    f->x2 = f->x1;
    f->x1 = input;

    return out;
}

// midian 5

#pragma CODE_SECTION(median5, "ramfuncs");
static inline int16_t median5(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e)
{
    sort2(&a, &b);
    sort2(&c, &d);
    sort2(&a, &c);
    sort2(&b, &d);
    sort2(&b, &c);
    sort2(&d, &e);
    sort2(&c, &d);
    sort2(&b, &c);

    return c;
}

#pragma CODE_SECTION(Filter_Median5, "ramfuncs");
int16_t Filter_Median5(int16_t input, Median5Filter_t *f)
{
    int16_t out;

    if(f->init == 0)
    {
        f->x1 = input;
        f->x2 = input;
        f->x3 = input;
        f->x4 = input;
        f->init = 1;
        return input;
    }

    out = median5(f->x4, f->x3, f->x2, f->x1, input);

    // 历史必须是 input（原始数据）
    f->x4 = f->x3;
    f->x3 = f->x2;
    f->x2 = f->x1;
    f->x1 = input;

    return out;
}

// midian 7

#pragma CODE_SECTION(median7, "ramfuncs");
static inline int16_t median7(
    int16_t a, int16_t b, int16_t c,
    int16_t d, int16_t e, int16_t f, int16_t g)
{
    sort2(&a, &b);
    sort2(&c, &d);
    sort2(&e, &f);

    sort2(&a, &c);
    sort2(&b, &d);
    sort2(&e, &g);

    sort2(&a, &e);
    sort2(&b, &f);
    sort2(&c, &g);

    sort2(&b, &c);
    sort2(&d, &e);
    sort2(&f, &g);

    sort2(&c, &d);
    sort2(&e, &f);

    sort2(&d, &e);   // 最终中值

    return d;
}

#pragma CODE_SECTION(Filter_Median7, "ramfuncs");
int16_t Filter_Median7(int16_t input, Median7Filter_t *f)
{
    int16_t out;

    if (f->init == 0)
    {
        f->x1 = input;
        f->x2 = input;
        f->x3 = input;
        f->x4 = input;
        f->x5 = input;
        f->x6 = input;
        f->init = 1;
        return input;
    }

    out = median7(f->x6, f->x5, f->x4, f->x3, f->x2, f->x1, input);

    // ?? 历史必须是 input（原始数据）
    f->x6 = f->x5;
    f->x5 = f->x4;
    f->x4 = f->x3;
    f->x3 = f->x2;
    f->x2 = f->x1;
    f->x1 = input;

    return out;
}
