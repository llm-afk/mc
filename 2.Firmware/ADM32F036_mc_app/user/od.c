#include "od.h"

ODObjs_t ODObjs;
static uint16_t ODObjsCount = 0;

typedef struct {
    uint16_t index;
    void *obj;
    uint16_t datasize;
    uint16_t attribute;
    int (*update_func)(void);
} OD_entry_t;

uint16_t g_need_reboot = 0;

static int SN_update_callback(void)
{
    g_need_reboot = 1;
    return 0;
}

static int ResetDSP_callback(void)
{
    ResetDSP();
    return 0;
}

static const OD_entry_t ODList[] = 
{
    {0x2000, &ODObjs.error_code,                2, ATTR_RAM | ATTR_R,  NULL},
    {0x2002, &ODObjs.control_word,              2, ATTR_RAM | ATTR_RW, MC_controlword_update},

    {0x2004, &ODObjs.sn_s0,                     4, ATTR_ROM | ATTR_RW,  NULL},
    {0x2005, &ODObjs.sn_s1,                     4, ATTR_ROM | ATTR_RW,  NULL},
    {0x2006, &ODObjs.sn_s2,                     4, ATTR_ROM | ATTR_RW,  NULL},
    {0x2007, &ODObjs.sn_s3,                     4, ATTR_ROM | ATTR_RW,  NULL},
    {0x2008, &ODObjs.sn_s4,                     4, ATTR_ROM | ATTR_RW,  NULL},
    {0x2009, &ODObjs.sn_s5,                     4, ATTR_ROM | ATTR_RW,  NULL},
    {0x200A, &ODObjs.sn_s6,                     4, ATTR_ROM | ATTR_RW,  SN_update_callback},
    
    {0x2040, &ODObjs.node_id,                   1, ATTR_ROM | ATTR_RW, ResetDSP_callback},  

    {0x2043, &ODObjs.heartbeat_Producer_enable, 2, ATTR_ROM | ATTR_RW, NULL},
    {0x2044, &ODObjs.heartbeat_consumer_enable, 2, ATTR_ROM | ATTR_RW, NULL},
    
    {0x205B, &ODObjs.torque_limit,              4, ATTR_ROM | ATTR_RW, NULL},
    {0x2060, &ODObjs.over_temp_drv_level,       4, ATTR_ROM | ATTR_RW, NULL},
    {0x2061, &ODObjs.over_temp_motor_level,     4, ATTR_ROM | ATTR_RW, NULL},

    {0x2070, &ODObjs.in_encoder_offset,         2, ATTR_ROM | ATTR_RW, enc_set_zero}, // 标零只需要写入一次0x2070即可
    {0x2071, &ODObjs.ex_encoder_offset,         2, ATTR_ROM | ATTR_RW, NULL}, 
    {0x2072, &ODObjs.is_calibrated,             2, ATTR_ROM | ATTR_RW, NULL},
    {0x2100, &ODObjs.firmware_version,          2, ATTR_RAM | ATTR_R,  NULL},
};

static void dictionary_init(void)
{
    ODObjs.error_code = 0;
    ODObjs.control_word = 0;

    ODObjs.sn_s0 = 0;
    ODObjs.sn_s1 = 0;
    ODObjs.sn_s2 = 0;
    ODObjs.sn_s3 = 0;
    ODObjs.sn_s4 = 0;
    ODObjs.sn_s5 = 0;
    ODObjs.sn_s6 = 0;

    ODObjs.node_id = 1;
    ODObjs.is_calibrated = 0;

    ODObjs.heartbeat_Producer_enable = 0; // 默认关闭心跳上报功能
    ODObjs.heartbeat_consumer_enable = 1; // 默认开启心跳监测功能

    ODObjs.torque_limit = 30.0f;
    ODObjs.over_temp_drv_level = 85.0f;
    ODObjs.over_temp_motor_level = 150.0f;

    ODObjs.in_encoder_offset = 0;
    ODObjs.ex_encoder_offset = 0;
    ODObjs.firmware_version = SOFT_VERSION; 
}

