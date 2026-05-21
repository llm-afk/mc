#ifndef MOTOR_CTRL_H
#define MOTOR_CTRL_H

#include "MainInclude.h"
#include "encoder.h"
#include "MotorInclude.h"
#include "od.h"

typedef enum{
    INIT,
    MIT,
    ENCODER_CALIBRATE,
    ENCODER_ZERO,
    SOFT_STOP,
    STOPPED,
}motor_mode_t;

typedef enum {
    ERR_OVER_VOLTAGE            = 0x0001,
    ERR_UNDER_VOLTAGE           = 0x0002,
    ERR_FOLLOWING_ERROR         = 0x0004,
    ERR_OVER_TEMP_DRV           = 0x0008,
    ERR_OVER_TEMP_MOTOR         = 0x0010,
    ERR_OVER_CURRENT_SOFT       = 0x0020,
    ERR_OVER_LOAD               = 0x0040,
    ERR_HEARTBEAT_TIMEOUT       = 0x0080,
    
    ERR_ENC_CALIB               = 0x4000,
    ERR_ADC_SELFTEST            = 0x8000,
} tErrorCode;

#define CW_CMD_OPERATION_ENABLE             0x01
#define CW_CMD_OPERATION_DISABLE            0x02
#define CW_CMD_RESET_HOME                   0x03
#define CW_CMD_ERROR_RESET                  0xFF
#define CW_CMD_DEV_ENCODER_CALIB            0xF1

typedef struct {
    motor_mode_t state;   // 控制模式状态

    int32_t degree_ref_q14;   // 输出端目标角度 rad
    int32_t velocity_ref_q14; // 输出端目标速度 rad/s
    int32_t torque_ref_q14;   // mit前馈力矩 nm
    uint32_t Kp_q14;          // mit_kp 0.01
    uint32_t Kd_q14;          // mit_kd 0.01

    int32_t board_temp_q14;   // 驱动器温度
    int32_t motor_temp_q14;   // 电机温度
}motor_ctrl_t;

extern motor_ctrl_t motor_ctrl;
extern float board_temp;
extern float motor_temp;

void MC_servo_loop(void);
void info_collect_loop(void);

int MC_controlword_update(void);

#endif
