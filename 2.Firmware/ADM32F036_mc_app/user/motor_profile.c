#include "motor_profile.h"
#include "Parameter.h"
#include "encoder.h"

/* 修改此处的结构体参数即可适配电机，关节方向由 output_reverse 指定。 */
const motor_parameters_t motor_parameters = {
    .pole_pairs = 10U,                  // 极对数：当前电机为 10 对极
    .gear_numerator = 12U,              // 减速比分子：默认 12/1，即 12:1
    .gear_denominator = 1U,             // 减速比分母：必须大于 0
    .main_teeth = 17U,                  // 主编码器齿数
    .aux_teeth = 18U,                   // 副编码器齿数：原算法要求比主齿数大 1
    .output_reverse = 1U,               // 关节方向：0=原正向，1=指令及位置/速度/力矩上报反向
    .current_base_da = 280U,            // 原库电流基值：0.1 A/单位，默认 28.0 A
    .current_limit_ma = 35000U,         // MIT 控制电流上限：mA，默认 35 A
    .calibration_current_ma = 9667U,    // 校准 d 轴电流：mA，换算后保持原 Id=1000
    .torque_limit_mnm = 30000U,         // 关节力矩上限：mN·m，默认 30 N·m
    .resistance_mohm = 269U,            // 原库电阻参数：mΩ，默认 0.269 Ω
    .ld_uh = 270U,                     // d 轴电感：μH，默认 270 μH
    .lq_uh = 326U,                     // q 轴电感：μH，默认 326 μH
    .initial_electrical_zero = 1675U    // 无旧校准记录时的初始零偏：14 位编码器计数
};

motor_profile_t motor_profile;

int motor_profile_init(void)
{
    const motor_parameters_t *p = &motor_parameters;
    uint64_t calibration_code;

    /* 参数在启动阶段检查，保持原板限流与原编码器算法的有效范围。 */
    if (p->pole_pairs < 1U || p->pole_pairs > 64U ||
        p->gear_denominator < 1U || p->gear_numerator < p->gear_denominator ||
        (uint32_t)p->gear_numerator > 4096UL * p->gear_denominator ||
        p->main_teeth < 1U || p->main_teeth > 254U || p->aux_teeth != p->main_teeth + 1U ||
        p->output_reverse > 1U || p->current_base_da < 1U || p->current_base_da > 4095U ||
        p->current_limit_ma < 1U || p->current_limit_ma > 35000U ||
        p->calibration_current_ma < 1U || p->calibration_current_ma > p->current_limit_ma ||
        p->torque_limit_mnm < 1U || p->torque_limit_mnm > 30000U ||
        p->resistance_mohm < 1U || p->ld_uh < 1U || p->lq_uh < 1U ||
        p->initial_electrical_zero >= 16384U)
    {
        return -1;
    }

    /* 保留原电流基值和四舍五入方式；默认 9667 mA 对应原库代码值 1000。 */
    calibration_code = ((uint64_t)p->calibration_current_ma * 40960000ULL +
                        (uint64_t)p->current_base_da * 707107ULL) /
                       ((uint64_t)p->current_base_da * 1414214ULL);
    if (calibration_code < 1ULL || calibration_code > 32767ULL) return -1;

    motor_profile.pole_pairs = p->pole_pairs;
    motor_profile.main_teeth = p->main_teeth;
    motor_profile.aux_teeth = p->aux_teeth;
    motor_profile.output_reverse = p->output_reverse;
    motor_profile.calibration_current_code = (uint16_t)calibration_code;
    motor_profile.probe_electrical_step = (256U * p->pole_pairs + 5U) / 10U;
    motor_profile.gear_ratio = (float)p->gear_numerator / p->gear_denominator;
    motor_profile.gear_ratio_inverse = (float)p->gear_denominator / p->gear_numerator;
    motor_profile.current_limit_a = p->current_limit_ma / 1000.0f;
    motor_profile.torque_limit_nm = p->torque_limit_mnm / 1000.0f;
    /* 沿用 245 的坐标与整数舍入约定，按减速比换算，默认值逐位一致。 */
    motor_profile.position_scale_q30 = 562203932LL * 12 * p->gear_denominator / p->gear_numerator;
    motor_profile.position_bias_q14 = 8565LL * 12 * p->gear_denominator / p->gear_numerator;

    /* 在实际电流环及其参数换算初始化之前，接入原库正在使用的变量。 */
    MOTOR_RATED_CUR = p->current_base_da;
    PMSM_LD = p->ld_uh;
    PMSM_LQ = p->lq_uh;
    PMSM_RS = p->resistance_mohm;
    encoder_config.elec_degree_calib = p->initial_electrical_zero;
    return 0;
}
