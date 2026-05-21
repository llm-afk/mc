#ifndef ENCODER_H
#define ENCODER_H

#include "MainInclude.h"
#include "SPIcomm.h"
#include "motor_ctrl.h"
#include "utils.h"

#define ENCODER_BITS            (14)
#define ENCODER_CPR             (1 << ENCODER_BITS)
#define ENCODER_CPR_DIV         (ENCODER_CPR >> 1)

#define GEAR_RATIO              (12) 
#define GEAR_RATIO_INV          (1.0f / GEAR_RATIO)

#define CALIB_TABLE_SIZE        (512) 

#define MOTOR_POLE_PAIRS        (10)

#define MAIN_ENCODER_TEETH      (17)   // 主齿
#define EX_ENCODER_TEETH        (18)   // 副齿

typedef struct{
    uint16_t elec_degree_calib; // 角度校准值
    int16_t linearity_table[CALIB_TABLE_SIZE];  // 线化
    int16_t encoder_reverse; // 编码器反向
    int16_t phase_reverse;   // 电机相序反向
}encoder_config_t;

typedef struct{
    volatile uint16_t enc_degree_raw;      // 原始编码器值
    volatile uint16_t enc_degree_raw_reversed; // 反转后的原始编码器值
    volatile uint16_t enc_degree_lined;    // 线性化后的编码器值
    volatile int32_t enc_turns;            // 编码器端累加圈数
    volatile int32_t enc_velocity_q14;     // 编码器端速度 (rad/s)

    volatile uint16_t ex_enc_degree_raw;   // 副编码器原始值
    volatile uint16_t ex_enc_degree_lined; // 副编码器线性校准值
    
    volatile int64_t degree_q14;           // 减速端角度 (rad)
    volatile int32_t velocity_q14;         // 减速端速度 (rad/s)

    volatile uint16_t elec_degree;         // 电角度

    volatile uint16_t in_enc_deg_zero_conf;// 暂存eeprom中存储的标零时刻主编码器的值
    volatile uint16_t ex_enc_deg_zero_conf;// 暂存eeprom中存储的标零时刻副编码器的值
    volatile int16_t  enc_error_conf;      // 暂存eeprom中存储的标零时刻两个编码器的差值
    volatile uint16_t in_enc_deg_zero;     // 上电瞬间主编码器的值
    volatile uint16_t ex_enc_deg_zero;     // 上电瞬间副编码器的值
    volatile int16_t  enc_error;           // 上电瞬间两个编码器的差值
    volatile int16_t  error;               // 标零时刻双编差值和上电瞬间双差值这两个差值的差值，可以用来描述上电后主编码器的磁铁到底往那个方向旋转了多少圈
}encoder_t;

extern encoder_config_t encoder_config;
extern encoder_t encoder;
void encoder_init(void);
void encoder_loop(void);
int enc_set_zero(void);
uint16_t encoder_calibrate(void);

#endif
