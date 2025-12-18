#include "iap.h"

#if defined(APP)

uint32_t addr_offset = 0;

/**
 * @brief 擦除download空间的三个扇区
 * @return 0:擦除成功
 */
uint16_t clean_download(void)
{
    uint16_t error_mask = 0;
    error_mask |= flash_erase_sector(SECTORB);
    error_mask |= flash_erase_sector(SECTORC);
    error_mask |= flash_erase_sector(SECTORD);
    return error_mask;
}

/**
 * @brief app跳转到bootloader
 */
void jump_to_bootloader(void)
{
    SET_IAP_FLAG(IAP_FLAG_NUM); // 设置更新标志位
    ResetDSP(); // 直接软重启dsp
}

/**
 * @brief 逐个8字节的接收新固件的bin文件，攒满一个扇区擦除写入
 * @param data 新固件的8字节小包的首地址
 */
void write_iap_data(uint16_t *data)
{
    if((addr_offset + 4) < DOWNLOAD_SIZE)
    {
        flash_program((uint16_t*)(DOWNLOAD_ADDR + addr_offset), data, 4);
        addr_offset+=4;
    }
}

#else

/**
 * @brief 内部使用指定扇区擦除的函数
 * @param sectorMask 指定的擦除的扇区掩码，在Flash_ADP32F03x_API_Library.h中有定义
 * @return 0:擦除成功 其他值：失败
 */
#pragma CODE_SECTION(flash_erase_sector, "ramfuncs");
uint16_t flash_erase_sector(uint16_t sectorMask)
{
    // 系统暂停
    DisableDog();
    DINT;
    DRTM;

    FLASH_ST FlashStatus;
    uint16_t status = Flash_Erase(sectorMask, &FlashStatus);

    // 系统恢复
    EINT;
    ERTM;
    KickDog();
    EnableDog();

    return status;
}

/**
 * @brief 内部使用指定地址写入数据的函数
 * @param flashAddr Flash写入的起始地址
 * @param dataBuf 要写入的数据缓冲区指针
 * @param length 写入长度（以16bit为单位）
 * @return 0:写入成功 其他值：失败
 */
#pragma CODE_SECTION(flash_program, "ramfuncs");
uint16_t flash_program(uint16_t *flashAddr, uint16_t *dataBuf, uint16_t length)
{
    //系统暂停
    DisableDog();
    DINT;
    DRTM;

    FLASH_ST FlashStatus;
    uint16_t Status = Flash_Program(flashAddr, dataBuf, length, &FlashStatus);

    // 系统恢复
    EINT;
    ERTM;
    KickDog();
    EnableDog();

    return Status;
}

uint16_t page_temp[8192] = {0};

void bootloader(void)
{ 
    if(GET_IAP_FLAG() == IAP_FLAG_NUM)
    {
        flash_erase_sector(SECTORF);
        flash_erase_sector(SECTORG);
        flash_erase_sector(SECTORH);

        memcpy(page_temp, (uint16_t*)0x3F0000, 8192);
        flash_program((uint16_t *)APP_ADDR, page_temp, 8192); 

        memcpy(page_temp, (uint16_t*)0x3F2000, 8192);
        flash_program((uint16_t *)(APP_ADDR + 0x2000), page_temp, 8192); 

        memcpy(page_temp, (uint16_t*)0x3F4000, 8192);
        flash_program((uint16_t *)(APP_ADDR + 0x4000), page_temp, 8192); 

        // 清空更新标志位
        SET_IAP_FLAG(0);
    }

    ((void (*)(void))0x3E8000)(); // 跳转app
}

#endif
