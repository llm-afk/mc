/*
 * AngleSensor.h
 *
 *  Created on: 2025年6月25日
 *      Author: advancechip
 */

#ifndef HEARDFILES_SOURCEHEADER_ANGLESENSOR_H_
#define HEARDFILES_SOURCEHEADER_ANGLESENSOR_H_


typedef struct
{
    long    AdcSinN;
    long    AdcSinP;
    long    AdcCosN;
    long    AdcCosP;
    long    MrSin_Calib;//基准值
    long    MrCos_Calib;//基准值
    long    MrSin;//传感器实时信号
    long    MrCos;//传感器实时信号
    Uint    InitElecDegree;//校准电角度
    Uint    ElecDegree;//反馈电角度
    Uint    MachDegree;//反馈机械角度
    Uint    MotorPairs;
    Uint    MotorPowerOnFlag;

}DEGREE_STRUCT;

typedef struct
{
    long  PLL_P;
    long  PLL_I;
    Uint  PLL_Theta;
    long  PLL_OMG;
    long  PLL_OMG_filter;
    long  PLL_freq;
    long  PLL_error;
    long  PLL_cos;
    long  PLL_sin;
    long  PLL_intgerator;
}PLL_STRUCT;
extern PLL_STRUCT    PLL_Sensors;
extern DEGREE_STRUCT Degree;

void AngleCal(void);

#endif /* HEARDFILES_SOURCEHEADER_ANGLESENSOR_H_ */
