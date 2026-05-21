#include "MainInclude.h"
#include "iap.h"

extern COPY_TABLE prginRAM;

void main(void)
{
    InitPll(DSP_CLOCK/20);
    copy_prg(&prginRAM);		// Move the program from FLASH to RAM
    InitFlash();				// Initializes the Flash Control registers

    bootloader();
    while(1)
    {

    }
}
