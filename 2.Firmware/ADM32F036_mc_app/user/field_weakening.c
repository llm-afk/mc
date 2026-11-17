#include "field_weakening.h"

/* 开启状态、利用率目标及积分增益为研究值，其余参数采用 Axis 默认值。 */
const field_weakening_config_t field_weakening_config = {
    .enabled = 1U,
    .negative_id_limit_a = 5.0f,
    .voltage_threshold = 0.4f,
    .gain_a_s = 400.0f,
    .dq_current_limit_a = 30.0f
};

field_weakening_status_t field_weakening_status;

/* 复用原电流环使用的整数开平方入口；下面校正其近似值，保证圆限幅不越界。 */
extern uint16_t qsqrt(uint32_t value);

static int32_t id_q16;
static int32_t negative_limit_q16;
static uint16_t current_limit_code;
static uint16_t threshold_q15;
static uint16_t gain_per_count_q20;

#pragma FUNC_CANNOT_INLINE(root32)
static uint16_t root32(uint32_t value)
{
    uint32_t root = qsqrt(value);
    while (root * root > value) --root;
    while (root < 65535UL && (root + 1UL) * (root + 1UL) <= value) ++root;
    return (uint16_t)root;
}

void field_weakening_init(uint16_t current_base_da, uint32_t pwm_clock_hz)
{
    const field_weakening_config_t *p = &field_weakening_config;
    float code_per_a, gain;
    field_weakening_status = (field_weakening_status_t){0};
    id_q16 = 0;
    if (p->enabled > 1U || current_base_da == 0U || pwm_clock_hz == 0UL ||
        !(p->dq_current_limit_a > 0.0f && p->dq_current_limit_a <= 35.0f) ||
        !(p->negative_id_limit_a >= 0.0f && p->negative_id_limit_a <= p->dq_current_limit_a) ||
        !(p->voltage_threshold >= 0.0f && p->voltage_threshold <= 1.0f) ||
        !(p->gain_a_s >= 0.0f)) return;
    code_per_a = 40960.0f / ((float)current_base_da * 1.41421356f);
    /* 中心对齐 PWM 一周期为 2*TBPRD 个时钟。快环只用整数，不调用浮点函数。 */
    gain = p->gain_a_s * code_per_a * 2097152.0f / (float)pwm_clock_hz;
    if (!(gain >= 0.0f && gain <= 65534.0f) ||
        !(p->dq_current_limit_a * code_per_a < 32768.0f)) return;
    current_limit_code = (uint16_t)(p->dq_current_limit_a * code_per_a);
    negative_limit_q16 = -(int32_t)(p->negative_id_limit_a * code_per_a) * 65536L;
    threshold_q15 = (uint16_t)(p->voltage_threshold * 32768.0f + 0.5f);
    gain_per_count_q20 = (uint16_t)(gain + 0.5f);
    field_weakening_status.ready = current_limit_code != 0U;
}

#pragma FUNC_CANNOT_INLINE(field_weakening_prepare)
uint16_t field_weakening_prepare(uint16_t allowed, uint16_t pwm_period,
    int16_t voltage_limit, int32_t base_id, int32_t base_iq)
{
    uint16_t was_active = field_weakening_status.active;
    int32_t id, iq, limit = current_limit_code;
    uint32_t remaining;
    int64_t next;
    if (!allowed || !field_weakening_config.enabled || !field_weakening_status.ready ||
        pwm_period == 0U || voltage_limit <= 0 || base_id != 0L) {
        id_q16 = 0;
        field_weakening_status.active = 0U;
        field_weakening_status.voltage_utilization_q15 = 0UL;
        field_weakening_status.id_target_code = base_id;
        field_weakening_status.iq_target_code = base_iq;
        return was_active;
    }
    /* 与 Axis 一样使用上一拍限幅前利用率积分，Id 始终处于 [-上限, 0]。
     * Q16 保存不足一个电流代码值的增量，避免小误差被整数截断后无法进入弱磁。 */
    next = (int64_t)id_q16 +
        ((int64_t)(((uint32_t)gain_per_count_q20 * pwm_period) >> 4) *
        ((int32_t)threshold_q15 - (int32_t)field_weakening_status.voltage_utilization_q15) >> 15);
    if (next < negative_limit_q16) next = negative_limit_q16;
    if (next > 0L) next = 0L;
    id_q16 = (int32_t)next;
    id = -(int32_t)((uint32_t)(-id_q16) >> 16);
    iq = base_iq > limit ? limit : (base_iq < -limit ? -limit : base_iq);
    remaining = (uint32_t)(limit * limit - id * id);
    if ((uint32_t)(iq * iq) > remaining) {
        int32_t q_limit = root32(remaining);
        iq = iq < 0L ? -q_limit : q_limit;
    }
    field_weakening_status.active = 1U;
    field_weakening_status.id_target_code = id;
    field_weakening_status.iq_target_code = iq;
    return 1U;
}

#pragma FUNC_CANNOT_INLINE(field_weakening_voltage)
void field_weakening_voltage(int16_t *vd, int16_t *vq, int16_t voltage_limit)
{
    int32_t d, q, limit = voltage_limit;
    uint32_t magnitude;
    if (!field_weakening_status.active || voltage_limit <= 0) return;
    d = *vd;
    q = *vq;
    magnitude = root32((uint32_t)(d * d) + (uint32_t)(q * q));
    field_weakening_status.voltage_utilization_q15 = (magnitude << 15) / (uint16_t)limit;
    if (field_weakening_status.id_target_code < 0L) {
        int32_t q_limit;
        /* 保留负 Id 所需的 d 电压，避免 q 饱和时原径向限幅把弱磁电压一起压低。 */
        d = d > limit ? limit : (d < -limit ? -limit : d);
        q_limit = root32((uint32_t)(limit * limit - d * d));
        q = q > q_limit ? q_limit : (q < -q_limit ? -q_limit : q);
        *vd = (int16_t)d;
        *vq = (int16_t)q;
    }
}
