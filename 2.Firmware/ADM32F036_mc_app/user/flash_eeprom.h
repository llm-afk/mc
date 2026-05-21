#ifndef FLASH_EEPROM_H
#define FLASH_EEPROM_H

#include "MainInclude.h"
#include "Flash_ADP32F03x_API_Library.h"
#include "Flash_ADP32F03x_API_Config.h"
#include "mm.h"
#include <string.h>

/* note:
eeprom空间实际的存储结构：
根据key顺序往下存：n地址数据 + 1地址checksum + 1地址flag

在eeprom_item_list中注册的参数的key必须顺序从0开始递增
*/

#define START_ADDR CONFIG_ADDR // 分配的eeprom占用的flash的扇区起始地址
#define ADDR_NUM 1024 // 分配地址数量

// 错误掩码
#define ERROR_MASK_ERASE_FAIL    (1 << 0) // 扇区擦除失败
#define ERROR_MASK_WRITE_FAIL    (1 << 1) // 扇区写入失败
#define ERROR_MASK_ER_DATA_ERROR (1 << 2) // EEPROM中数据出错

typedef struct{
    uint16_t key; // 每个参数一个唯一 ID
    uint16_t len; // 长度（以 16bit 为单位）
    void *ramAddr; // 参数在 RAM 中的地址
}eeprom_item_t;

typedef struct{
    uint16_t item_num; // 参数数量
}eeprom_t;

void eeprom_init(void);
uint16_t load_eeprom_to_ram(void);
uint16_t load_ram_item_to_eeprom_from_key(uint16_t key);

uint16_t flash_erase_sector(uint16_t sectorMask);
uint16_t flash_program(uint16_t *flashAddr, uint16_t *dataBuf, uint16_t length);

#endif
