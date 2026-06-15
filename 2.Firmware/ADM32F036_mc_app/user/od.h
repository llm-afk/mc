#ifndef OD_H
#define OD_H

#include "MainInclude.h"
#include "canfd.h"
#include "motor_ctrl.h"

extern uint16_t g_need_reboot;

#define ATTR_R      0x01
#define ATTR_W      0x02
#define ATTR_RW     0x03
#define ATTR_ROM    0x04
#define ATTR_RAM    0x08

typedef struct {
    uint16_t error_code;                  // tErrorCode
    uint16_t control_word;                // MC_controlword_update

    uint32_t sn_s0;                       // 存放sn码的高4个字符
    uint32_t sn_s1;
    uint32_t sn_s2; 
    uint32_t sn_s3; 
    uint32_t sn_s4;                      
    uint32_t sn_s5;                   
    uint32_t sn_s6;              

    uint16_t node_id;         

    uint16_t heartbeat_Producer_enable; // 是否开启驱动器的心跳上报功能，开启后驱动器会每秒发送一次心跳帧
    uint16_t heartbeat_consumer_enable; // 是否开启驱动器的心跳监测功能，开启后驱动器会监测是否有心跳帧数据，如果超过一定时间没有收到心跳帧数据则认为通信断开了，会切断输出，需要断电重启才能恢复

    float    torque_limit;
    float    over_temp_drv_level;
    float    over_temp_motor_level;
    
    uint16_t in_encoder_offset;
    uint16_t ex_encoder_offset;
    uint16_t is_calibrated;
    uint16_t firmware_version;
}ODObjs_t;

extern ODObjs_t ODObjs;

void OD_init(void);
uint16_t OD_read(uint16_t idx, uint16_t *data);
uint16_t OD_write_1(uint16_t idx, uint16_t *data);
uint16_t OD_write_2(uint16_t idx, uint16_t *data);
uint16_t OD_write_4(uint16_t idx, uint16_t *data);
void OD_check_sn(void);

#endif
