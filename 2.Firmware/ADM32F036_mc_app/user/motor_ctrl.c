#include "motor_ctrl.h"
#include "MotorInclude.h"
#include "utils.h"

#define HAS_ERR(e) (ODObjs.error_code & (e))

motor_ctrl_t motor_ctrl = {
    .state = INIT,
};

// 前置声明
static void motor_fault_shutdown(tErrorCode err);
static void motor_fault_recover(tErrorCode err);
void set_err(tErrorCode err);
void clr_err(tErrorCode err);

// 故障斜坡状态（MIT 内部使用）
static int32_t fault_ramp_tick      = 0;
static int32_t fault_ramp_iq_start  = 0;
static int32_t fault_ramp_id_start  = 0;

#pragma CODE_SECTION(MC_controlword_update, "ramfuncs");
int MC_controlword_update(void)
{
    switch(ODObjs.control_word & 0x00FF)
    {
        case CW_CMD_OPERATION_ENABLE: // 使能电机
        {
            if(RunSignal == 0)
            {
                RunSignal = 1;
            }
            break;
        }
        case CW_CMD_OPERATION_DISABLE: // 失能电机
        {
            if(RunSignal == 1)
            {
                RunSignal = 0;
            }
            break;
        }
        case CW_CMD_RESET_HOME: // 复位原点
        {
            break;
        }
        case CW_CMD_ERROR_RESET: // 错误清除
        {
            ODObjs.error_code = 0;
            // 【FIX】如果电机已经停稳，允许通过复位指令回到 INIT 重新待命
            if(motor_ctrl.state == STOPPED) 
            {
                motor_ctrl.state = INIT;
                fault_ramp_tick = 0;
            }
            break;
        }
        case CW_CMD_DEV_ENCODER_CALIB: // 编码器校准
        {
            motor_ctrl.state = ENCODER_CALIBRATE;
            break;
        }
        default:
        {
            break;
        }
    }
    ODObjs.control_word &= 0xFF00;
    return 0;
}

/**
 * @brief 电机状态控制主循环
 * @note 放在stimer框架下2Khz循环执行
 */
