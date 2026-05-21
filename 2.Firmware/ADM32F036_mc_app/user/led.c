#include "led.h"

const lpg_seg_t led_off[]  = {{100,0}};
const lpg_seg_t led_on[]   = {{100,1}};

// 电机失能状态下的led_id闪烁
const lpg_seg_t led_id_1_dis[] = {{12,1},{16,0},{120,0}};
const lpg_seg_t led_id_2_dis[] = {{12,1},{16,0},{12,1},{16,0},{120,0}};
const lpg_seg_t led_id_3_dis[] = {{12,1},{16,0},{12,1},{16,0},{12,1},{16,0},{120,0}};
const lpg_seg_t led_id_4_dis[] = {{12,1},{16,0},{12,1},{16,0},{12,1},{16,0},{12,1},{16,0},{120,0}};

// 电机使能状态下的led_id闪烁
const lpg_seg_t led_id_1_en[] = {{12,1},{16,0},{30,0},{4,1},{5,0},{4,1},{5,0},{120,0}};
const lpg_seg_t led_id_2_en[] = {{12,1},{16,0},{12,1},{16,0},{30,0},{4,1},{5,0},{4,1},{5,0},{120,0}};
const lpg_seg_t led_id_3_en[] = {{12,1},{16,0},{12,1},{16,0},{12,1},{16,0},{30,0},{4,1},{5,0},{4,1},{5,0},{120,0}};
const lpg_seg_t led_id_4_en[] = {{12,1},{16,0},{12,1},{16,0},{12,1},{16,0},{12,1},{16,0},{30,0},{4,1},{5,0},{4,1},{5,0},{120,0}};

// 错误状态指示
const lpg_seg_t led_err_can_phy_off[]      = {{4,1},{5,0},{4,1},{5,0},{120,0}};
const lpg_seg_t led_err_drv_over_temp[]    = {{4,1},{5,0},{4,1},{5,0},{30,0},{4,1},{5,0},{120,0}};
const lpg_seg_t led_err_motor_over_temp[]  = {{4,1},{5,0},{4,1},{5,0},{30,0},{4,1},{5,0},{4,1},{5,0},{120,0}};
const lpg_seg_t led_err_can_bus_off[]      = {{4,1},{5,0},{4,1},{5,0},{4,1},{5,0},{120,0}};
const lpg_seg_t led_err_under_voltage[]    = {{4,1},{5,0},{4,1},{5,0},{4,1},{5,0},{30,0},{4,1},{5,0},{120,0}};
const lpg_seg_t led_err_uncalibrated[]     = {{5,1},{5,0}};

typedef enum{
    LED_OFF,
    LED_ON,
    LED_ID_BLINK,
    LED_ERR_CAN_PHY_OFF,        // 物理断线导致通信中断                
    LED_ERR_DRV_OVER_TEMP,      // 驱动器过温                            
    LED_ERR_MOTOR_OVER_TEMP,    // 电机过温                          
    LED_ERR_UNDER_VOLTAGE,      // 欠压    
    LED_ERR_CAN_BUS_OFF,        // 关节自身CANFD外设Bus-Off,主动挂出总线   
    LED_ERR_UNCALIBRATED,       // 未校准
}led_state_t;

led_state_t led_state = LED_OFF;
led_state_t led_state_last = LED_OFF;

#pragma CODE_SECTION(led_on_1,"ramfuncs");
static inline void led_on_1(void)
{
    GpioDataRegs.GPBCLEAR.bit.GPIO34 = 1;
}

#pragma CODE_SECTION(led_off_1,"ramfuncs");
static inline void led_off_1(void)
{
    GpioDataRegs.GPBSET.bit.GPIO34 = 1;
}

void led_init(void)
{
    // gpio_init
    EALLOW;
    GpioCtrlRegs.GPBPUD.bit.GPIO34 = 0;  
    GpioCtrlRegs.GPBMUX1.bit.GPIO34 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO34 = 1;
    GpioDataRegs.GPBSET.bit.GPIO34 = 1; // 上电不亮
    EDIS;

    lpg_init(&lpg); // 初始化lpg底层序列生成器，驱动tick频率100hz
    lpg_register(&lpg, led_off, ARRAY_SIZE(led_off), led_on_1, led_off_1); // 注册该序列生成器下的一个Led模块
}

/**
 * @brief 控制灯状态循环
 * @note 100hz循环执行
 */
