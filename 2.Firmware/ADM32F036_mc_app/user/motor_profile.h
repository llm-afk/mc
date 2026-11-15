#ifndef MC_MOTOR_PROFILE_H
#define MC_MOTOR_PROFILE_H

#include <stdint.h>

/* 内部电机参数：默认值在 motor_profile.c 的 motor_parameters 中逐项配置。
 * 修改后重新编译、烧录；参数不新增通信对象，也不改变原 EEPROM 保存格式。
 */
typedef struct {
    uint16_t pole_pairs;               // 电机极对数，范围 1～64；10 表示 20 个磁极
    uint16_t gear_numerator;           // 减速比分子；电机转数/关节转数 = 分子/分母
    uint16_t gear_denominator;         // 减速比分母，必须大于 0；12/1 表示 12:1
    uint16_t main_teeth;               // 主编码器齿数，范围 1～254
    uint16_t aux_teeth;                // 副编码器齿数；沿用原算法，必须等于主齿数加 1
    uint16_t output_reverse;           // 关节方向：0=原正向，1=控制与上报同时反向；校准不改写
    uint16_t current_base_da;          // 原库电流归一化基值，单位 0.1 A；280 表示 28.0 A
    uint16_t current_limit_ma;         // MIT 控制电流上限，单位 mA；最大沿用原板的 35000 mA
    uint16_t calibration_current_ma;   // 校准 d 轴电流，单位 mA；不得超过控制电流上限
    uint16_t torque_limit_mnm;         // 关节力矩上限，单位 mN·m；30000 表示 30 N·m
    uint16_t resistance_mohm;          // 原库 PMSM_RS 电阻参数，单位 mΩ；独立于温度估算基准
    uint16_t ld_uh;                    // 原库 PMSM_LD 的 d 轴电感，单位 μH
    uint16_t lq_uh;                    // 原库 PMSM_LQ 的 q 轴电感，单位 μH
    uint16_t initial_electrical_zero;  // 无旧校准记录时的零偏，单位为 14 位编码器计数（0～16383）
} motor_parameters_t;

/* 启动时计算一次的控制用参数；中断直接读取，不重复做参数换算。 */
typedef struct {
    uint16_t pole_pairs;               // 电角度和校准扫描使用的极对数
    uint16_t main_teeth;               // 主编码器齿数
    uint16_t aux_teeth;                // 副编码器齿数，用于原多圈位置换算
    uint16_t output_reverse;           // 控制与反馈统一使用的关节方向位
    uint16_t calibration_current_code; // 换算后的原库 Id/Iq 代码值；默认保持 1000
    uint16_t probe_electrical_step;    // 校准方向探测的单次电角度增量；10 对极时为 256
    float gear_ratio;                 // 减速比，等于分子/分母
    float gear_ratio_inverse;         // 减速比倒数，用于电机速度换算为关节速度
    float current_limit_a;            // 控制电流上限，单位 A
    float torque_limit_nm;            // 关节力矩上限，单位 N·m
    int64_t position_scale_q30;        // 原关节位置换算系数，Q30 定点格式
    int64_t position_bias_q14;         // 原关节位置坐标偏置，Q14 格式，单位 rad
} motor_profile_t;

extern const motor_parameters_t motor_parameters; // 内部配置，默认值统一在 .c 文件中修改
extern motor_profile_t motor_profile;             // 初始化后供实际控制逻辑读取的派生参数

/* 在外设、电机和 EEPROM 初始化前调用；成功返回 0，参数不合法返回 -1。 */
int motor_profile_init(void);

#endif
