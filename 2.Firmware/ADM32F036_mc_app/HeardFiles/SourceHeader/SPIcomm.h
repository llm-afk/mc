
#ifndef _SPI_CANCOMM_H_
#define _SPI_CANCOMM_H_

#include "Define.h"
#include "ADP32F03x_Device.h"
#include "MainInclude.h"

#define PRI_CS_H()      (GpioDataRegs.GPASET.bit.GPIO1 = 1)
#define PRI_CS_L()      (GpioDataRegs.GPACLEAR.bit.GPIO1 = 1)

#define SEC_CS_H()      (GpioDataRegs.GPASET.bit.GPIO19 = 1) 
#define SEC_CS_L()      (GpioDataRegs.GPACLEAR.bit.GPIO19 = 1) 

void InitSpi(void);
void ma900_write_reg_crc(uint16_t reg_addr, uint16_t data_val);
void ma900_read_reg_crc(uint16_t reg_addr_start, uint16_t *buf, uint16_t size);
uint16_t get_pri_enc_val(void);
uint16_t get_sec_enc_val(void);

#endif

