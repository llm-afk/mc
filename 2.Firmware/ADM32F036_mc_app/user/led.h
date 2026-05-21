#ifndef LED_H
#define LED_H

#include "MainInclude.h"
#include "lpg.h"
#include "od.h"

/*
目前的led模块的设计思路是只显示系统的状态
*/

void led_init(void);
void led_loop(void);

#endif