void led_loop(void)
{
    static uint16_t cnt = 0;
    static uint16_t RunSignal_last = 0;

    if(ODObjs.is_calibrated == 0)
    {
        led_state = LED_ERR_UNCALIBRATED;
        if(led_state != led_state_last)
        {
            lpg_set_pattern(&lpg.units[0], led_err_uncalibrated, ARRAY_SIZE(led_err_uncalibrated));
        }
    }
    else if(cnt <= 1000)
    {
        cnt++;
        led_state = LED_ID_BLINK;
        if((led_state != led_state_last) || (RunSignal != RunSignal_last))
        {
            switch(ODObjs.node_id)
            {
                case 1: RunSignal ? lpg_set_pattern(&lpg.units[0], led_id_1_en, ARRAY_SIZE(led_id_1_en)) : lpg_set_pattern(&lpg.units[0], led_id_1_dis, ARRAY_SIZE(led_id_1_dis)); break;
                case 2: RunSignal ? lpg_set_pattern(&lpg.units[0], led_id_2_en, ARRAY_SIZE(led_id_2_en)) : lpg_set_pattern(&lpg.units[0], led_id_2_dis, ARRAY_SIZE(led_id_2_dis)); break;
                case 3: RunSignal ? lpg_set_pattern(&lpg.units[0], led_id_3_en, ARRAY_SIZE(led_id_3_en)) : lpg_set_pattern(&lpg.units[0], led_id_3_dis, ARRAY_SIZE(led_id_3_dis)); break;
                case 4: RunSignal ? lpg_set_pattern(&lpg.units[0], led_id_4_en, ARRAY_SIZE(led_id_4_en)) : lpg_set_pattern(&lpg.units[0], led_id_4_dis, ARRAY_SIZE(led_id_4_dis)); break;
                default:break;;
            }
        }
    }
    else
    {
        // 错误优先级 断线 > 驱动器过温 > 电机过温 > 欠压 > can_bus_off
        if(canfd_frame_flag)
        {
            led_state = LED_ERR_CAN_PHY_OFF;
            if(led_state != led_state_last)
            {
                lpg_set_pattern(&lpg.units[0], led_err_can_phy_off, ARRAY_SIZE(led_err_can_phy_off));
            }
        }
        else if(ODObjs.error_code & ERR_OVER_TEMP_DRV)
        {
            led_state = LED_ERR_DRV_OVER_TEMP;
            if(led_state != led_state_last)
            {
                lpg_set_pattern(&lpg.units[0], led_err_drv_over_temp, ARRAY_SIZE(led_err_drv_over_temp));
            }
        }
        else if(ODObjs.error_code & ERR_OVER_TEMP_MOTOR)
        {
            led_state = LED_ERR_MOTOR_OVER_TEMP;
            if(led_state != led_state_last)
            {
                lpg_set_pattern(&lpg.units[0], led_err_motor_over_temp, ARRAY_SIZE(led_err_motor_over_temp));
            }
        }
        else if(ODObjs.error_code & ERR_UNDER_VOLTAGE)
        {
            led_state = LED_ERR_UNDER_VOLTAGE;
            if(led_state != led_state_last)
            {
                lpg_set_pattern(&lpg.units[0], led_err_under_voltage, ARRAY_SIZE(led_err_under_voltage));
            }
        }
        else if(canfd_buf_off_flag)
        {
            led_state = LED_ERR_CAN_BUS_OFF;
            if(led_state != led_state_last)
            {
                lpg_set_pattern(&lpg.units[0], led_err_can_bus_off, ARRAY_SIZE(led_err_can_bus_off));
            }
        }
        else
        {
            led_state = LED_ID_BLINK;
            if((led_state != led_state_last) || (RunSignal != RunSignal_last))
            {
                switch(ODObjs.node_id)
                {
                    case 1: RunSignal ? lpg_set_pattern(&lpg.units[0], led_id_1_en, ARRAY_SIZE(led_id_1_en)) : lpg_set_pattern(&lpg.units[0], led_id_1_dis, ARRAY_SIZE(led_id_1_dis)); break;
                    case 2: RunSignal ? lpg_set_pattern(&lpg.units[0], led_id_2_en, ARRAY_SIZE(led_id_2_en)) : lpg_set_pattern(&lpg.units[0], led_id_2_dis, ARRAY_SIZE(led_id_2_dis)); break;
                    case 3: RunSignal ? lpg_set_pattern(&lpg.units[0], led_id_3_en, ARRAY_SIZE(led_id_3_en)) : lpg_set_pattern(&lpg.units[0], led_id_3_dis, ARRAY_SIZE(led_id_3_dis)); break;
                    case 4: RunSignal ? lpg_set_pattern(&lpg.units[0], led_id_4_en, ARRAY_SIZE(led_id_4_en)) : lpg_set_pattern(&lpg.units[0], led_id_4_dis, ARRAY_SIZE(led_id_4_dis)); break;
                    default:break;;
                }
            }
        }
    }
    led_state_last = led_state;
    RunSignal_last = RunSignal;
    lpg_loop();
}

