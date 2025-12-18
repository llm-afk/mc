#ifndef IAP_H
#define IAP_H

#include "MainInclude.h"
#include "Flash_ADP32F03x_API_Library.h"
#include "Flash_ADP32F03x_API_Config.h"
#include "mm.h"


//#define APP
#define BOOTLOADER

#define IAP_FLAG_ADDR 0x00BFFF // SRAM的最后一个地址
#define IAP_FLAG_NUM  0xA5A5

#define SET_IAP_FLAG(x)  (*(volatile uint16_t *)IAP_FLAG_ADDR = (uint16_t)(x))
#define GET_IAP_FLAG()   (*(volatile uint16_t *)IAP_FLAG_ADDR)

#if defined(APP)
extern uint32_t addr_offset;
uint16_t clean_download(void);
void jump_to_download(void);
void write_iap_data(uint16_t *data);
#else
void bootloader(void);
#endif

#endif
