#include "encoder.h"
#include "flash_eeprom.h"

encoder_config_t encoder_config = {
    .elec_degree_calib = 1675,
    .linearity_table = {0},
    .encoder_reverse = 0,
    .phase_reverse = 0,
};

encoder_t encoder = {0};

/**
 * @brief 初始化
 */
void encoder_init(void)
{
    // 获取eeprom中标志位偏移
    encoder.in_enc_deg_zero_conf = ODObjs.in_encoder_offset; 
    encoder.ex_enc_deg_zero_conf = ODObjs.ex_encoder_offset;
    encoder.enc_error_conf = encoder.in_enc_deg_zero_conf - encoder.ex_enc_deg_zero_conf;
    if(encoder.enc_error_conf > ENCODER_CPR_DIV) 
    {
        encoder.enc_error_conf -= ENCODER_CPR; 
    }
    else if(encoder.enc_error_conf < -ENCODER_CPR_DIV) 
    {
        encoder.enc_error_conf += ENCODER_CPR; 
    }

    // 读取角度
    uint16_t enc_temp = get_pri_enc_val(); 
    uint16_t ex_enc_temp = get_sec_enc_val();

    // 翻转处理
    if(!encoder_config.encoder_reverse)
    {
        enc_temp = 16383 - enc_temp;
        ex_enc_temp = 16383 - ex_enc_temp;
    }

    // 上电时刻偏移量
    encoder.in_enc_deg_zero = enc_temp;
    encoder.ex_enc_deg_zero = ex_enc_temp;
    encoder.enc_error = encoder.in_enc_deg_zero - encoder.ex_enc_deg_zero;
    if(encoder.enc_error > ENCODER_CPR_DIV) 
    {
        encoder.enc_error -= ENCODER_CPR; 
    }
    else if(encoder.enc_error < -ENCODER_CPR_DIV) 
    {
        encoder.enc_error += ENCODER_CPR; 
    }

    // 计算上电时刻双编码器误差
    encoder.error = encoder.enc_error + encoder.enc_error_conf;
    if(encoder.error > ENCODER_CPR_DIV) 
    {
        encoder.error -= ENCODER_CPR; 
    }
    else if(encoder.error < -ENCODER_CPR_DIV) 
    {
        encoder.error += ENCODER_CPR; 
    }
}

/**
 * @brief 编码器循环
 * @note 在电角度的 20kHz 中断中执行
 */
