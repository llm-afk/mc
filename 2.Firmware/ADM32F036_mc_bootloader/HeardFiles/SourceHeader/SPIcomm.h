
#ifndef _SPI_CANCOMM_H_
#define _SPI_CANCOMM_H_

#include "Define.h"
#include "ADP32F03x_Device.h"
#include "MainInclude.h"

void InitSpi(void);
uint16_t get_main_degree_raw(void);
uint16_t get_ex_degree_raw(void);

#endif /* _SPI_CANCOMM_H_ */