#pragma CODE_SECTION(MC_servo_loop, "ramfuncs");
void MC_servo_loop(void)
{
    // =========================================================================
    // 【终极防溢出 & 零精度损失计算法】
    // 1. 获取纯净的总 Tick 数 (64位)，永不溢出
    // =========================================================================
    int64_t total_ticks = ((int64_t)encoder.enc_turns << 14)  // 等效于 * 16384
                        + (int32_t)((int16_t)encoder.enc_degree_lined + (int16_t)encoder.in_enc_deg_zero)
                        - (int32_t)encoder.error * 18;

    // =========================================================================
    // 2. 将浮点乘除转换为 Q30 格式的 64位定点乘法
    // =========================================================================
    encoder.degree_q14 = (-(total_ticks * 562203932LL) >> 30) + 8565;
    
    // 假设 GEAR_RATIO_INV 是常量宏
    encoder.velocity_q14 = (int32_t)(encoder.enc_velocity_q14 * GEAR_RATIO_INV);

    // =========================================================================
    // 【优化 2】优化 64 位数据的绝对值计算与突变滤波
    // =========================================================================
    static int64_t degree_q14_last = 0;
    static int16_t first_flag = 0;
    
    if(!first_flag) 
    {
        first_flag = 1; 
    }
    else
    {
        int64_t diff = encoder.degree_q14 - degree_q14_last;
        if(diff > 1000 || diff < -1000) 
        {
            encoder.degree_q14 = degree_q14_last;
        }
    }
    degree_q14_last = encoder.degree_q14;
    
    switch(motor_ctrl.state)
    {
        case INIT:
        {
            // 如果存在未清除的错误，不允许进入 MIT
            if(ODObjs.error_code == 0)
            {
                motor_ctrl.state = MIT;
            }
            break;
        }
        case MIT:
        {
            // =========================================================================
            // 【核心优化 3】64位安全降维，激活 DSP 单周期乘法器
            // =========================================================================
            int64_t raw_degree_err = motor_ctrl.degree_ref_q14 - encoder.degree_q14;
            int64_t raw_velocity_err = motor_ctrl.velocity_ref_q14 - encoder.velocity_q14;

            int32_t degree_err_q14;
            if(raw_degree_err > 163840) degree_err_q14 = 163840;
            else if(raw_degree_err < -163840) degree_err_q14 = -163840;
            else degree_err_q14 = (int32_t)raw_degree_err;

            int32_t velocity_err_q14;
            if(raw_velocity_err > 1638400) velocity_err_q14 = 1638400;
            else if(raw_velocity_err < -1638400) velocity_err_q14 = -1638400;
            else velocity_err_q14 = (int32_t)raw_velocity_err;

            // =========================================================================
            // 【优化 4】编译期合并常量
            // =========================================================================
            #define MIT_TERM_SCALE (MOTOR_RATED_CUR * 1.41421356f / 40960.0f)
            #define MIT_IQ_SCALE   (40960.0f / (MOTOR_RATED_CUR * 1.41421356f))

            int32_t p_term = (int32_t)( ( ((int64_t)motor_ctrl.Kp_q14 * degree_err_q14) >> 14 ) * MIT_TERM_SCALE );
            int32_t d_term = (int32_t)( ( ((int64_t)motor_ctrl.Kd_q14 * velocity_err_q14) >> 14 ) * MIT_TERM_SCALE );
            
            int32_t out_q14 = p_term + d_term + motor_ctrl.torque_ref_q14; 

            // =========================================================================
            // 【优化 5】限幅链
            // =========================================================================
            #define MOTOR_TORQUE_MAX  30.0f                                   
            #define DRV_IQ_MAX        ((int32_t)(35.0f * MIT_IQ_SCALE))      

            float t_limit = ODObjs.torque_limit;
            t_limit = (t_limit > MOTOR_TORQUE_MAX) ? MOTOR_TORQUE_MAX : ((t_limit < 0.0f) ? 0.0f : t_limit);
            ODObjs.torque_limit = t_limit;

            int32_t out_limit = (int32_t)(t_limit * 16384.0f);
            out_q14 = (out_q14 > out_limit) ? out_limit : ((out_q14 < -out_limit) ? -out_limit : out_q14);

            Iq = (int32_t)(Torque_To_Iq((float)(-out_q14) * 0.00006103515625f) * MIT_IQ_SCALE);
            Iq = (Iq > DRV_IQ_MAX) ? DRV_IQ_MAX : ((Iq < -DRV_IQ_MAX) ? -DRV_IQ_MAX : Iq);
            Id = 0;

            // =========================================================================
            // 【FIX】软急停闭环保护：确保 S 曲线衰减不被打断
            // =========================================================================
            #define FAULT_RAMP_TICKS 2000  // @ 2kHz = 1 秒
            
            // 只要有错误，或者急停过程已经开始，就必须强制走完斜坡，拒绝中途切回 INIT
            if(ODObjs.error_code != 0 || fault_ramp_tick > 0)
            {
                if(fault_ramp_tick == 0)
                {
                    // 仅在首次触发急停时捕获当前电流。防止多重故障引发的多次捕获导致电流突跳
                    fault_ramp_iq_start = Iq;
                    fault_ramp_id_start = Id;
                }
                
                if(fault_ramp_tick < FAULT_RAMP_TICKS)
                {
                    float t = (float)fault_ramp_tick * (1.0f / (float)FAULT_RAMP_TICKS);
                    float s = t * t * (3.0f - 2.0f * t);  // smoothstep 平滑衰减
                    Iq = (int32_t)((float)fault_ramp_iq_start * (1.0f - s));
                    Id = (int32_t)((float)fault_ramp_id_start * (1.0f - s));
                    fault_ramp_tick++;
                }
                else
                {
                    // 斜坡走完，电流彻底归零
                    Iq = 0;
                    Id = 0;
                    motor_ctrl.state = STOPPED; 
                }
            }
            else
            {
                fault_ramp_tick = 0;  // 无故障且正常运行，保持复位
            }
            break;
        }
        case ENCODER_CALIBRATE:
        {
            if(RunSignal == 0)
            {
                RunSignal = 1;
            }
            if(encoder_calibrate() == 1)
            {
                RunSignal = 0;
                motor_ctrl.state = MIT;
                clr_err(ERR_ENC_CALIB);
            }
            break;
        }
        case ENCODER_ZERO:
        {
            break;
        }
        case STOPPED:
        {
            Iq = 0;
            Id = 0;
            // 【FIX】当电机已经完全停稳且所有错误标志已被后台清除，自动切回 INIT 待命
            if (ODObjs.error_code == 0) 
            {
                motor_ctrl.state = INIT;
                fault_ramp_tick = 0;
            }
            break;
        }
        default :
        {
            break;
        }
    }
}

