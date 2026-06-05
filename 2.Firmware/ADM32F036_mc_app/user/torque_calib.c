#include "torque_calib.h"
#include <stddef.h>

/**
 * @file torque_calib.c
 * @brief 等幅值矢量控制下 Iq 电流 <-> 电机输出力矩双向转换模块
 *
 * 核心设计规则：
 * 1. 标定表仅存放正半轴数据。函数内部自动提取符号, 取绝对值查表后还原符号
 * 2. 0A 必对应 0Nm, 标定表第 0 点必须为 {0.0f, 0.0f}
 * 3. 标定点必须以 Iq 升序严格排列, 允许非均匀分布
 * 4. 超出标定范围时使用最后两点的斜率线性外推
 * 5. 内部查表用二分法, O(log N)
 */

/* ========================================================================= *
 * 默认力矩标定表 (力矩系数 1:1, 待实测后替换)
 * ========================================================================= */

const tTorqueCalibPoint gTorqueCalibTable[] = {
    {   0.0000f,    0.0000f },    {   0.1000f,    0.1123f },    {   0.4000f,    0.4510f },
    {   0.7000f,    0.7925f },    {   1.0000f,    1.1366f },    {   1.3000f,    1.4832f },
    {   1.6000f,    1.8322f },    {   1.9000f,    2.1833f },    {   2.2000f,    2.5365f },
    {   2.5000f,    2.8917f },    {   2.8000f,    3.2486f },    {   3.1000f,    3.6072f },
    {   3.4000f,    3.9672f },    {   3.7000f,    4.3286f },    {   4.0000f,    4.6912f },
    {   4.3000f,    5.0548f },    {   4.6000f,    5.4194f },    {   4.9000f,    5.7846f },
    {   5.2000f,    6.1505f },    {   5.5000f,    6.5169f },    {   5.8000f,    6.8835f },
    {   6.1000f,    7.2504f },    {   6.4000f,    7.6172f },    {   6.7000f,    7.9840f },
    {   7.0000f,    8.3504f },    {   7.3000f,    8.7165f },    {   7.6000f,    9.0820f },
    {   7.9000f,    9.4468f },    {   8.2000f,    9.8107f },    {   8.5000f,   10.1737f },
    {   8.8000f,   10.5356f },    {   9.1000f,   10.8963f },    {   9.4000f,   11.2555f },
    {   9.7000f,   11.6133f },    {  10.0000f,   11.9693f },    {  10.3000f,   12.3237f },
    {  10.6000f,   12.6761f },    {  10.9000f,   13.0265f },    {  11.2000f,   13.3747f },
    {  11.5000f,   13.7207f },    {  11.8000f,   14.0643f },    {  12.1000f,   14.4053f },
    {  12.4000f,   14.7438f },    {  12.7000f,   15.0795f },    {  13.0000f,   15.4124f },
    {  13.3000f,   15.7424f },    {  13.6000f,   16.0693f },    {  13.9000f,   16.3931f },
    {  14.2000f,   16.7137f },    {  14.5000f,   17.0310f },    {  14.8000f,   17.3448f },
    {  15.1000f,   17.6552f },    {  15.4000f,   17.9620f },    {  15.7000f,   18.2651f },
    {  16.0000f,   18.5646f },    {  16.3000f,   18.8602f },    {  16.6000f,   19.1520f },
    {  16.9000f,   19.4399f },    {  17.2000f,   19.7239f },    {  17.5000f,   20.0038f },
    {  17.8000f,   20.2797f },    {  18.1000f,   20.5514f },    {  18.4000f,   20.8191f },
    {  18.7000f,   21.0826f },    {  19.0000f,   21.3419f },    {  19.3000f,   21.5970f },
    {  19.6000f,   21.8479f },    {  19.9000f,   22.0945f },    {  20.2000f,   22.3369f },
    {  20.5000f,   22.5751f },    {  20.8000f,   22.8090f },    {  21.1000f,   23.0388f },
    {  21.4000f,   23.2643f },    {  21.7000f,   23.4857f },    {  22.0000f,   23.7030f },
    {  22.3000f,   23.9161f },    {  22.6000f,   24.1252f },    {  22.9000f,   24.3303f },
    {  23.2000f,   24.5315f },    {  23.5000f,   24.7287f },    {  23.8000f,   24.9222f },
    {  24.1000f,   25.1120f },    {  24.4000f,   25.2980f },    {  24.7000f,   25.4806f },
    {  25.0000f,   25.6597f },    {  25.3000f,   25.8354f },    {  25.6000f,   26.0080f },
    {  25.9000f,   26.1774f },    {  26.2000f,   26.3438f },    {  26.5000f,   26.5074f },
    {  26.8000f,   26.6683f },    {  27.1000f,   26.8267f },    {  27.4000f,   26.9827f },
    {  27.7000f,   27.1365f },    {  28.0000f,   27.2882f },    {  28.3000f,   27.4382f },
    {  28.6000f,   27.5865f },    {  28.9000f,   27.7333f },    {  29.2000f,   27.8789f },
    {  29.5000f,   28.0236f },    {  29.8000f,   28.1674f },    {  30.1000f,   28.3108f },
    {  30.4000f,   28.4538f },    {  30.7000f,   28.5969f },    {  31.0000f,   28.7402f },
    {  31.3000f,   28.8840f },    {  31.6000f,   29.0286f },    {  31.9000f,   29.1744f },
    {  32.2000f,   29.3216f },    {  32.5000f,   29.4705f },    {  32.8000f,   29.6215f },
    {  33.1000f,   29.7750f },    {  33.4000f,   29.9311f },    {  33.7000f,   30.0904f },
    {  34.0000f,   30.2532f },    {  34.3000f,   30.4199f },    {  34.6000f,   30.5909f },
    {  34.9000f,   30.7665f },
};


