
#include "flash_eeprom.h"
#include "canfd.h"
#include "od.h"
#include "encoder.h"

eeprom_t eeprom = {0};
uint16_t eeprom_data_temp[ADDR_NUM] = {0}; // eeprom擦写备份缓冲区

eeprom_item_t eeprom_item_list[] = {
    {0, sizeof(ODObjs.node_id),                 &ODObjs.node_id},
    {1, sizeof(encoder_config_t),               &encoder_config},
    {2, sizeof(ODObjs.in_encoder_offset),       &ODObjs.in_encoder_offset},
    {3, sizeof(ODObjs.ex_encoder_offset),       &ODObjs.ex_encoder_offset},
    {4, sizeof(ODObjs.torque_limit),            &ODObjs.torque_limit},
    {5, sizeof(ODObjs.over_temp_drv_level),     &ODObjs.over_temp_drv_level},
    {6, sizeof(ODObjs.over_temp_motor_level),   &ODObjs.over_temp_motor_level},
    {7, sizeof(ODObjs.heartbeat_Producer_enable),&ODObjs.heartbeat_Producer_enable},
    {8, sizeof(ODObjs.heartbeat_consumer_enable),&ODObjs.heartbeat_consumer_enable},
    {9, sizeof(ODObjs.is_calibrated),           &ODObjs.is_calibrated},
    {10,sizeof(ODObjs.sn_s0),                    &ODObjs.sn_s0},
    {11,sizeof(ODObjs.sn_s1),                    &ODObjs.sn_s1},
    {12,sizeof(ODObjs.sn_s2),                    &ODObjs.sn_s2},
    {13,sizeof(ODObjs.sn_s3),                    &ODObjs.sn_s3},
    {14,sizeof(ODObjs.sn_s4),                    &ODObjs.sn_s4},
    {15,sizeof(ODObjs.sn_s5),                    &ODObjs.sn_s5},
};

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

/**
 * @brief 根据key获取指定参数在eeprom_data_temp的首地址
 * @param key 指定参数的key
 * @return 返回指定参数eeprom_data_temp存储的首地址
 */
static inline uint16_t* get_temp_item_addr_from_key(uint16_t key)
{
    uint16_t addrOffset = 0; 
    for(uint16_t i = 0; i < key; i++) 
    {
        addrOffset += (eeprom_item_list[i].len + 2);
    }
    return (uint16_t *)(eeprom_data_temp + addrOffset);
}

/**
 * @brief 根据key获取指定参数实际ram中的首地址
 * @param key 指定参数的key
 * @return 返回指定参数实际ram中的首地址指针
 */
static inline uint16_t* get_ram_item_addr_from_key(uint16_t key)
{
    return (uint16_t*)eeprom_item_list[key].ramAddr;
}

/**
 * @brief 根据key获取指定参数的长度（占用的地址数）
 * @param key 指定参数的key
 * @return 返回指定参数的长度
 */
static inline uint16_t get_item_len_from_key(uint16_t key)
{
    return eeprom_item_list[key].len;
}

/**
 * @brief 根据key获取数据从 eeprom 加载到 eeprom_data_temp 中指定参数的flag
 * @param key 指定参数的key
 * @return flag
 */
static inline uint16_t get_temp_item_flag_from_key(uint16_t key)
{
    return *(uint16_t *)(get_temp_item_addr_from_key(key) + get_item_len_from_key(key) + 1);
}

/**
 * @brief 计算 temp 中的指定key索引的参数的checksum
 * @param key 指定参数的key
 * @return 计算的checksum
 */
static inline uint16_t cal_temp_item_checksum_from_key(uint16_t key)
{
    uint16_t checksum_temp = 0;
    uint16_t *temp_item_addr = get_temp_item_addr_from_key(key);
    for(uint16_t i=0; i<get_item_len_from_key(key); i++)
    {
        checksum_temp += temp_item_addr[i];
    }
    return checksum_temp;
}

/**
 * @brief 获取 temp 中的指定key索引的参数的checksum
 * @param key 指定参数的key
 * @return 返回的checksum
 */
static inline uint16_t get_temp_item_checksum_from_key(uint16_t key)
{
    return *(uint16_t *)(get_temp_item_addr_from_key(key) + get_item_len_from_key(key));
}

/**
 * @brief 通过key将指定的变量从 ram 中加载到 eeprom_data_temp 中
 * @param key 指定参数的key
 */
static inline void load_ram_item_to_temp_from_key(uint16_t key)
{
    memcpy(get_temp_item_addr_from_key(key), get_ram_item_addr_from_key(key), get_item_len_from_key(key));
}

/**
 * @brief 将 eeprom 中的所有参数加载到 eeprom_data_temp 中
 */
static inline void load_eeprom_item_to_temp(void)
{
    memcpy(eeprom_data_temp, (uint16_t *)START_ADDR, ADDR_NUM);
}

/**
 * @brief 通过key索引 ram 中的某变量计算它的 checksum 然后加载到 eeprom_data_temp 中
 * @param key 指定参数的key
 */