/**
 * @brief 置位错误
 */
void set_err(tErrorCode err)
{
    if(!(ODObjs.error_code & err)) 
    {
        ODObjs.error_code |= err;
    }
}

/**
 * @brief 清除错误
 */
void clr_err(tErrorCode err)
{
    if(ODObjs.error_code & err) 
    {
        ODObjs.error_code &= ~err; 
    }
}

/**
 * @brief 统一保护关机 —— 报错
 */
static void motor_fault_shutdown(tErrorCode err)
{
    set_err(err);
    // 【FIX】这里不再清零 fault_ramp_tick。
    // 让 MIT 状态机根据故障标志位自主启动斜坡；如果是多重故障连发，也能保证斜坡平稳延续不被打断。
}

/**
 * @brief 统一保护恢复 —— 清错
 */
static void motor_fault_recover(tErrorCode err)
{
    clr_err(err);
    // 【FIX】不再在这里强制干涉电机状态，状态转换统一交给 MC_servo_loop 里的 STOPPED 判断处理。
    // 这样能确保只有电机完全停稳后，才能恢复运行，彻底杜绝急停被半路打断的抽动问题。
}

float board_temp;
float motor_temp;
float under_v_level = 20.0f;

#define ADC_MAX 4095.0f
#define INV_T25     0.0033540164f  // 1.0f / 298.15f
#define INV_BETA    0.0002902758f  // 1.0f / 3445.0f

extern uint16_t heatbeat_flag;
extern float torque;
float i2t_acc = 0.0f;

/**
 * @brief 采集电机控制相关数据
 * @note 放在stimer框架下100hz循环执行
 */
