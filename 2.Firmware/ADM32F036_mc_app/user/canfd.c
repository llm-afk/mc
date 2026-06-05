#include "canfd.h"
#include "od.h"
#include "motor_ctrl.h"
#include "MotorInclude.h"
#include "encoder.h"
#include "utils.h"

ringbuffer_t canFrameRxRingbuffer = {0}; // 接收ringbuffer控制块
uint16_t canFrameRxBuffer[512]    = {0}; // 接收ringbuffer缓冲区
ringbuffer_t canFrameTxRingbuffer = {0}; // 发送ringbuffer控制块
uint16_t canFrameTxBuffer[512]    = {0}; // 发送ringbuffer缓冲区 

uint16_t m_node_id = 0;

/**
 * @brief 初始化canfd协议需要的环形缓冲区
 */
void canfd_ringbuffer_init(void)
{
    ringbuffer_init(&canFrameRxRingbuffer, canFrameRxBuffer, 512);
    ringbuffer_init(&canFrameTxRingbuffer, canFrameTxBuffer, 512);
}

/**
 * @brief 配置接收滤波器 (只匹配11位标准ID的低7位)
 * @param id 需要匹配的ID的低7位数值 (0x00 ~ 0x7F)
 */
static inline void canfd_config_filter_low7_dual(uint16_t id)
{
    CanfdRegs.ACF_EN.bit.AE_0 = 1; // 使能第0个滤波器
    CanfdRegs.CIA_ACF_CFG.bit.SELMASK = 0;
    CanfdRegs.CIA_ACF_CFG.bit.ACFADR = 0; // 使用第0个滤波器
    CanfdRegs.ACF_0.all = (id & 0x007F);
    CanfdRegs.ACF_1.all = 0;
    CanfdRegs.CIA_ACF_CFG.bit.SELMASK = 1;
    CanfdRegs.CIA_ACF_CFG.bit.ACFADR = 0;
    CanfdRegs.ACF_0.all = 0xFF80;
    CanfdRegs.ACF_1.all = 0;

    // CanfdRegs.ACF_EN.bit.AE_1 = 1; // 使能第1个滤波器
    // CanfdRegs.CIA_ACF_CFG.bit.SELMASK = 0;
    // CanfdRegs.CIA_ACF_CFG.bit.ACFADR = 1; // 使用第0个滤波器
    // CanfdRegs.ACF_0.all = (0 & 0x007F);
    // CanfdRegs.ACF_1.all = 0;
    // CanfdRegs.CIA_ACF_CFG.bit.SELMASK = 1;
    // CanfdRegs.CIA_ACF_CFG.bit.ACFADR = 1;
    // CanfdRegs.ACF_0.all = 0xFF80;
    // CanfdRegs.ACF_1.all = 0;
}

/**
 * @brief 配置canfd的波特率
 * @param lowSpeed 低速的仲裁段的波特率
 * @param highSpeed 高速的数据段的波特率
 */
static inline void canfd_config_baudRate(uint16_t lowSpeed, uint16_t highSpeed)
{
    switch(lowSpeed)
    {
        case 1:
        {
            // Arbitration Phase：1M @ 75%
            CanfdRegs.S_CFG.bit.S_PRESC = 4; // canfd_clk = 100M / (PRESC + 1)
            CanfdRegs.S_SEG.bit.S_Seg_1 = 13; // canfd_bandrate = canfd_clk / (F_Seg_1 + 2 + F_Seg_2 + 1)
            CanfdRegs.S_SEG.bit.S_Seg_2 = 4;  
            CanfdRegs.S_CFG.bit.S_SJW   = 4; // sjw < Seg_2
        }
        default :
        {
            break;
        }
    }
    switch(highSpeed)
    {
        case 4:
        {
            // Data Phase：4M @ 60%
            CanfdRegs.F_CFG.bit.F_PRESC = 0; // canfd_clk = 100M / (PRESC + 1)
            CanfdRegs.F_SEG.bit.F_Seg_1 = 14; // canfd_bandrate = canfd_clk / (F_Seg_1 + 2 + F_Seg_2 + 1)
            CanfdRegs.F_SEG.bit.F_Seg_2 = 8;    
            CanfdRegs.F_CFG.bit.F_SJW   = 6; // sjw < Seg_2     
        }
        default :
        {
            break;
        }
    }
}