/**
 * @brief 兼容我写的eeprom库的一个补丁吧算是
 * @param idx od obj索引
 * @return eeprom库key索引
 * @note 所有注册在od字典对象中的有rom属性的变量都需要在这里多注册一遍
 */
static uint16_t get_eeprom_key_from_index(uint16_t idx)
{
    switch(idx)
    {
        case 0x2040: return 0;   // node_id
        case 0x2070: return 2;   // in_encoder_offset
        case 0x2071: return 3;   // ex_encoder_offset
        case 0x205B: return 4;   // torque_limit
        case 0x2060: return 5;   // over_temp_drv_level
        case 0x2061: return 6;   // over_temp_motor_level
        case 0x2043: return 7;   // heartbeat_Producer_enable
        case 0x2044: return 8;   // heartbeat_consumer_enable
        case 0x2004: return 10;  // sn_s0
        case 0x2005: return 11;  // sn_s1
        case 0x2006: return 12;  // sn_s2
        case 0x2007: return 13;  // sn_s3
        case 0x2008: return 14;  // sn_s4
        case 0x2009: return 15;  // sn_s5
        case 0x200A: return 16;  // sn_s6
        default: return 0xFF;    // 无效索引，返回错误标识
    }
}

static OD_entry_t *find_entry(uint16_t index)
{
    uint16_t min = 0;
    uint16_t max = ODObjsCount - 1;

    while(min < max) 
    {
        uint16_t cur = (min + max) >> 1;
        OD_entry_t* entry = (OD_entry_t*)&ODList[cur];

        if(index == entry->index) 
        {
            return entry;
        }

        if(index < entry->index) 
        {
            max = (cur > 0) ? (cur - 1) : cur;
        } 
        else 
        {
            min = cur + 1;
        }
    }

    if(min == max) 
    {
        OD_entry_t* entry = (OD_entry_t*)&ODList[min];
        if(index == entry->index) 
        {
            return entry;
        }
    }

    return NULL;
}

void OD_init(void)
{
    ODObjsCount = sizeof(ODList) / sizeof(OD_entry_t);
    dictionary_init();
}

void OD_check_sn(void)
{
    // 没有sn码，报错
    if(ODObjs.sn_s0 == 0 && ODObjs.sn_s1 == 0 && ODObjs.sn_s2 == 0 && 
       ODObjs.sn_s3 == 0 && ODObjs.sn_s4 == 0 && ODObjs.sn_s5 == 0 && ODObjs.sn_s6 == 0)
    {
        ODObjs.error_code |= ERR_NO_SN;
        return;
    }

    // 解析硬件版本段 (sn_s3)
    // 根据规则，sn_s3是4个ASCII字符(ABCD)，在小端模式下：
    // [7:0]   A位 (结构变动)
    // [15:8]  B位 (芯片平台)
    // [23:16] C位 (电机型号)
    // [31:24] D位 (硬件版本)
    uint16_t b_bit_platform = (ODObjs.sn_s3 >> 8) & 0xFF;
    uint16_t c_bit_motor    = (ODObjs.sn_s3 >> 16) & 0xFF;
    uint16_t d_bit_version  = (ODObjs.sn_s3 >> 24) & 0xFF;

    // 要求适配：B=2 (ADM平台), C=2 (C2_xinzhi), D=1 (硬件版本1) -> "221"
    if(b_bit_platform == '2' && c_bit_motor == '2' && d_bit_version == '1')
    {
        ODObjs.error_code &= ~ERR_NO_SN; // 校验通过，放行
    }
    else
    {
        ODObjs.error_code |= ERR_NO_SN;  // 不匹配本驱动器，锁定
    }
}

