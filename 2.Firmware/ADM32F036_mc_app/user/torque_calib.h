#ifndef __TORQUE_CALIB_H__
#define __TORQUE_CALIB_H__

#include <stdint.h>

/**
 * @brief 标定点结构体 (正半轴, 第一象限)
 * @note  标定表仅需存放 Iq >= 0 的数据, 负半轴由函数内部自动对称处理
 */
typedef struct {
    float iq;     // 等幅值 q 轴电流 [A]
    float torque; // 电机输出端实际测量力矩 [Nm]
} tTorqueCalibPoint;

/** 默认力矩标定表 (定义在 torque_calib.c) */
extern const tTorqueCalibPoint gTorqueCalibTable[];
extern const uint16_t gTorqueCalibTableSize;

/**
 * @brief 将等幅值 q 轴电流转换为电机输出端实际力矩 (使用默认标定表)
 * @param iq 等幅值 q 轴电流 [A]（正负均可）
 * @return 电机输出端力矩 [Nm]（与 iq 同符号）
 */
float calib_current_to_torque(float iq, const tTorqueCalibPoint *table, uint16_t length);
float Iq_To_Torque(float iq);

/**
 * @brief 将电机输出端实际力矩反向转换为等幅值 q 轴电流 (使用默认标定表)
 * @param torque 电机输出端实际力矩 [Nm]（正负均可）
 * @return 等幅值 q 轴电流 [A]（与 torque 同符号）
 */
float calib_torque_to_current(float torque, const tTorqueCalibPoint *table, uint16_t length);
float Torque_To_Iq(float target_torque);

#endif /* __TORQUE_CALIB_H__ */
