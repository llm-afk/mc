#include "MainInclude.h"
#include "iap.h"

extern COPY_TABLE prginRAM;

void main(void)
{
    DisableDog();			    // Disable the watchdog
    InitPll(DSP_CLOCK/10);
    copy_prg(&prginRAM);		// Move the program from FLASH to RAM
    InitFlash();				// Initializes the Flash Control registers

    bootloader();
    while(1)
    {

    }
}