uint16_t OD_read(uint16_t idx, uint16_t *data)
{
    uint16_t cs = CS_ERR;
    memset(data, 0, 2);
    OD_entry_t *entry = find_entry(idx);

    if(entry != NULL && (entry->attribute & ATTR_R))
    {
        switch(entry->datasize)
        {
            case 1:
            {
                data[0] = __byte(entry->obj, 0);
                cs = CS_R_ACK_1;
                break;
            }
            case 2:
            {
                data[0] = (__byte(entry->obj, 1) << 8) | __byte(entry->obj, 0);
                cs = CS_R_ACK_2;
                break;
            }
            case 3:
            {
                data[0] = (__byte(entry->obj, 1) << 8) | __byte(entry->obj, 0);
                data[1] = __byte(entry->obj, 2);
                cs = CS_R_ACK_3;
                break;
            }
            case 4:
            {
                data[0] = (__byte(entry->obj, 1) << 8) | __byte(entry->obj, 0);
                data[1] = (__byte(entry->obj, 3) << 8) | __byte(entry->obj, 2);
                cs = CS_R_ACK_4;
                break;
            }
        }
    }
    return cs;
}

uint16_t OD_write_1(uint16_t idx, uint16_t *data)
{
    uint16_t cs = CS_ERR;
    OD_entry_t *entry = find_entry(idx);
    
    if((entry != NULL) && (entry->attribute & ATTR_W) && (entry->datasize == 1))
    {
        // if(__byte(entry->obj, 0) != __byte(data, 0))
        // {
            __byte(entry->obj, 0) = __byte(data, 0);
            if(entry->attribute & ATTR_ROM)
            {
                load_ram_item_to_eeprom_from_key(get_eeprom_key_from_index(idx));
                cs = CS_W_ACK;
            }
            else
            {
                cs = CS_W_ACK;
            }
        // }
        // else
        // {
        //     cs = CS_W_ACK;
        // }
    }

    if(cs == CS_W_ACK && entry->update_func != NULL) 
    {
        if(0 != entry->update_func())
        {
            cs = CS_ERR;
        }
    }

    memset(data, 0, 2);

    return cs;
}

uint16_t OD_write_2(uint16_t idx, uint16_t *data)
{
    uint16_t cs = CS_ERR;
    OD_entry_t *entry = find_entry(idx);
    
    if((entry != NULL) && (entry->attribute & ATTR_W) && (entry->datasize == 2))
    {
        // if(*(uint16_t *)entry->obj != *(uint16_t *)data)
        // {
            *(uint16_t *)entry->obj = *(uint16_t *)data;
            if(entry->attribute & ATTR_ROM)
            {
                load_ram_item_to_eeprom_from_key(get_eeprom_key_from_index(idx));
                cs = CS_W_ACK;
            }
            else
            {
                cs = CS_W_ACK;
            }
        // }
        // else
        // {
        //     cs = CS_W_ACK;
        // }
    }
    
    if(cs == CS_W_ACK && entry->update_func != NULL) 
    {
        if(0 != entry->update_func())
        {
            cs = CS_ERR;
        }
    }

    memset(data, 0, 2);

    return cs;
}

uint16_t OD_write_4(uint16_t idx, uint16_t *data)
{
    uint16_t cs = CS_ERR;
    OD_entry_t *entry = find_entry(idx);
    
    if((entry != NULL) && (entry->attribute & ATTR_W) && (entry->datasize == 4))
    {
        // if(*(uint32_t *)entry->obj != *(uint32_t *)data)
        // {
            *(uint32_t *)entry->obj = *(uint32_t *)data;
            if(entry->attribute & ATTR_ROM)
            {
                load_ram_item_to_eeprom_from_key(get_eeprom_key_from_index(idx));
                cs = CS_W_ACK;
            }
            else
            {
                cs = CS_W_ACK;
            }
        // }
        // else
        // {
        //     cs = CS_W_ACK;
        // }
    }
    
    if(cs == CS_W_ACK && entry->update_func != NULL) 
    {
        if(0 != entry->update_func())
        {
            cs = CS_ERR;
        }
    }

    memset(data, 0, 2);

    return cs;
}
