#include "SPIcomm.h"
#include "Main.h"
#include "MotorInclude.h"
#include "Record.h"
#include "AngleSensor.h"
#include "encoder.h"

#pragma CODE_SECTION(spi_transfer,"ramfuncs");
static inline uint16_t spi_transfer(uint16_t data)
{
    while(SpiaRegs.SPISTS.bit.BUFFULL_FLAG == 1);
    SpiaRegs.SPITXBUF = data;
    while(SpiaRegs.SPISTS.bit.INT_FLAG == 0);
    return SpiaRegs.SPIRXBUF;
}

void InitSpi(void)
{
    EALLOW;
    GpioCtrlRegs.GPAPUD.bit.GPIO16 = 0;   // Enable pull-up on GPIO16 (SPISIMOA)
    GpioCtrlRegs.GPAPUD.bit.GPIO17 = 0;   // Enable pull-up on GPIO17 (SPISOMIA)
    GpioCtrlRegs.GPAPUD.bit.GPIO18 = 0;   // Enable pull-up on GPIO18 (SPICLKA)
    
    GpioCtrlRegs.GPAQSEL2.bit.GPIO16 = 3; // Asynch input GPIO16 (SPISIMOA)
    GpioCtrlRegs.GPAQSEL2.bit.GPIO17 = 3; // Asynch input GPIO17 (SPISOMIA)
    GpioCtrlRegs.GPAQSEL2.bit.GPIO18 = 3; // Asynch input GPIO18 (SPICLKA)

    GpioCtrlRegs.GPAMUX2.bit.GPIO16 = 1;  // Configure GPIO16 as SPISIMOA
    GpioCtrlRegs.GPAMUX2.bit.GPIO17 = 1;  // Configure GPIO17 as SPISOMIA
    GpioCtrlRegs.GPAMUX2.bit.GPIO18 = 1;  // Configure GPIO18 as SPICLKA

    GpioCtrlRegs.GPAPUD.bit.GPIO19  = 0; // pri_cs
    GpioCtrlRegs.GPAMUX2.bit.GPIO19 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO19  = 1;  
    
    GpioCtrlRegs.GPAPUD.bit.GPIO1   = 0; // sec_cs
    GpioCtrlRegs.GPAMUX1.bit.GPIO1  = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO1   = 1;  

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
    SpiaRegs.SPIBRR = 9; // 目前是 100/6 = 16.7Mhz 的spi时钟频率
    SpiaRegs.SPICCR.bit.SPISWRESET =1;
    EDIS;

    PRI_CS_H();
    SEC_CS_H();

    PRI_CS_L(); ma900_write_reg_crc(0x0F, 0x00); PRI_CS_H(); // 清除CRC_EN 
    PRI_CS_L(); ma900_write_reg_crc(0x11, 0xC9); PRI_CS_H(); // 设置数据滤波器
    
    SEC_CS_L(); ma900_write_reg_crc(0x0F, 0x00); SEC_CS_H(); // 清除CRC_EN 
    SEC_CS_L(); ma900_write_reg_crc(0x11, 0xC9); SEC_CS_H(); // 设置数据滤波器
}

static const uint16_t MA900_CRC4_TABLE[16] = {0, 13, 7, 10, 14, 3, 9, 4, 1, 12, 6, 11, 15, 2, 8, 5};

/**
 * @brief 计算 MA900 SPI 协议的 CRC4 并打包数据
 * @param data_12bit 原始数据 (仅低12位有效，例如 0x0ABC)
 * @return 组包后的16位数据 (格式: [Data 12-bit] | [CRC 4-bit])
 */
uint16_t ma900_spi_crc4_calc(uint16_t data_12bit)
{
    uint16_t crc;
    
    // 1. 确保输入数据只有低12位有效，清空DSP高位噪声
    data_12bit &= 0x0FFFu;

    // 2. 设置初始值 (Seed)，MA900 规定为 10
    crc = 10; 

    /* 
     * 3. 分三步计算 CRC (每次处理 4-bit Nibble)
     * 逻辑源自官方文档 Page 41: crc4 = (data >> shift & 0xF) ^ table[crc4];
     * 注意：这里使用的是查表法，查表索引是当前的 CRC 值
     */

    // 处理高 4 位 (Bits 11-8)
    // 这里的 & 0x000Fu 非常重要，确保取出的是0-15的值
    crc = ((data_12bit >> 8) & 0x000Fu) ^ MA900_CRC4_TABLE[crc];

    // 处理中 4 位 (Bits 7-4)
    crc = ((data_12bit >> 4) & 0x000Fu) ^ MA900_CRC4_TABLE[crc];

    // 处理低 4 位 (Bits 3-0)
    crc = ((data_12bit >> 0) & 0x000Fu) ^ MA900_CRC4_TABLE[crc];

    // 4. 最终 CRC 结果取低 4 位 (防御性编程，防止溢出)
    crc &= 0x000Fu;

    // 5. 返回拼接结果: 数据左移4位 + CRC
    return (data_12bit << 4) | crc;
}

/**
 * @brief 读取 MA900 寄存器
 * @param reg_addr 寄存器地址 (0-127)
 * @return 寄存器内的 8-bit 值 (如果通讯失败或校验错，返回 0xFFFF 表示错误)
 */
#pragma CODE_SECTION(ma900_read_reg,"ramfuncs");
void ma900_read_reg_crc(uint16_t reg_addr_start, uint16_t *buf, uint16_t size)
{
    uint16_t tx_frame;
    uint16_t rx_frame;

    tx_frame = ma900_spi_crc4_calc(0x0300 | (reg_addr_start & 0xFF));
    
    spi_transfer(tx_frame); 

    for(uint16_t i=0; i<size; i++)
    {
        rx_frame = spi_transfer(0x0000);
        buf[i] = (rx_frame >> 4) & 0x00FF; // 解析到的这个地址下的8bit数据
    }
}

/**
 * @brief 向 MA900 单个寄存器写入数据
 * @param reg_addr 寄存器地址 (0-127)
 * @param data_val 要写入的 8-bit 数据
 * @note  必须在片选拉低(PRI_CS_L)的状态下调用，函数执行完后需在外部拉高片选
 */
void ma900_write_reg_crc(uint16_t reg_addr, uint16_t data_val)
{
    spi_transfer(ma900_spi_crc4_calc(0x0C00 | (reg_addr & 0xFF)));
    spi_transfer(ma900_spi_crc4_calc(data_val & 0xFF));
    spi_transfer(ma900_spi_crc4_calc(0x0F00));
}

#pragma CODE_SECTION(get_pri_enc_val,"ramfuncs");
uint16_t get_pri_enc_val(void)
{
    uint16_t frame[2] = {0};
    
    PRI_CS_L(); 
    frame[0] = spi_transfer(0x3000); 
    frame[1] = spi_transfer(0x0000); 
    PRI_CS_H();

    return 16384 - ((uint16_t)((frame[0] & 0xFFF0) | (frame[1] >> 12)) >> 2);
}

#pragma CODE_SECTION(get_sec_enc_val,"ramfuncs");
uint16_t get_sec_enc_val(void)
{
    uint16_t frame[2] = {0};
    
    SEC_CS_L(); 
    frame[0] = spi_transfer(0x3000); 
    frame[1] = spi_transfer(0x0000); 
    SEC_CS_H();

    return (uint16_t)((frame[0] & 0xFFF0) | (frame[1] >> 12)) >> 2;
}