/**
 * @brief canfd配置
 */
void canfd_init(void)
{
    m_node_id = ODObjs.node_id;

    // gpio
    EALLOW;
    #if 0
    GpioCtrlRegs.GPAPUD.bit.GPIO28 = 1;      // CANFD_Rx
    GpioCtrlRegs.GPAPUD.bit.GPIO29 = 1;     // CANFD_Tx
    AnalogRegs.PIN_MUX_FLASH.bit.canfd_gpio_mux = 0xA;   //GPIO0 CANFDRXD ; GPIO32 CANFDTXD
    #else
    GpioCtrlRegs.GPAPUD.bit.GPIO0 = 1;      // CANFD_Rx
    GpioCtrlRegs.GPBPUD.bit.GPIO32 = 1;     // CANFD_Tx
    AnalogRegs.PIN_MUX_FLASH.bit.canfd_gpio_mux = 0x5;   //GPIO0 CANFDRXD ; GPIO32 CANFDTXD
    #endif
    EDIS;

    CanfdRegs.CFG_STAT.bit.RESET = 1; 

    // clk
    CanfdRegs.CANCFG.bit.CAN_CLK_SEL = 0;   // 0:sysclk 1:pllclk

    // band rate
    canfd_config_baudRate(1, 4);

    // filter
    canfd_config_filter_low7_dual(m_node_id);

    // TDC
    CanfdRegs.DELAY_EALCAP.bit.TDCEN = 1;
    CanfdRegs.DELAY_EALCAP.bit.SSPOFF = 8;

    CanfdRegs.CFG_STAT.bit.RESET = 0;
    
    // interrupt
    CanfdRegs.RTINTFE.all = 0;
    CanfdRegs.RTINTFE.bit.RIE = 1; // 只开启接收缓冲区非空中断

    // config
    CanfdRegs.TCTRL.bit.TSMODE = 0;   // 0 = FIFO mode
    CanfdRegs.CFG_STAT.bit.TBSEL = 1; // 配置为二级缓冲区
    CanfdRegs.CFG_STAT.bit.TPE = 0;

    CanfdRegs.CFG_STAT.bit.TPSS = 1;
    CanfdRegs.CFG_STAT.bit.TSSS = 1;

    canfd_ringbuffer_init();   
}

/**
 * @brief 发送一帧数据
 */
#pragma CODE_SECTION(sendCanFrame_fifo, "ramfuncs");
static inline void sendCanFrame_fifo(canFrame_t *canFrame)
{
    CanfdRegs.TBUF.RID0.bit.ID0 = canFrame->id; 
    CanfdRegs.TBUF.RIDST.bit.DLC = CANFD_LEN_TO_DLC(canFrame->len); 
    memcpy(CanfdRegs.TBUF.DATA, canFrame->data, ((canFrame->len + 1) >> 1));

    CanfdRegs.TBUF.RIDST.bit.IDE = 0;        // 0: standard frame ID; 1: extended frame ID
    CanfdRegs.TBUF.RIDST.bit.RTR = 0;        // 0: data frame; 1: remote frame(for can)
    CanfdRegs.TBUF.RIDST.bit.FDF_EDL = 1;    // 0: can 2.0; 1:can_fd
    CanfdRegs.TBUF.RIDST.bit.BRS = 1;        // 1：开启发送canfd加速模式

    CanfdRegs.TCTRL.bit.TSNEXT = 1;
    CanfdRegs.CFG_STAT.bit.TPE = 0;
    CanfdRegs.CFG_STAT.bit.TSONE = 0;
    CanfdRegs.CFG_STAT.bit.TSALL = 1;
}

/**
* @brief 将待发送的帧压入发送缓冲区
* @param frame 待发送的CAN帧指针
*/
#pragma CODE_SECTION(enqueue_tx_frame, "ramfuncs");
static inline void enqueue_tx_frame(canFrame_t *frame)
{
    if(ringbuffer_avail(&canFrameTxRingbuffer) >= sizeof(canFrame_t)) // 发送缓冲区还有帧空间
    {
        ringbuffer_in(&canFrameTxRingbuffer, frame, sizeof(canFrame_t)); // 将帧压入发送帧环形缓冲区
    }
}