const uint16_t gTorqueCalibTableSize = sizeof(gTorqueCalibTable) / sizeof(gTorqueCalibTable[0]);

/**
 * @brief 基于 Iq 查力矩 (一维线性插值 + 上边界外推)
 * @param x_val  Iq 绝对值 [A]
 * @param table  标定表
 * @param length 点数
 * @return 力矩绝对值 [Nm]
 */
static float interpolate_iq_to_torque(float x_val, const tTorqueCalibPoint *table, uint16_t length)
{
    /* 上边界外推 */
    if (x_val >= table[length - 1].iq) {
        float dx = table[length - 1].iq - table[length - 2].iq;
        float dy = table[length - 1].torque - table[length - 2].torque;
        float slope = dy / (dx + 1e-9f);
        return table[length - 1].torque + slope * (x_val - table[length - 1].iq);
    }

    /* 二分查找区间 [idx, idx+1] */
    int16_t low = 0;
    int16_t high = (int16_t)(length - 2);
    int16_t idx = 0;
    while (low <= high) {
        int16_t mid = (low + high) >> 1;
        if (x_val >= table[mid].iq && x_val <= table[mid + 1].iq) {
            idx = mid;
            break;
        } else if (x_val < table[mid].iq) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }

    /* 线性插值 */
    float dx = table[idx + 1].iq - table[idx].iq;
    float dy = table[idx + 1].torque - table[idx].torque;
    float ratio = (x_val - table[idx].iq) / (dx + 1e-9f);
    return table[idx].torque + ratio * dy;
}

/**
 * @brief 基于力矩反查 Iq (一维线性插值 + 上边界外推)
 * @param x_val  力矩绝对值 [Nm]
 * @param table  标定表
 * @param length 点数
 * @return Iq 绝对值 [A]
 */
static float interpolate_torque_to_iq(float x_val, const tTorqueCalibPoint *table, uint16_t length)
{
    /* 上边界外推 */
    if (x_val >= table[length - 1].torque) {
        float dx = table[length - 1].torque - table[length - 2].torque;
        float dy = table[length - 1].iq - table[length - 2].iq;
        float slope = dy / (dx + 1e-9f);
        return table[length - 1].iq + slope * (x_val - table[length - 1].torque);
    }

    /* 二分查找区间 [idx, idx+1] */
    int16_t low = 0;
    int16_t high = (int16_t)(length - 2);
    int16_t idx = 0;
    while (low <= high) {
        int16_t mid = (low + high) >> 1;
        if (x_val >= table[mid].torque && x_val <= table[mid + 1].torque) {
            idx = mid;
            break;
        } else if (x_val < table[mid].torque) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }

    /* 线性插值 */
    float dx = table[idx + 1].torque - table[idx].torque;
    float dy = table[idx + 1].iq - table[idx].iq;
    float ratio = (x_val - table[idx].torque) / (dx + 1e-9f);
    return table[idx].iq + ratio * dy;
}

/**
 * @brief Iq 电流 -> 输出力矩
 */
#pragma CODE_SECTION(calib_current_to_torque, "ramfuncs");
float calib_current_to_torque(float iq, const tTorqueCalibPoint *table, uint16_t length)
{
    if (table == NULL || length < 2) {
        return 0.0f;
    }

    float sign = (iq >= 0.0f) ? 1.0f : -1.0f;
    float abs_iq = (iq >= 0.0f) ? iq : -iq;
    float abs_torque = interpolate_iq_to_torque(abs_iq, table, length);

    return sign * abs_torque;
}

/**
 * @brief 输出力矩 -> Iq 电流
 */
#pragma CODE_SECTION(calib_torque_to_current, "ramfuncs");
float calib_torque_to_current(float torque, const tTorqueCalibPoint *table, uint16_t length)
{
    if (table == NULL || length < 2) {
        return 0.0f;
    }

    float sign = (torque >= 0.0f) ? 1.0f : -1.0f;
    float abs_torque = (torque >= 0.0f) ? torque : -torque;
    float abs_iq = interpolate_torque_to_iq(abs_torque, table, length);

    return sign * abs_iq;
}

/**
 * @brief Iq 电流 -> 输出力矩 (使用默认标定表)
 */
#pragma CODE_SECTION(Iq_To_Torque, "ramfuncs");
float Iq_To_Torque(float iq)
{
    return calib_current_to_torque(iq, gTorqueCalibTable, gTorqueCalibTableSize);
}

/**
 * @brief 输出力矩 -> Iq 电流 (使用默认标定表)
 */
#pragma CODE_SECTION(Torque_To_Iq, "ramfuncs");
float Torque_To_Iq(float target_torque)
{
    return calib_torque_to_current(target_torque, gTorqueCalibTable, gTorqueCalibTableSize);
}