static inline void load_ram_checksum_to_temp_from_key(uint16_t key)
{
    uint16_t *temp_item_addr = get_temp_item_addr_from_key(key);
    uint16_t *ram_item_addr = get_ram_item_addr_from_key(key);
    uint16_t item_len = get_item_len_from_key(key);
    uint16_t checksum_temp = 0; 
    for(uint16_t j=0; j<item_len; j++)
    {
        checksum_temp += ram_item_addr[j];
    }
    *(uint16_t *)(temp_item_addr + item_len) = checksum_temp;
}

/**
 * @brief 通过key索引 temp 中的某变量并置位它的 flag
 * @param key 指定参数的key
 */
static inline void set_temp_flag_from_key(uint16_t key)
{
    *(uint16_t *)(get_temp_item_addr_from_key(key) + get_item_len_from_key(key) + 1) = 1; // 置为temp中的指定item的flag
}

/**
 * @brief 通过key将指定的变量从 eeprom_data_temp 中加载到 ram 中
 * @param key 指定参数的key
 */
static inline void load_temp_item_to_ram_from_key(uint16_t key)
{
    memcpy(get_ram_item_addr_from_key(key), get_temp_item_addr_from_key(key), get_item_len_from_key(key));
}

/**
 * @brief 初始化flash_eeprom模块
 */
void eeprom_init(void)
{
    memset(&eeprom, 0, sizeof(eeprom));
    eeprom.item_num = sizeof(eeprom_item_list)/sizeof(eeprom_item_t); // 获取参数表参数数量
}

/**
 * @brief 通过key将指定的变量值保存到eeprom中
 * @param key 参数的标识ID
 * @return 错误码 0：无错误
 */
#pragma CODE_SECTION(load_ram_item_to_eeprom_from_key, "ramfuncs");
uint16_t load_ram_item_to_eeprom_from_key(uint16_t key)
{  
    uint16_t error_mask = 0; // 错误掩码
    
    load_eeprom_item_to_temp(); // 首先是将eeprom的数据回读到eeprom擦写备份缓冲区

    if(flash_erase_sector(SECTORE) != 0) // 然后擦除该扇区,事先分配的是SECTORE
    {
        error_mask |= ERROR_MASK_ERASE_FAIL; // 扇区擦除失败
    }

    load_ram_item_to_temp_from_key(key); // 加载数据到temp
    load_ram_checksum_to_temp_from_key(key); // 加载数据的checksum到temp中
    set_temp_flag_from_key(key); // 置位temp中该item的flag

    // 然后把 eeprom_data_temp 写回这个扇区
    if(flash_program((uint16_t *)START_ADDR, eeprom_data_temp, ADDR_NUM) != 0) 
    {
        error_mask |= ERROR_MASK_WRITE_FAIL; // 扇区写入失败
    }
    
    return error_mask;
}

/**
 * @brief 将 eeprom 中的所有参数加载更新到 ram 中的变量中
 * @return 错误码 0：无错误
 */
uint16_t load_eeprom_to_ram(void)
{
    uint16_t flag = 0;
    uint16_t error_mask = 0; // 错误掩码
    load_eeprom_item_to_temp(); // 先将eeprom中的参数数据都加载到 eeprom_data_temp 中

    for(uint16_t i=0; i<eeprom.item_num; i++) // 逐个分析temp中的item
    {
        if(get_temp_item_flag_from_key(i) == 0xFFFF) // 表示eeprom中的这个item还没有被写入过，将注册的item的默认值作为第一次的加载值
        {
            load_ram_item_to_temp_from_key(i); // 加载数据到temp
            load_ram_checksum_to_temp_from_key(i); // 加载数据的checksum到temp中
            set_temp_flag_from_key(i); // 置位temp中该item的flag
            flag = 1; // 需要更新eeprom
        }
        else // 使用eeprom中的值
        {
            if(get_temp_item_checksum_from_key(i) == cal_temp_item_checksum_from_key(i)) // temp中的数据校验通过了
            {
                load_temp_item_to_ram_from_key(i); // 加载到ram中
            }
            else // 校验失败，数据损坏，用RAM默认值修复eeprom中的item
            {
                load_ram_item_to_temp_from_key(i); // 用RAM默认值更新temp
                load_ram_checksum_to_temp_from_key(i); // 加载数据的checksum到temp中
                set_temp_flag_from_key(i); // 置位temp中该item的flag
                flag = 1; // 需要更新eeprom
                error_mask |= ERROR_MASK_ER_DATA_ERROR; // eeprom有数据损坏，加载ram默认值
            }
        }
    }

    if(flag == 1) // 表示 eeprom 中的数据需要更新
    {
        if(flash_erase_sector(SECTORE) != 0) // 擦除该扇区,事先分配的是 SECTORE
        {
            error_mask |= ERROR_MASK_ERASE_FAIL; // 扇区擦除失败
        }
        if(flash_program((uint16_t *)START_ADDR, eeprom_data_temp, ADDR_NUM) != 0) // 然后把 eeprom_data_temp 写回这个扇区
        {
            error_mask |= ERROR_MASK_WRITE_FAIL; // 扇区写入失败
        }
    }

    return error_mask;
}