/**
 * @brief canfd总中断
 */
#pragma CODE_SECTION(canfd_IsrHander1, "ramfuncs");
interrupt void canfd_IsrHander1(void)
{
    if(CanfdRegs.RTINTFE.bit.RIF)
    {
        CanfdRegs.RTINTFE.bit.RIF = 1;
        while(CanfdRegs.TCTRL.bit.RSTAT != 0)
        {
            if(ringbuffer_avail(&canFrameRxRingbuffer) >= sizeof(canFrame_t)) 
            {
                canFrame_t canFrame_temp = {0};
                canFrame_temp.id = CanfdRegs.RBUF.RID0.bit.ID0; 
                canFrame_temp.len = CANFD_DLC_TO_LEN(CanfdRegs.RBUF.RIDST.bit.DLC);
                memcpy(canFrame_temp.data, CanfdRegs.RBUF.DATA, ((canFrame_temp.len + 1) >> 1));
                ringbuffer_in(&canFrameRxRingbuffer, &canFrame_temp, sizeof(canFrame_t));

                canfd_timeout_cnt = 0; // 收到帧数据，清除canfd通信断开的计数
            }
            CanfdRegs.TCTRL.bit.RREL = 1;
        }
    }
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP4;
}

float torque = 0;

/**
 * @brief CAN 数据帧解析函数
 * @param frame 待解析的 CAN 数据帧
 */