#pragma CODE_SECTION(encoder_loop,"ramfuncs");
void encoder_loop(void)
{
    uint16_t pri_enc_val = get_pri_enc_val();
    uint16_t sec_enc_val = get_sec_enc_val();
    uint16_t is_reverse = encoder_config.encoder_reverse;

    uint16_t raw_deg_rev;
    uint16_t ex_raw;

    encoder.enc_degree_raw = pri_enc_val;

    if(is_reverse)
    {
        raw_deg_rev = 16383 - pri_enc_val;
        ex_raw      = 16383 - sec_enc_val;
    }
    else
    {
        raw_deg_rev = pri_enc_val;
        ex_raw      = sec_enc_val;
    }

    encoder.enc_degree_raw_reversed = raw_deg_rev;
    encoder.ex_enc_degree_raw       = ex_raw;
    ODObjs.in_encoder_offset        = raw_deg_rev;
    ODObjs.ex_encoder_offset        = ex_raw;

    // 线性化补偿
    uint16_t lined_deg;
    {
        uint16_t idx1 = raw_deg_rev >> 5; 
        uint16_t idx2 = (idx1 + 1) & 0x1FF; 
        uint16_t fraction = raw_deg_rev & 0x1F; 

        int16_t err1 = encoder_config.linearity_table[idx1];
        int16_t err2 = encoder_config.linearity_table[idx2];

        int32_t interp_error = err1 + ( ((int32_t)(err2 - err1) * fraction) >> 5 );
        lined_deg = (raw_deg_rev - interp_error) & (ENCODER_CPR - 1);
        encoder.enc_degree_lined = lined_deg;
    }

    // 维护多圈累积值
    static uint16_t degree_last = 0;
    static uint16_t flag = 0; 
    
    if(!flag) 
    {
        flag = 1;
        degree_last = lined_deg; 
    }
    
    int16_t delta = (int16_t)(lined_deg - degree_last);

    // --- 【核心修复1】：拦截并过滤物理不可能的角度跳变（SPI 毛刺） ---
    // 如果跳变超过 100（约等效 100r/s 的物理极限），且没有发生真实的过零跳变，
    // 则认为这是一次 SPI 通信错误，强制将 lined_deg 修正为连续值。
    if(delta > 100 && delta < (ENCODER_CPR - 100)) 
    {
        delta = 100;
        lined_deg = (degree_last + delta) & (ENCODER_CPR - 1);
    }
    else if(delta < -100 && delta > -(ENCODER_CPR - 100)) 
    {
        delta = -100;
        lined_deg = (degree_last + delta) & (ENCODER_CPR - 1);
    }

    // --- 【核心修复2】：在滤除毛刺后，安全地处理过零点和圈数累计 ---
    // enc_turns 现在受到毛刺过滤器的保护，不会因为 SPI 错误而莫名其妙多/少一圈
    if(delta > ENCODER_CPR_DIV) 
    {
        encoder.enc_turns--;
        delta -= ENCODER_CPR; 
    }
    else if(delta < -ENCODER_CPR_DIV) 
    {
        encoder.enc_turns++;
        delta += ENCODER_CPR; 
    }

    // 更新受保护的数据：确保电角度计算和下一拍位置参考是平滑的
    encoder.enc_degree_lined = lined_deg;
    degree_last = lined_deg;

    // 速度计算
    {
        #define VEL_WINDOW_BITS      3
        #define VEL_WINDOW_SIZE      (1 << VEL_WINDOW_BITS)
        #define VEL_ALPHA_MIN_Q8     1    
        #define VEL_ALPHA_MAX_Q8     128  
        #define VEL_NOISE_DEADZONE   16000  

        static int16_t  delta_history[VEL_WINDOW_SIZE] = {0};
        static uint16_t history_idx = 0;
        static int32_t  delta_sum = 0;

        static int32_t  velocity_temp_q14 = 0;
        static int32_t  vel_alpha_q8 = VEL_ALPHA_MIN_Q8;
        static int32_t  alpha_target_smooth_q8 = VEL_ALPHA_MIN_Q8; 
        static uint16_t vel_filter_init = 0;

        delta_sum += delta - delta_history[history_idx];
        delta_history[history_idx] = delta;
        history_idx = (history_idx + 1) & (VEL_WINDOW_SIZE - 1);

        int32_t vel_target_q14 = (delta_sum * 125664) >> VEL_WINDOW_BITS;

        if(!vel_filter_init) {
            vel_filter_init = 1;
            velocity_temp_q14 = vel_target_q14;
            alpha_target_smooth_q8 = VEL_ALPHA_MIN_Q8;
        }

        int32_t error = vel_target_q14 - velocity_temp_q14;
        int32_t abs_error = (error >= 0) ? error : -error;
        int32_t active_error = abs_error - VEL_NOISE_DEADZONE;
        active_error = (active_error > 0) ? active_error : 0; 

        int32_t alpha_instant = VEL_ALPHA_MIN_Q8 + (active_error >> 9);
        alpha_instant = (alpha_instant < VEL_ALPHA_MAX_Q8) ? alpha_instant : VEL_ALPHA_MAX_Q8; 

        alpha_target_smooth_q8 += (alpha_instant - alpha_target_smooth_q8) >> 4;

        int32_t alpha_diff = alpha_target_smooth_q8 - vel_alpha_q8;
        if(alpha_diff > 0) {
            vel_alpha_q8 += alpha_diff >> 1; 
        } else {
            vel_alpha_q8 += alpha_diff >> 3; 
        }

        // --- 【核心修复3】：使用 int64_t 强制转换，防止 (error * vel_alpha_q8) 结果溢出 32 位范围 ---
        // 这是解决静止时由于 SPI 毛刺误触发 160 rad/s 恒定异常速度的数学根本原因
        velocity_temp_q14 += (int32_t)(((int64_t)error * vel_alpha_q8) >> 8);
        encoder.enc_velocity_q14 = -velocity_temp_q14;
    }
    
    // 电角度计算 (16-bit)
    if(motor_ctrl.state == MIT)
    {
        uint16_t mech_ang = (lined_deg - encoder_config.elec_degree_calib) & (ENCODER_CPR - 1);
        uint16_t e_deg_temp = (uint16_t)(mech_ang * (MOTOR_POLE_PAIRS * 4));

        if(encoder_config.phase_reverse)
        {
            encoder.elec_degree = e_deg_temp;
        }
        else
        {
            encoder.elec_degree = 65535 - e_deg_temp;   
        }
    }
}

