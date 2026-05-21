
/************************************************************************
 Project:             Automotive water pumps
 Filename:            LDF.c
 Partner Filename:    N/A
 Description:         Partner file of LDF.h
 Complier:            Code Composer Studio for ADM32F036, CCS Systems.
 CPU TYPE :           ADM32F036A2
*************************************************************************
 Copyright (c) 2024 **** Co., Ltd.
 All rights reserved.
*************************************************************************
*************************************************************************
 Revising History (ECL of this file):

************************************************************************/

/************************************************************************
 Beginning of File, do not put anything above here except notes
 Compiler Directives
*************************************************************************/

/*
需要关注的参数:
MAX_DC_VOL_3_3V: 母线电压采集的最大量程，根据实际的硬件电路决定3V和16倍
MAX_PEAK_CUR_3_3V: 电流采集的最大量程，由运放增益和采样电阻阻值共同决定 ±20A就填2000 
OVER_DC_VOL_POINT: 过压保护阈值 切断输出 目前的逻辑需要重启以恢复
OVER_DC_VOL_LIMIT: 过压控制限制点（非立即保护）降低电流
UNDER_DC_VOL_POINT: 欠压保护阈值 切断输出 目前的逻辑需要重启以恢复
MOTOR_RATED_VOL: 额定电压 母线电压/sqrt2
MOTOR_RATED_CUR: 电机的额定电流 iq值与这个有关系 iq为4096即MOTOR_RATED_CUR大小电流（为了实际的量程可测量，必须确保电机的峰值电流小于MAX_PEAK_CUR_3_3V）
TORQUE_CUR_LIMIT: 设定电机的峰值电流为额定电流的多少倍 200是2倍，然后这个会限制软件层面最大的扭矩输出
D_CUR_KI
D_CUR_KP
Q_CUR_KI
Q_CUR_KD
*/

/*
场景分析：
1.我现在的硬件是5m欧的采样电阻，16倍的运放增益。考虑到adc的满量程是3v以及运放的1.5v的偏置，所以实际最大可采集的电流是±18.75A
所以我应该设定MAX_PEAK_CUR_3_3V的值是1875，这是最大可测量电流的一个设定。假设我的电机是10A的额定电流，15A的峰值电流，我就需要设定
MOTOR_RATED_CUR = 1000，TORQUE_CUR_LIMIT = 150 来在软件层面限制电流环的输出。同时我们应该确保电机的峰值电流应该在实际的可测量量程内
同时注意一个问题，就是我们接触到的所有的电流相关的参数都是和MOTOR_RATED_CUR有关系的，实际的iq值0~4096表示电流的0~MOTOR_RATED_CUR大小,可以超过

2.后续我们由于调整了硬件，采样电阻变成了2m欧，所以实际的可采集电流范围到了±46.875A，所以这时候MAX_PEAK_CUR_3_3V的值应设置为4687
过流保护的阈值还是MAX_PEAK_CUR_3_3V的0.95倍，但是其实只要我设定我的电机的峰值电流比这个还要小，就不会触发过流保护
但是电机的参数没有变，由于硬件电流支持测量这么大的电流了，所以可以解放电机的性能了，设置MOTOR_RATED_CUR为2000，TORQUE_CUR_LIMIT为150表示峰值电流30A

*/

#include "Define.h"
#include "Main.h"

///////////////////////////////////////////////////////////////////////////////////////
// 控制板参数设置
///////////////////////////////////////////////////////////////////////////////////////
unsigned int MAX_DC_VOL_3_3V        =  480;        //! 0.1vdc 母线电压采集的最大量程 根据实际的硬件电路决定3V和16倍
#define  MAX_PEAK_CUR_3_3V             4687        //! 0.01A // 电流采集的最大量程，由运放增益和采样电阻阻值共同决定 ±20A就填2000
unsigned long MAX_PEAK_CUR_3_3V_2   =  MAX_PEAK_CUR_3_3V;
unsigned long MAX_PEAK_CUR_3_3V_410 =  ((long)MAX_PEAK_CUR_3_3V << 12) / 10;

unsigned int OVER_DC_VOL_POINT    =  360;         //! 0.1vdc // 母线电压过压保护阈值 切断输出
unsigned int OVER_DC_VOL_LIMIT    =  350;         //! 0.1vdc // 母线电压过压控制限制点（非立即保护）降低电流
unsigned int UNDER_DC_VOL_POINT   =  170;         //! 0.1vdc // 母线电压欠压保护阈值 

