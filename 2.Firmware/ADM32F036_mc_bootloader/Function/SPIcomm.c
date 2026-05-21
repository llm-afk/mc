#include "SPIcomm.h"
#include "Main.h"
#include "MotorInclude.h"
#include "Record.h"
#include "AngleSensor.h"

#define CS_MAIN_EN()      (GpioDataRegs.GPACLEAR.bit.GPIO19 = 1)
#define CS_MAIN_DIS()     (GpioDataRegs.GPASET.bit.GPIO19 = 1)

#define CS_EX_EN()      (GpioDataRegs.GPACLEAR.bit.GPIO29 = 1)
#define CS_EX_DIS()     (GpioDataRegs.GPASET.bit.GPIO29 = 1)

void InitSpiIO(void)
{
    EALLOW;

    GpioCtrlRegs.GPAPUD.bit.GPIO16 = 0;   // Enable pull-up on GPIO16 (SPISIMOA)
    GpioCtrlRegs.GPAPUD.bit.GPIO17 = 0;   // Enable pull-up on GPIO17 (SPISOMIA)
    GpioCtrlRegs.GPAPUD.bit.GPIO18 = 0;   // Enable pull-up on GPIO18 (SPICLKA)
    GpioCtrlRegs.GPAPUD.bit.GPIO19 = 0;   // Enable pull-up on GPIO19 (SPISTEA)
    GpioCtrlRegs.GPAPUD.bit.GPIO29 = 0;   // ex_cs


    GpioCtrlRegs.GPAQSEL2.bit.GPIO16 = 3; // Asynch input GPIO16 (SPISIMOA)
    GpioCtrlRegs.GPAQSEL2.bit.GPIO17 = 3; // Asynch input GPIO17 (SPISOMIA)
    GpioCtrlRegs.GPAQSEL2.bit.GPIO18 = 3; // Asynch input GPIO18 (SPICLKA)
    GpioCtrlRegs.GPAQSEL2.bit.GPIO19 = 3; // Asynch input GPIO19 (SPISTEA)
    GpioCtrlRegs.GPAQSEL2.bit.GPIO29 = 3;

    GpioCtrlRegs.GPAMUX2.bit.GPIO16 = 1; // Configure GPIO16 as SPISIMOA
    GpioCtrlRegs.GPAMUX2.bit.GPIO17 = 1; // Configure GPIO17 as SPISOMIA
    GpioCtrlRegs.GPAMUX2.bit.GPIO18 = 1; // Configure GPIO18 as SPICLKA
    GpioCtrlRegs.GPAMUX2.bit.GPIO19 = 0; // Configure GPIO19 as SPISTEA
    GpioCtrlRegs.GPAMUX2.bit.GPIO29 = 0;

    GpioCtrlRegs.GPADIR.bit.GPIO19 = 1; // ca_main设置为输出
    GpioCtrlRegs.GPADIR.bit.GPIO29 = 1; // ca_main设置为输出

    CS_MAIN_DIS();
    CS_EX_DIS();

    EDIS;
}

void InitSpi(void)
{
    InitSpiIO();

    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.SPIAENCLK = 1;   // SPI-A
    SpiaRegs.SPICCR.bit.SPISWRESET = 0;//SPI 软件复位位。在改变 SPI 模块的配置之前应先将该位清零；在恢复 SPI操作之前应将该位置 1
    SpiaRegs.SPICCR.bit.SPICHAR = 0xf;//16位数据长度
    SpiaRegs.SPICCR.bit.SPILBK = 0;//回环控制禁止
    SpiaRegs.SPICCR.bit.CLKPOLARITY = 1;//数据在上升沿输出，在下降沿输入。当没有SPI 数据发送时， SPICLK 为低电 平。数据的输入和输出边沿视
    SpiaRegs.SPICTL.bit.SPIINTENA =0;//禁止中断
    //SpiaRegs.SPICTL.bit.SPIINTENA = 1;//使能中断
    SpiaRegs.SPICTL.bit.TALK = 1;//允许4 脚发送，保证使能接收器的SPISTE 输入管脚。
    SpiaRegs.SPICTL.bit.MASTER_SLAVE = 1;//SPI 被配置为主控制器
    SpiaRegs.SPICTL.bit.CLK_PHASE = 0;//标准的SPICLK 信号，其极性由CLOCK POLARITY 位决定
    SpiaRegs.SPICTL.bit.OVERRUNINTENA =0;//禁止RECEIVER OVERRUN Flag 位产生的中断
    SpiaRegs.SPIBRR = 6; // 目前是 100/7 = 14.29Mhz 的spi时钟频率
    SpiaRegs.SPICCR.bit.SPISWRESET =1;
    EDIS;
}

#pragma CODE_SECTION(SPITransfer_main,"ramfuncs");
Uint16 SPITransfer_main(Uint16 data)
{
    CS_MAIN_EN();
    while(SpiaRegs.SPISTS.bit.BUFFULL_FLAG == 1);
    SpiaRegs.SPITXBUF = data;
    while(SpiaRegs.SPISTS.bit.INT_FLAG == 0);
    CS_MAIN_DIS();
    return SpiaRegs.SPIRXBUF;
}

#pragma CODE_SECTION(SPITransfer_ex,"ramfuncs");
Uint16 SPITransfer_ex(Uint16 data)
{
    CS_EX_EN();
    while(SpiaRegs.SPISTS.bit.BUFFULL_FLAG == 1);
    SpiaRegs.SPITXBUF = data;
    while(SpiaRegs.SPISTS.bit.INT_FLAG == 0);
    CS_EX_DIS();
    return SpiaRegs.SPIRXBUF;
}


/**
 * @brief 获取主编码器的原始值
 */
#pragma CODE_SECTION(get_main_degree_raw,"ramfuncs");
uint16_t get_main_degree_raw(void)
{
    uint16_t h_data = SPITransfer_main(0x8300); // 获取主编码器03寄存器的高8位值
    uint16_t l_data = SPITransfer_main(0x8400); // 获取主编码器04寄存器的低6位值
    return ((((h_data & 0x00FF) << 8) | (l_data & 0x00FF)) >> 2);
}

/**
 * @brief 获取副编码器的原始角度
 */
#pragma CODE_SECTION(get_ex_degree_raw,"ramfuncs");
uint16_t get_ex_degree_raw(void)
{
    uint16_t h_data = SPITransfer_ex(0x8300); // 获取主编码器03寄存器的高8位值
    uint16_t l_data = SPITransfer_ex(0x8400); // 获取主编码器04寄存器的低6位值
    return (16384 - ((((h_data & 0x00FF) << 8) | (l_data & 0x00FF)) >> 2));
}


