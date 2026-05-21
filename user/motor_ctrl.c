#include "motor_ctrl.h"
#include "utils.h"

motor_ctrl_t motor_ctrl = {
    .state = INIT,    
};

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
    // 即使电机以 3000 RPM 运转，int64_t 足以让它连续单向运行超 100 万圈不溢出。
    // =========================================================================
    int64_t total_ticks = ((int64_t)encoder.enc_turns << 14)  // 等效于 * 16384
                        + (int32_t)((int16_t)encoder.enc_degree_lined + (int16_t)encoder.in_enc_deg_zero)
                        - (int32_t)encoder.error * 18;

    // =========================================================================
    // 2. 将浮点乘除转换为 Q30 格式的 64位定点乘法 (彻底消灭浮点数导致的精度吃光现象)
    // 原公式推导：(total_ticks / 16384) / 12 * TWO_PI * 16384 => total_ticks * (TWO_PI / 12)
    // TWO_PI / 12.0 = 0.523598775598，乘以 2^30 转为 Q30 定点常数：562203932LL
    // =========================================================================
    encoder.degree_q14 = (-(total_ticks * 562203932LL) >> 30) + 8565;
    
    // 假设 GEAR_RATIO_INV 是常量宏，此处保留原意
    encoder.velocity_q14 = (int32_t)(encoder.enc_velocity_q14 * GEAR_RATIO_INV);

    // =========================================================================
    // 【优化 2】优化 64 位数据的绝对值计算与突变滤波
    // =========================================================================
    static int64_t degree_q14_last = 0;
    static int16_t first_flag = 0;
    
    if(!first_flag) 
    {
        first_flag = 1; // 替换自增，防止多年运行后溢出
    }
    else
    {
        // 纯整数减法和逻辑判断，避开调用 64 位宽的 INT_ABS 库函数开销
        int64_t diff = encoder.degree_q14 - degree_q14_last;
        if(diff > 1000 || diff < -1000) // 滤除50rps以上的瞬时错误角度数据
        {
            encoder.degree_q14 = degree_q14_last;
        }
    }
    degree_q14_last = encoder.degree_q14;
    
    switch(motor_ctrl.state)
    {
        case INIT:
        {
            motor_ctrl.state = MIT;
            break;
        }
        case MIT:
        {
            // =========================================================================
            // 【核心优化 3】64位安全降维，激活 DSP 的 32x32 硬件单周期乘法器
            // =========================================================================
            int64_t raw_degree_err = motor_ctrl.degree_ref_q14 - encoder.degree_q14;
            int64_t raw_velocity_err = motor_ctrl.velocity_ref_q14 - encoder.velocity_q14;

            // 拦截 64 位超大误差，安全降维至 32 位，断绝后续溢出可能
            int32_t degree_err_q14;
            if(raw_degree_err > 163840) degree_err_q14 = 163840;
            else if(raw_degree_err < -163840) degree_err_q14 = -163840;
            else degree_err_q14 = (int32_t)raw_degree_err;

            int32_t velocity_err_q14;
            if(raw_velocity_err > 1638400) velocity_err_q14 = 1638400;
            else if(raw_velocity_err < -1638400) velocity_err_q14 = -1638400;
            else velocity_err_q14 = (int32_t)raw_velocity_err;

            // =========================================================================
            // 【优化 4】编译期合并常量，减少运行时的浮点运算
            // =========================================================================
            #define MIT_TERM_SCALE (MOTOR_RATED_CUR * 1.41421356f / 40960.0f)
            #define MIT_IQ_SCALE   (40960.0f / (MOTOR_RATED_CUR * 1.41421356f))

            // 此时 err 是 32 位，Kp 是 32 位。它们相乘刚好利用 DSP 单周期指令 IMACL
            int32_t p_term = (int32_t)( ( ((int64_t)motor_ctrl.Kp_q14 * degree_err_q14) >> 14 ) * MIT_TERM_SCALE );
            int32_t d_term = (int32_t)( ( ((int64_t)motor_ctrl.Kd_q14 * velocity_err_q14) >> 14 ) * MIT_TERM_SCALE );
            
            int32_t out_q14 = p_term + d_term + motor_ctrl.torque_ref_q14; 

            // =========================================================================
            // 【优化 5】纯净的限幅与倒数相乘算法
            // =========================================================================
            float t_limit = ODObjs.torque_limit;
            
            // 浮点域限幅：利用三目运算符激发底层硬件比较优化
            t_limit = (t_limit > 30.0f) ? 30.0f : ((t_limit < 0.0f) ? 0.0f : t_limit);
            ODObjs.torque_limit = t_limit; 

            // 转为整型边界
            int32_t out_limit = (int32_t)(t_limit * 16384.0f); 
            
            // 整型限幅，彻底避开对动态边界调用复杂的浮点比较库
            out_q14 = (out_q14 > out_limit) ? out_limit : ((out_q14 < -out_limit) ? -out_limit : out_q14);
            
            // 除以 16384 优化为乘以常数倒数 0.00006103515625f，极速计算 Iq
            Iq = Torque_To_Iq((float)(-out_q14) * 0.00006103515625f) * MIT_IQ_SCALE; 
            Id = 0;

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
            }
            break;
        }
        case ENCODER_ZERO: 
        {
            break;
        }
        case SOFT_STOP:
        {
            DisableDrive();
            motor_ctrl.state = STOPPED;
            break;
        }
        case STOPPED:
        {
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
 * @param err 错误类型
 */
void set_err(tErrorCode err)
{
    if(ODObjs.error_code & err) // 当前错误已存在
    {
        return; 
    }
    else
    {
        ODObjs.error_code |= err; // 置位该错误
    }
}

/**
 * @brief 清除错误
 * @param err 错误类型
 */
void clr_err(tErrorCode err)
{
    if(ODObjs.error_code & err) // 当前错误已存在
    {
        ODObjs.error_code &= ~err; // 清除该错误
    }
}

float board_temp;
float motor_temp;

float under_v_level = 20.0f;

#define MOTOR_TEMP_RECOVER 70
#define BOARD_TEMP_RECOVER 50

#define ADC_MAX 4095.0f
// 提前计算好常数倒数，将运行时的“除法”转化为“乘法”
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

    /* -------- 温度解算核心优化 -------- */
    // 提取 ADC 值到局部变量，防止编译器多次读取外设寄存器导致开销增加
    float adc_drv = (float)ADC_NTC;   
    float adc_mot = (float)ADC_NTC_M; 

    // 【数学化简 1】：
    // 原逻辑: r_ntc = 10000.0 * ADC / (4095.0 - ADC); 
    //         logf(r_ntc / 10000.0)
    // 这里的 10k 上拉电阻和 10k 基准电阻在代数上完美抵消。
    // 化简后: logf(ADC / (4095.0 - ADC))，直接省去了一次乘法和一次除法！
    float ratio_drv = adc_drv / (4095.0f - adc_drv);
    float ratio_mot = adc_mot / (4095.0f - adc_mot);

    // 【数学化简 2】：
    // 原逻辑: 1.0f / (1.0f / 298.15f + logf(...) / 3445.0f)
    // 利用预编译常量，将两次极慢的 float 变量除法变为了 float 乘法和加法。
    float board_temp_raw = 1.0f / (INV_T25 + INV_BETA * logf(ratio_drv)) - 273.15f;
    float motor_temp_raw = 1.0f / (INV_T25 + INV_BETA * logf(ratio_mot)) - 273.15f;

    /* -------- 一阶滤波（带初始化） -------- */
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

    /* -------- 保护逻辑优化 -------- */
    // 使用逻辑与 (&&) 合并嵌套的 if。
    // 编译器会利用短路求值（Short-circuit evaluation）特性，若错误码已置位，直接跳过温度浮点比较，减少分支流水线清空。
    if(!(ODObjs.error_code & ERR_OVER_TEMP_MOTOR) && (motor_temp > ODObjs.over_temp_motor_level))
    {
        set_err(ERR_OVER_TEMP_MOTOR);
        motor_ctrl.state = SOFT_STOP;
    }
    if(ODObjs.error_code & ERR_OVER_TEMP_MOTOR)
    {
        if(motor_temp < MOTOR_TEMP_RECOVER)
        {
            clr_err(ERR_OVER_TEMP_MOTOR);
            EnableDrive();
            motor_ctrl.state = INIT;
        }
    }

    if(!(ODObjs.error_code & ERR_OVER_TEMP_DRV) && (board_temp > ODObjs.over_temp_drv_level))
    {
        set_err(ERR_OVER_TEMP_DRV);
        motor_ctrl.state = SOFT_STOP;
    }
    if(ODObjs.error_code & ERR_OVER_TEMP_DRV)
    {
        if(board_temp < BOARD_TEMP_RECOVER)
        {
            clr_err(ERR_OVER_TEMP_DRV);
            EnableDrive();
            motor_ctrl.state = INIT;
        }
    }
/* -------- I?t 算法高级热保护（堵转保护） -------- */
    // 效果：满量程(30.0N) 持续 5s 判定过流。
    // 设定 28.0N 为发热死区(连续容忍电流)。只要低于 28N 积分就不会累积。
    // static float i2t_acc = 0.0f; // 注意：此变量需在函数外或作为静态变量定义
    
    const float dt = 0.01f; // 100Hz 循环时间
    
    // 1. 基准测试点直接锚定 30.0N (不再打 95% 折扣)
    const float t_trip_test = 30.0f; 
    
    // 2. 持续容忍死区拉高到 28.0N
    const float t_rated = 28.0f;  
    
    // 3. 测试点容忍时间
    const float t_trip_time = 5.0f; 
    
    // 4. 自动计算积分阈值
    // 数学过程: (30^2 - 28^2) * 5.0 = (900 - 784) * 5.0 = 116 * 5.0 = 580.0
    const float i2t_threshold = (t_trip_test * t_trip_test - t_rated * t_rated) * t_trip_time;

    float current_sq = torque * torque;
    float rated_sq = t_rated * t_rated;

    // 核心累加器：发热减去基础散热
    i2t_acc += (current_sq - rated_sq) * dt;

    // 冷却下限拦截
    if(i2t_acc < 0.0f) 
    {
        i2t_acc = 0.0f; // 完全冷却，积分清零
    }
    
    // 触发保护判断
    if(!(ODObjs.error_code & ERR_OVER_CURRENT_SOFT) && (i2t_acc > i2t_threshold))
    {
        set_err(ERR_OVER_CURRENT_SOFT); // 触发 I?t 热过载（过流堵转）保护
        motor_ctrl.state = SOFT_STOP;
    }

    /* -------- CAN 状态机检测 -------- */
    static uint16_t cnt = 0; 
    if(cnt < 1000)
    {
        cnt++;
    }
    else
    {
        // 判断can_phy连通性
        if(ODObjs.heartbeat_consumer_enable) 
        {
            canfd_timeout_cnt++;
            if(canfd_timeout_cnt > 250) 
            {
                canfd_frame_flag = 1;
                RunSignal = 0;
                motor_ctrl.state = STOPPED;
            }
            else
            {
                if((motor_ctrl.state == STOPPED) && canfd_frame_flag)
                {
                    //RunSignal = 1;
                    motor_ctrl.state = INIT;
                }
                canfd_frame_flag = 0;
            }
        }

        // 判断can_bus_off
        static uint32_t can_buf_off_cnt = 0;
        if(CanfdRegs.CFG_STAT.bit.BUSOFF) 
        {
            can_buf_off_cnt++;
            if(can_buf_off_cnt > 100) 
            {
                can_buf_off_cnt = 0;
                canfd_buf_off_flag = 1;
                RunSignal = 0;
                motor_ctrl.state = STOPPED;
            }
        }
        else
        {
            if((motor_ctrl.state == STOPPED) && canfd_buf_off_flag)
            {
                //RunSignal = 1;
                motor_ctrl.state = INIT;
            }
            can_buf_off_cnt = 0;
            canfd_buf_off_flag = 0;
        }

        // 判断欠压
        if((gUDC.uDCBigFilter * 0.1f) < under_v_level)
        {
            set_err(ERR_UNDER_VOLTAGE);
        }
        if((ODObjs.error_code & ERR_UNDER_VOLTAGE) && ((gUDC.uDCBigFilter * 0.1f) > under_v_level)) 
        {
            clr_err(ERR_UNDER_VOLTAGE);
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
