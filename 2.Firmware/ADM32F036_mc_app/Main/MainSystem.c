#include "MainInclude.h"
#include "MotorInclude.h"
#include "Parameter.h"
#include "SPIcomm.h"
#include "AngleSensor.h"
#include "stimer.h"
#include "flash_eeprom.h"
#include "canfd.h"
#include "od.h"
#include "iap.h"
#include "encoder.h"
#include "motor_ctrl.h"
#include "led.h"

// xinZhi first commit 2026/4/21

stimer_t stimer_main;

#define DEBUG 0
#if(DEBUG == 1)
int16_t debug_buffer_1[2048] = {0};
int16_t debug_buffer_2[2048] = {0};
#endif

void main(void)
{
    for(volatile uint32_t i = 0; i < 100000; i++) asm(" NOP");

    InitSysCtrl();
    InitInterrupt();                 
    InitPeripherals();             
    InitForMotorApp();        
    InitForFunctionApp();
    
    OD_init(); // 先初始化OD对象,初始化参数默认值
    
    eeprom_init();
    load_eeprom_to_ram(); // 初始化参数

    led_init();
    encoder_init(); // 根据eeprom参数配置encoder
    canfd_init(); // 根据eeprom参数初始化canfd

    stimer_init(&stimer_main);
    stimer_addTask(&stimer_main, 0, 1, 0, KickDog);
    stimer_addTask(&stimer_main, 1, 20,0, led_loop);
    stimer_addTask(&stimer_main, 2, 1, 0, MC_servo_loop);
    stimer_addTask(&stimer_main, 3, 20,1, info_collect_loop);
    stimer_addTask(&stimer_main, 4, 1, 0, SystemLeve05msMotor);
    stimer_addTask(&stimer_main, 5, 1, 0, SystemLeve05msFunction);
    stimer_addTask(&stimer_main, 6, 4, 0, SystemLeve2msMotor);
    stimer_addTask(&stimer_main, 7, 4, 2, SystemLeve2msFunction);

    EnableDog();
    SetInterruptEnable();      
    Zero_Len = 120;
    EINT; //使能全局中断
    ERTM; //使能实时模式
    while(1)
    {
        stimer_loop(&stimer_main);
        can_com_loop();
    }
}

/***************************************************************
    取代EPWM的ADC 中断，使用下溢中断
    1：清除中断标志，可以使下一个中断进来
    2：中断嵌套，设置可以被其他中断打断（比如SCI 等执行时间非常短的中断）
    3：执行FOC 算法
    4：关中断
    5：Acknowledge this interrupt

    2&4 是可以选择项目，供设计师参考！！！
****************************************************************/
#pragma CODE_SECTION(ZeroOfEPWMISR, "ramfuncs");
interrupt void ZeroOfEPWMISR(void)
{
    // 电流环计算
    EPwm2Regs.ETCLR.bit.INT = 1;
    EINT;
    encoder_loop();
    ADCOverInterrupt();
    DINT;
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP3; // Acknowledge this interrupt

    // 向stimer模块提供心跳
    static Uint16 stimer_main_cnt = 0;
    if(++stimer_main_cnt >= 10)
    {
        stimer_main_cnt = 0;
        stimer_heartBeat(&stimer_main); // 2Khz的心跳频率
    }

    #if(DEBUG == 1)
    static uint16_t cnt = 0;
    //debug_buffer_1[cnt] = gIMT.T;
    //debug_buffer_2[cnt] = 4 + (abs_error >> 9) ;

    cnt++;
    cnt%=2000;
    #endif
}

/***************************************************************
    EPWM的过流中断
         对硬件过流信号处理：

    注意：
    逻辑是：先封锁（硬件已经完成，大概200NS），然后进该中断（执行报错，清除标志位等），
    也可以屏蔽此中断，在外面采用查询的方式，不影响保护
****************************************************************/
#pragma CODE_SECTION(OneShotTZOfEPWMISR, "ramfuncs");
interrupt void OneShotTZOfEPWMISR(void)
{
    HardWareErrorDeal();                    // 处理硬件故障－电机控制模块处理
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP2; // Acknowledge this interrupt
}