unsigned int  OVER_DC_VOL_KP      =  800;
unsigned int MaxAllowUdcLimit     =  170;
unsigned int MidAllowUdcLimit     =  155;

#define DEAD_TIME                    20          // 设置死区时间，实际的上下管的死区时间是 DEAD_TIME+20 单位0.01us
#define DEAD_COMPENATION_TIME        0           // 死区补偿时间，0.25US
unsigned int DeadTimeInternal     =  ((long)DEAD_TIME * DSP_CLOCK) / 100;               // 死区时间
unsigned int DeadTimeCompInternal =  ((long)DEAD_COMPENATION_TIME * DSP_CLOCK) / 100;   // 死区补偿时间

#define HARDWARE_OVER_CUR            (MAX_PEAK_CUR_3_3V *0.99f) //! 硬件比较器的过流阈值，根据硬件设计决定
#define HARDWARE_CBC_CUR             7000                                // CBC过流比较值, 8.00A 逐波限流实际没有用到
unsigned int OverCurPointInternal =  ((long)HARDWARE_OVER_CUR << 9) / MAX_PEAK_CUR_3_3V + 512;   // 硬件过流比较值
unsigned int CBCCurPointInternal  =  ((long)HARDWARE_CBC_CUR  << 9) / MAX_PEAK_CUR_3_3V + 512;   // CBC过流比较值

///////////////////////////////////////////////////////////////////////////////////////
// 电机参数设置
///////////////////////////////////////////////////////////////////////////////////////
unsigned int MOTOR_RATED_POWER  =  2000;    //0.1W为单位
unsigned int MOTOR_RATED_VOL    =  17;  //! 相电压的有效值，输入实际的母线电压/1.414
unsigned int MOTOR_RATED_CUR    =  300; //! 20A的额定电流，这里是额定
unsigned int LOWER_LIMIT_FREQ   =  0;

unsigned int PMSM_LD            =  270;//250;//30;//150;//432;          // 37uH
unsigned int PMSM_LQ            =  326;//260;//33;//170;//464;          // 50uH
unsigned int PMSM_RS            =  269;//150;//25;//50;//251;          // 11mohm
unsigned int PMS_L_R_UNIT       =  0;    
#ifdef KE_WIDE_RANGE
unsigned int PMSM_KE            =  80;//22;//40;//60;          // Ke = 43
#else
unsigned int PMSM_KE            =  4;//6;           // ke = 4
#endif

///////////////////////////////////////////////////////////////////////////////////////
// 开环切换闭环参数设置
///////////////////////////////////////////////////////////////////////////////////////
unsigned int FRQ_OPEN_TO_CLOSE      =  3000;    // 要计算一下单位，不是0.01HZ
unsigned int OPEN_LOOP_TOR_START    =  1000;
unsigned int FRQ_PWM_SHIFT_EN       =  10000;   // 要计算一下单位，不是0.01HZ

unsigned int LowLimit               =  550;     // 开环启动低速 调制比系数
unsigned int HighLimit              =  1300;    // 开环启动高速 调制比系数

#define SpinControlHZ                  2000              // 顺风启动阈值点, 0.01HZ
long SpinControlInternal   =  (long)SpinControlHZ << 4;  // 顺风启动阈值点, 内部单位。OMG,must be long as pm_est_lpf_omg1 is long
unsigned int BemfOffset             =  80;

///////////////////////////////////////////////////////////////////////////////////////
// 频率和加减速参数设置
///////////////////////////////////////////////////////////////////////////////////////
         int RefSet                 =  17000;  // 120.00HZ
unsigned int ACC1                   =  200;    // 低速段加速时间 5.0S
unsigned int ACC2                   =  200;    // 高速段加速时间 2.0S
unsigned int DCBrakeTime            =  20;     // 直流制动时间   1.0S

///////////////////////////////////////////////////////////////////////////////////////
// 速度环参数设置
///////////////////////////////////////////////////////////////////////////////////////
unsigned int F_CHANGING_POINT_1_PI  =  2000;
unsigned int F_CHANGING_POINT_2_PI  =  3000;
unsigned int LOW_SPEED_KI           =  30;
unsigned int LOW_SPEED_KP           =  30;
unsigned int HIGH_SPEED_KI_GAIN     =  10;
unsigned int HIGH_SPEED_KP_GAIN     =  10;

///////////////////////////////////////////////////////////////////////////////////////
// 电流环环参数设置
///////////////////////////////////////////////////////////////////////////////////////
unsigned int D_CUR_KI               =  20; // !
unsigned int D_CUR_KP               =  200;  // !
unsigned int Q_CUR_KI               =  20; // !
unsigned int Q_CUR_KP               =  200;  // !