void info_collect_loop(void)
{
    static float board_temp_filt;
    static float motor_temp_filt;
    static uint16_t temp_filt_inited = 0;
    const float alpha = 0.001f;

    // 数学化简逻辑保持不变
    float adc_drv = (float)ADC_NTC;   
    float adc_mot = (float)ADC_NTC_M; 
    float ratio_drv = adc_drv / (4095.0f - adc_drv);
    float ratio_mot = adc_mot / (4095.0f - adc_mot);
    float board_temp_raw = 1.0f / (INV_T25 + INV_BETA * logf(ratio_drv)) - 273.15f;
    float motor_temp_raw = 1.0f / (INV_T25 + INV_BETA * logf(ratio_mot)) - 273.15f;

    if (!temp_filt_inited)
    {
        board_temp_filt = board_temp_raw;
        motor_temp_filt = motor_temp_raw;
        temp_filt_inited = 1;
    }
    else
    {
        board_temp_filt += alpha * (board_temp_raw - board_temp_filt);
        motor_temp_filt += alpha * (motor_temp_raw - motor_temp_filt);
    }
    board_temp = board_temp_filt;
    motor_temp = motor_temp_filt;

    /* -------- 统一保护逻辑 -------- */
    
    // 1. 电机过温保护
    if(!(HAS_ERR(ERR_OVER_TEMP_MOTOR)) && (motor_temp > ODObjs.over_temp_motor_level))
    {
        motor_fault_shutdown(ERR_OVER_TEMP_MOTOR);
    }
    if(HAS_ERR(ERR_OVER_TEMP_MOTOR)) // 【修复括号补齐】
    {
        if(motor_temp < ODObjs.over_temp_motor_level - 20.0f)
        {
            motor_fault_recover(ERR_OVER_TEMP_MOTOR);
        }
    }

    // 2. 驱动器过温保护
    if(!(HAS_ERR(ERR_OVER_TEMP_DRV)) && (board_temp > ODObjs.over_temp_drv_level))
    {
        motor_fault_shutdown(ERR_OVER_TEMP_DRV);
    }
    if(HAS_ERR(ERR_OVER_TEMP_DRV)) // 【修复括号补齐】
    {
        if(board_temp < ODObjs.over_temp_drv_level - 20.0f)
        {
            motor_fault_recover(ERR_OVER_TEMP_DRV);
        }
    }

    // 【FIX】6. 欠压保护（移出 CAN 10秒盲区，确保硬件上电立刻处于保护监测中）
    if(!(HAS_ERR(ERR_UNDER_VOLTAGE)) && ((gUDC.uDCFilter * 0.1f) < under_v_level))
    {
        motor_fault_shutdown(ERR_UNDER_VOLTAGE);
    }
    if(HAS_ERR(ERR_UNDER_VOLTAGE)) // 【修复括号补齐】
    {
        if((gUDC.uDCFilter * 0.1f) > under_v_level + 2.0f)
        {
            motor_fault_recover(ERR_UNDER_VOLTAGE);
        }
    }

    /* -------- I²t 算法高级热保护（堵转保护） -------- */
    const float dt = 0.01f; 
    const float t_trip_test = 30.0f;
    const float t_rated = 28.0f;
    const float t_trip_time = 10.0f;
    const float i2t_threshold = (t_trip_test * t_trip_test - t_rated * t_rated) * t_trip_time;

    float current_sq = torque * torque;
    float rated_sq = t_rated * t_rated;
    i2t_acc += (current_sq - rated_sq) * dt;

    if(i2t_acc < 0.0f) i2t_acc = 0.0f; 

    // 3. I²t 过流保护（手动清除）
    if(!(HAS_ERR(ERR_OVER_CURRENT_SOFT)) && (i2t_acc > i2t_threshold))
    {
        motor_fault_shutdown(ERR_OVER_CURRENT_SOFT);
    }

    /* -------- CAN 状态机检测 -------- */
    static uint16_t can_delay_cnt = 0;
    if(can_delay_cnt < 1000)
    {
        can_delay_cnt++;
    }
    else
    {
        // 4. 心跳超时保护（CAN FD 帧超时 2.5s）
        if(ODObjs.heartbeat_consumer_enable)
        {
            // 只要没到250，就正常累加；到了250就锁死，等待CAN接收中断将其清零
            if(canfd_timeout_cnt < 250)
            {
                canfd_timeout_cnt++;
            }

            // 判断当前状态
            if(canfd_timeout_cnt >= 250)
            {
                canfd_frame_flag = 1;
                motor_fault_shutdown(ERR_HEARTBEAT_TIMEOUT);
            }
            else
            {
                // 如果 cnt < 250，说明外部接收中断刚刚清零过它，通信已经恢复！
                if(canfd_frame_flag && HAS_ERR(ERR_HEARTBEAT_TIMEOUT))
                {
                    motor_fault_recover(ERR_HEARTBEAT_TIMEOUT);
                    canfd_frame_flag = 0;
                }
            }
        }

        // 5. CAN Bus-Off 保护
        static uint32_t can_buf_off_cnt = 0;
        if(CanfdRegs.CFG_STAT.bit.BUSOFF)
        {
            can_buf_off_cnt++;
            if(can_buf_off_cnt > 100)
            {
                can_buf_off_cnt = 0;
                canfd_buf_off_flag = 1;
                motor_fault_shutdown(ERR_HEARTBEAT_TIMEOUT);
            }
        }
        else
        {
            if(canfd_buf_off_flag && HAS_ERR(ERR_HEARTBEAT_TIMEOUT))
            {
                motor_fault_recover(ERR_HEARTBEAT_TIMEOUT);
                canfd_buf_off_flag = 0;
            }
            can_buf_off_cnt = 0;
        }
    }

    /* -------- 心跳帧 -------- */
    static uint16_t heartbeat_cnt = 0;
    if(++heartbeat_cnt >= 100) 
    {
        heartbeat_cnt = 0;
        heatbeat_flag = 1; 
    }
}