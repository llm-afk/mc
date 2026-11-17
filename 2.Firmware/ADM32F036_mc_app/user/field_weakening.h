#ifndef MC_FIELD_WEAKENING_H
#define MC_FIELD_WEAKENING_H

#include <stdint.h>

typedef struct {
    uint16_t enabled;              // 弱磁开关：0=关闭，1=开启；本研究分支默认开启
    float negative_id_limit_a;     // 负 d 轴电流幅值上限，单位 A；Axis 默认 5 A
    float voltage_threshold;       // 电压利用率目标，相对于可用电压；研究值 0.4，Axis 默认 0.98
    float gain_a_s;                // 弱磁利用率积分增益，单位 A/s；研究值 400，Axis 默认 200
    float dq_current_limit_a;      // d/q 总电流圆半径，单位 A；Axis 默认 30 A
} field_weakening_config_t;

typedef struct {
    uint16_t ready;                // 配置及电流/时钟换算有效时为 1
    volatile uint16_t active;      // 当前快环正在执行 MIT 弱磁时为 1；CAN 前台也会读取
    int32_t id_target_code;        // 分配后的 d 轴给定，原库电流代码值；弱磁时不大于 0
    int32_t iq_target_code;        // 分配后的 q 轴给定，原库电流代码值，保留指令符号
    uint32_t voltage_utilization_q15; // 上一拍限幅前 Vdq / 可用电压；32768 表示 100%
} field_weakening_status_t;

extern const field_weakening_config_t field_weakening_config;
extern field_weakening_status_t field_weakening_status;

/* 上电、开启中断之前调用；current_base_da 沿用 MOTOR_RATED_CUR 的 0.1 A 单位。 */
void field_weakening_init(uint16_t current_base_da, uint32_t pwm_clock_hz);

/* 快环唯一写入者。返回 1 时将状态中的 d/q 给定写到实际电流环入口。
 * 退出弱磁的第一拍返回原始给定，清除本模块此前注入的负 Id；校准不执行弱磁。 */
uint16_t field_weakening_prepare(uint16_t allowed, uint16_t pwm_period,
    int16_t voltage_limit, int32_t base_id, int32_t base_iq);

/* 在 CurrentLoop 之后、原库 VolVectCalation 之前调用。
 * 记录限幅前电压利用率；负 Id 时优先保留 d 电压，q 使用剩余电压圆。 */
void field_weakening_voltage(int16_t *vd, int16_t *vq, int16_t voltage_limit);

#endif