///////////////////////////////////////////////////////////////////////////////////////
// 控制参数设置
///////////////////////////////////////////////////////////////////////////////////////
unsigned int PWM_FREQUENCY          =  200;     // 100 * 0.1kHz 载波频率
unsigned int AVR_MODE               =  1;       // AVR. 0,1: Enable, 2:Disabled

//--------------------- 频率范围参数----------------------/
unsigned int MAX_LIMIT_FREQ         =  20000;   // 300.00Hz
unsigned int MOTOR_RATED_FRE        =  16000;
unsigned int COAST_STOP_FRQ         =  2000;    // 1.0HZ,自由停机起始频率

//--------------------- 直流制动参数----------------------/
unsigned int DC_CONTROL_KP          = 3;
unsigned int DC_CONTROL_KI          = 5;
unsigned int DC_BRAKE_CUR           = 1500;

//--------------------- 电流限制百分比参数-------------/
unsigned int TORQUE_CUR_LIMIT       = 150; //! 是用来设置软件层面限制电机输出扭矩的，实际的硬件过流点是通过HARDWARE_OVER_CUR这个设置
                                           
//--------------------- 弱磁参数----------------------/
unsigned int FILED_WEEKING_MODE     = 2;       // FE-00, 0 ~ 2
unsigned int FILED_WEEKING_CUR_SCAL = 80;      // FE-01, 0 ~ 200
unsigned int FILED_WEEKING_SCAL     = 4;       // FE-02, 1 ~ 10
unsigned int FIELD_WEAK_REF         = 3950;    // FE-05, 0 ~ 4100
unsigned int IPD_PERIOD_SCALING     = 128;     // 避免更换电机后电感特别小导致运行电流很大
unsigned int IdMaxScaling           = 512;     // 弱磁电流占最大电流百分比，512 就是50%，Q10 格式
unsigned int IqMaxSet               = 400;     // NOT USED

unsigned int SOFT_OC_POINT          = 10000;   // Current limit : 3A,150% // 软件限流，实际没有使用
                                               // 9000 * 5 / 4096 = 11A
unsigned int STALL_PR_TIME          = 8;       // 8s, 8000ms

unsigned int SF_LOW_SPEED            = 160;
unsigned int MIN_CUR_LOW_SPEED       = 5;

unsigned int OVER_SPEED_SCALING      = 130;


// #define  THETA_COM_0_1_DEGREE         20   // tmp_swDeltaThetaPu, 182 ~ 1.0Degree...
// 1000Hz, if Coff = 1000, tmp_swDeltaThetaPu = 620, that is 3.4Degree......
// For AC System, 250HZ Max,
// ThetaCoff    Degree
// 4000         3.4
// 10000        8.5
// 15000        12.7
unsigned int ThetaCoff              =  5000;
unsigned int AUTO_RESET_CNT         =  5;

//--------------------- FOC 磁链控制参数（一般不用修改）----------------------/
unsigned int SPEED_FEEDBACK_FILTER   = 56;
unsigned int FILED_WEEKING_OUTVOL_SC = 0;
unsigned int SPECIAL_FACTOR_1        = 0;
unsigned int SPEED_MEASURE_PAR_1     = 20;
unsigned int SPEED_MEASURE_PAR_2     = 20;
unsigned int START_PRESET_CUR        = 0;
unsigned int LOW_LIMIT_FREQUENCY     = 0;
unsigned int INIT_POS_DECTION_MODE   = 1;
unsigned int INIT_POS_DECTION_CUR    = 120;
unsigned int INIT_POS_DECTION_TIME   = 60;
unsigned int LOW_SPEED_FILTER        = 16;
unsigned int PMSM_RS_MIN             = 16;

//--------------------- 单电阻采样  参数----------------------/
unsigned int T_DELAY                = 150; // 1.5us
unsigned int T_SAMPLE               = 150; // 1.5us
//#define  CURR_SAMPLE_DELAY         (T_SAMPLE - T_DELAY)
//#define  MIN_SAMPLE_TIME           (T_SAMPLE + T_DELAY)
//#define  TWO_MIN_SAMPLE_TIME       (MIN_SAMPLE_TIME*2)

unsigned int FirstSamplePointShift  = 100; // 1.0us
unsigned int SecondSamplePointShift = 100; // 1.0us

unsigned int RatioForLowNoisePWM            = 2200;
unsigned int RatiotThresholdForLowNoisePWM  = 250;
