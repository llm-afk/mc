#ifndef CANFD_H
#define CANFD_H

#include "MainInclude.h"
#include "ringbuffer.h"
#include "iap.h"

#define CANFD_DLC_TO_LEN(dlc) ( \
    ((dlc) <  9) ? (dlc) : \
    ((dlc) == 9) ? 12 : \
    ((dlc) == 10) ? 16 : \
    ((dlc) == 11) ? 20 : \
    ((dlc) == 12) ? 24 : \
    ((dlc) == 13) ? 32 : \
    ((dlc) == 14) ? 48 : \
    /* dlc == 15 */ 64 \
)

#define CANFD_LEN_TO_DLC(len) \
(   ((len) <= 8)  ? (len) : \
    ((len) == 12) ? 9  : \
    ((len) == 16) ? 10 : \
    ((len) == 20) ? 11 : \
    ((len) == 24) ? 12 : \
    ((len) == 32) ? 13 : \
    ((len) == 48) ? 14 : \
    ((len) == 64) ? 15 : \
    0xFF /* 非法长度 */ )

#define GET_MSG_ID(canid)   (canid & 0x780) // 获取 11bit 中高 4bit 的msg_id
#define GET_NODE_ID(canid)  (canid & 0x07F) // 获取 11bit 中低 7bit 的node_id

#define MSG_ID_HEARTBEAT    0x700

#define MSG_ID_SDO_SRV      0x580
#define MSG_ID_SDO_CLI      0x600

#define MSG_ID_RPDO_5       0x100 
#define MSG_ID_TPDO_5       0x190  
#define MSG_ID_RPDO_6       0x400 

#define MSG_ID_DFU          0x680 

#define CS_R        0x40
#define CS_R_ACK_1  0x4F
#define CS_R_ACK_2  0x4B
#define CS_R_ACK_3  0x47
#define CS_R_ACK_4  0x43
#define CS_W_1      0x2F
#define CS_W_2      0x2B
#define CS_W_3      0x27
#define CS_W_4      0x23
#define CS_W_ACK    0x60
#define CS_ERR      0x80

typedef struct {
    uint16_t id;       // CAN ID：标准11位
    uint16_t len;      // 数据长度 查表后的实际的数据段字节数
    uint16_t data[32]; // 数据区：最多64字节，用32个 uint16_t 存储
} canFrame_t;

extern uint16_t canfd_frame_flag; // 用于指示当前是否有收到canfd帧数据 0-有 1-没有
extern volatile uint32_t canfd_timeout_cnt; // 100hz记录没有canfd帧数据的时候累加值
extern uint16_t canfd_buf_off_flag;

extern uint16_t m_node_id;
void canfd_init(void);
interrupt void canfd_IsrHander1(void);
void can_com_loop(void);

#endif