int enc_set_zero(void)
{
    load_ram_item_to_eeprom_from_key(3); 
    ResetDSP(); 
    return 0;
} 

int16_t temp_cw[512] = {0};
int16_t temp_ccw[512] = {0};

/**
 * @brief 2khz编码器校准程序
 * @return 0 校准中 1 校准完成
 */
uint16_t encoder_calibrate(void)
{
    static uint16_t cnt = 0;
    static uint16_t state = 0;
    static uint16_t enc_degree_raw_start = 0;
    static int16_t  mech_dir = 1; 

    #define CALIB_CURRENT 1000 

    switch(state)
    {
        case 0: // 锁定偏移
        {
            Iq = 0;
            Id = cnt; 
            encoder.elec_degree = 0;
            if(cnt >= CALIB_CURRENT)
            {
                enc_degree_raw_start = encoder.enc_degree_raw;
                cnt = 0;
                state = 1;
            }
            break;
        }
        case 1: // 探测相序和编码器方向
        {
            Iq = 0; Id = CALIB_CURRENT;
            encoder.elec_degree += 256;
            if(cnt >= 512) 
            {
                int16_t degree_dif = (int16_t)(encoder.enc_degree_raw - enc_degree_raw_start);
                if(degree_dif > ENCODER_CPR_DIV) degree_dif -= ENCODER_CPR;
                else if(degree_dif < -ENCODER_CPR_DIV) degree_dif += ENCODER_CPR;

                // 目标：Iq > 0 总是产生物理正向转动
                // 如果增加电角度导致原始机械角度减小 (假设 CW 对应原始减小)
                if(degree_dif < 0) 
                {
                    encoder_config.encoder_reverse = 1; // 设为反向，使 16383 - raw 变为增加
                    encoder_config.phase_reverse = 0;   // 正常相序
                }
                else 
                {
                    // 增加电角度导致原始增加 -> 物理反向 (CCW)
                    // 说明相线接反了
                    encoder_config.encoder_reverse = 0; // 数值增加
                    encoder_config.phase_reverse = 1;   // 相序反向
                }
                mech_dir = 1;
                
                cnt = 0;
                state = 2;
            } 
            break;
        }
        case 2: // 重新锁定 0 位
        {
            Iq = 0; Id = CALIB_CURRENT;
            encoder.elec_degree = 0;
            if(cnt >= 1000)
            {
                cnt = 0;
                state = 3;
                encoder_config.elec_degree_calib = encoder.enc_degree_raw_reversed;
            }
            break;
        }
        case 3: // 正向扫表
        {
            Iq = 0; Id = CALIB_CURRENT;
            static int16_t first_raw_val = 0;
            if(cnt == 1) first_raw_val = encoder.enc_degree_raw_reversed;
            
            if(cnt <= 4096)
            {
                if((cnt - 1) % 8 == 0)
                {
                    uint16_t idx = (cnt - 1) >> 3; 
                    int32_t ideal_pos = first_raw_val + (32 * idx * mech_dir);
                    ideal_pos = ideal_pos & (ENCODER_CPR - 1); 
                    
                    int16_t error = (int16_t)encoder.enc_degree_raw_reversed - (int16_t)ideal_pos;
                    if(error > ENCODER_CPR_DIV) error -= ENCODER_CPR;
                    else if(error < -ENCODER_CPR_DIV) error += ENCODER_CPR;
                    
                    uint16_t start_bin = first_raw_val >> 5; 
                    uint16_t abs_bin = (start_bin + idx * mech_dir) & 0x1FF; 
                    temp_cw[abs_bin] = error; 
                }
            }
            else
            {
                cnt = 0;
                state = 4;
            }
            encoder.elec_degree += (MOTOR_POLE_PAIRS * 16); 
            break;
        }
        case 4: 
        {
            Iq = 0; Id = CALIB_CURRENT;
            encoder.elec_degree = 0; 
            if(cnt >= 1000)
            {
                cnt = 0;
                state = 5;
            }
            break;
        }
        case 5: // 反向扫表
        {
            Iq = 0; Id = CALIB_CURRENT;
            static int16_t first_raw_val = 0;
            if(cnt == 1) first_raw_val = encoder.enc_degree_raw_reversed;
            
            if(cnt <= 4096)
            {
                if((cnt - 1) % 8 == 0)
                {
                    uint16_t idx = (cnt - 1) >> 3; 
                    int32_t ideal_pos = first_raw_val - (32 * idx * mech_dir);
                    ideal_pos = ideal_pos & (ENCODER_CPR - 1);
                    
                    int16_t error = (int16_t)encoder.enc_degree_raw_reversed - (int16_t)ideal_pos;
                    if(error > ENCODER_CPR_DIV) error -= ENCODER_CPR;
                    else if(error < -ENCODER_CPR_DIV) error += ENCODER_CPR;
                    
                    uint16_t start_bin = first_raw_val >> 5;
                    uint16_t abs_bin = (start_bin - idx * mech_dir) & 0x1FF;
                    temp_ccw[abs_bin] = error;
                }
            }
            else
            {
                cnt = 0;
                state = 6;
            }
            encoder.elec_degree -= (MOTOR_POLE_PAIRS * 16);
            break;
        }
        case 6: // 计算线性化表
        {
            Iq = 0; Id = 0; 
            int32_t sum_error = 0;
            static int16_t smooth_buf[512]; 

            for(uint16_t i = 0; i < 512; i++)
            {
                int16_t avg_error = (temp_cw[i] + temp_ccw[i]) / 2;
                encoder_config.linearity_table[i] = avg_error;
                sum_error += avg_error;
            }
            
            int16_t dc_offset = sum_error / 512;
            for(uint16_t i = 0; i < 512; i++)
            {
                encoder_config.linearity_table[i] -= dc_offset;
            }

            for(uint16_t i = 0; i < 512; i++)
            {
                uint16_t prev = (i == 0) ? 511 : i - 1;
                uint16_t next = (i == 511) ? 0 : i + 1;
                smooth_buf[i] = (encoder_config.linearity_table[prev] + 
                                 encoder_config.linearity_table[i] + 
                                 encoder_config.linearity_table[next]) / 3;
            }

            for(uint16_t i = 0; i < 512; i++)
            {
                encoder_config.linearity_table[i] = smooth_buf[i];
            }

            state = 0;
            cnt = 0;
            ODObjs.is_calibrated = 1;
            load_ram_item_to_eeprom_from_key(1); 
            load_ram_item_to_eeprom_from_key(9);
            return 1; 
        }
        default: break;
    }
    cnt++;
    return 0; 
}