#pragma CODE_SECTION(parse_frame, "ramfuncs");
static void parse_frame(canFrame_t *frame)
{
    uint16_t msg_id = GET_MSG_ID(frame->id);
    uint16_t node_id = GET_NODE_ID(frame->id);
    switch(msg_id)
    {
        case MSG_ID_SDO_CLI:
        {
            if(frame->len == 8)
            {
                uint16_t cs = __byte(frame->data, 0);
                uint16_t idx = (__byte(frame->data, 2) << 8) | __byte(frame->data, 1); 
                uint16_t *data = &frame->data[2]; 

                __byte(frame->data, 0) = CS_ERR; // 初始化命令字节为错误

                if(cs == CS_R)
                {
                    __byte(frame->data, 0) = OD_read(idx, data);
                }
                else if(cs == CS_W_1)
                {
                    __byte(frame->data, 0) = OD_write_1(idx, data);
                }
                else if(cs == CS_W_2)
                {
                    __byte(frame->data, 0) = OD_write_2(idx, data);
                }
                else if(cs == CS_W_4)
                {
                    __byte(frame->data, 0) = OD_write_4(idx, data);
                }

                frame->id = MSG_ID_SDO_SRV + m_node_id;
                enqueue_tx_frame(frame);
            }
            break;   
        }
        case MSG_ID_RPDO_5: // 上位机数据交互
        {
            // 数据解析
            motor_ctrl.degree_ref_q14   = (int32_t)(*(float*)&frame->data[0] * 16384.0f); 
            motor_ctrl.velocity_ref_q14 = (int32_t)(*(float*)&frame->data[2] * 16384.0f); 
            motor_ctrl.torque_ref_q14   = (int32_t)(*(float*)&frame->data[4] * 16384.0f); 
            motor_ctrl.Kp_q14           = ((uint32_t)(*(uint16_t*)&frame->data[6])) * 16400; 
            motor_ctrl.Kd_q14           = ((uint32_t)(*(uint16_t*)&frame->data[7])) * 16400;

            // 数据上报
            frame->id = MSG_ID_TPDO_5 + m_node_id;    
            frame->len = 16; 
            if(ODObjs.error_code) 
            {
                frame->len = 20;     
                *(uint16_t*)&frame->data[8] = ODObjs.error_code;
            }
            torque = Iq_To_Torque(imt_current_to_float(gIMT.M, gIMT.T, MOTOR_RATED_CUR));
            *(float*)&frame->data[0] = (float)encoder.degree_q14 / 16384.0f; // 减速端位置反馈(rad)
            *(float*)&frame->data[2] = (float)encoder.velocity_q14 / 16384.0f; // 减速端速度反馈(rad/s)
            *(float*)&frame->data[4] = torque; // 性能优化版本，减少50us计算时间
            frame->data[6] = (int16_t)(motor_temp * 10.0f); // 电机温度 (0.1°)
            frame->data[7] = (int16_t)(board_temp * 10.0f); // 驱动器温度 (0.1°) 
            enqueue_tx_frame(frame);
            break;   
        }
        case MSG_ID_RPDO_6: // 运控数据交互
        {
            if(RunSignal == 0 && ODObjs.error_code == 0)
            {
                RunSignal = 1;
            }

            // 数据解析
            motor_ctrl.degree_ref_q14   = (int32_t)(*(float*)&frame->data[0] * 16384.0f); 
            motor_ctrl.velocity_ref_q14 = (int32_t)(*(float*)&frame->data[2] * 16384.0f); 
            motor_ctrl.torque_ref_q14   = (int32_t)(*(float*)&frame->data[4] * 16384.0f); 
            motor_ctrl.Kp_q14           = ((uint32_t)(*(uint16_t*)&frame->data[6])) * 16400; 
            motor_ctrl.Kd_q14           = ((uint32_t)(*(uint16_t*)&frame->data[7])) * 16400;

            // 数据上报
            frame->id = MSG_ID_TPDO_5 + m_node_id;    
            frame->len = 16; 
            if(ODObjs.error_code) 
            {
                frame->len = 20;     
                *(uint16_t*)&frame->data[8] = ODObjs.error_code;
            }
            *(float*)&frame->data[0] = (float)encoder.degree_q14 / 16384.0f; // 减速端位置反馈(rad)
            *(float*)&frame->data[2] = (float)encoder.velocity_q14 / 16384.0f; // 减速端速度反馈(rad/s)
            *(float*)&frame->data[4] = Iq_To_Torque(imt_current_to_float(gIMT.M, gIMT.T, MOTOR_RATED_CUR)); // 性能优化版本，减少50us计算时间
            frame->data[6] = (int16_t)(motor_temp * 10.0f); // 电机温度 (0.1°)
            frame->data[7] = (int16_t)(board_temp * 10.0f); // 驱动器温度 (0.1°) 
            enqueue_tx_frame(frame);
            break;   
        }
        case MSG_ID_DFU:
        {
            if(frame->len == 4)
            {
                if(*(uint32_t*)&frame->data[0] == 0xDDDDDDDD) // start
                {
                    if(!clean_download())
                    {
                        *(uint32_t*)&frame->data[0] = 0xDDDDDDDD; // ack
                    }
                    else
                    {
                        *(uint32_t*)&frame->data[0] = 0x00000000; // nack
                    }
                    addr_offset = 0;
                }
                else if(*(uint32_t*)&frame->data[0] == 0xFFFFFFFF)
                {
                    *(uint32_t*)&frame->data[0] = 0xFFFFFFFF; // ack
                    sendCanFrame_fifo(frame); 
                    ADP32F03x_usDelay(1000);
                    jump_to_bootloader();
                }
            }
            else
            {
                write_iap_data(frame->data);
                frame->len = 0;
            }
            enqueue_tx_frame(frame);
            break;
        }
        default:
        {
            break;       
        }
    }
}

uint16_t canfd_frame_flag = 0; // 用于指示当前是否有收到canfd帧数据 0-有 1-没有
volatile uint32_t canfd_timeout_cnt = 0; // 100hz记录没有canfd帧数据的时候累加值
uint16_t canfd_buf_off_flag = 0; // can_bus_off标志位 0-正常 1-关闭
uint16_t heatbeat_flag = 0; // 心跳标志位 0-未发送 1-已发送

/**
 * @brief can协议通信loop
 */
#pragma CODE_SECTION(can_com_loop, "ramfuncs");
void can_com_loop(void)
{
    canFrame_t canFrame_temp = {0};

    // RX loop
    if(ringbuffer_used(&canFrameRxRingbuffer) >= sizeof(canFrame_t))
    {
        ringbuffer_out(&canFrameRxRingbuffer, &canFrame_temp, sizeof(canFrame_t));
        parse_frame(&canFrame_temp);
    }

    // TX loop
    if(ringbuffer_used(&canFrameTxRingbuffer) >= sizeof(canFrame_t))
    {
        ringbuffer_out(&canFrameTxRingbuffer, &canFrame_temp, sizeof(canFrame_t));
        sendCanFrame_fifo(&canFrame_temp);
    }
}


