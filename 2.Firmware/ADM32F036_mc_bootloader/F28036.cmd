MEMORY
{
PAGE 0 :
   RAMPRG      : origin = 0x008000, length = 0x001400  

   BOOT_SPACE  : origin = 0x3F6000, length = 0x001F80
   CSM_RSVD    : origin = 0x3F7F80, length = 0x000076     
   BEGIN       : origin = 0x3F7FF6, length = 0x000002   
   CSM_PWL     : origin = 0x3F7FF8, length = 0x000008  

   OTP         : origin = 0x3D7800, length = 0x000400      

   IQTABLES    : origin = 0x3FE000, length = 0x000B50   
   IQTABLES2   : origin = 0x3FEB50, length = 0x00008C 
   IQTABLES3   : origin = 0x3FEBDC, length = 0x0000AA	  
   ROM         : origin = 0x3FF27C, length = 0x000D44 
   RESET       : origin = 0x3FFFC0, length = 0x000002    
   VECTORS     : origin = 0x3FFFC2, length = 0x00003E    

PAGE 1 :                                          
   RAMM0       : origin = 0x000100, length = 0x000300   
   RAMM1       : origin = 0x000400, length = 0x000400    
   RAML0       : origin = 0x009400, length = 0x000C00
   RAMH0       : origin = 0x00A000, length = 0x002000
}

 
SECTIONS
{
   .cinit              : > BOOT_SPACE    PAGE = 0 
   .pinit              : > BOOT_SPACE    PAGE = 0 
   .text               : > BOOT_SPACE    PAGE = 0 
   .binit              : > BOOT_SPACE    PAGE = 0
   .ovly               : > BOOT_SPACE    PAGE = 0
   codestart           : > BEGIN         PAGE = 0 
   ramfuncs            :
   {
		-lADP32F036_027_Flash_API_ZONE.lib(.econst)
		-lADP32F036_027_Flash_API_ZONE.lib(.text)
   }LOAD = BOOT_SPACE, RUN = RAMPRG, PAGE = 0, TABLE(_prginRAM)

   .stack              : > RAMM1        PAGE = 1
   .esysmem            : > RAML0        PAGE = 1
   .ebss               : >> RAML0|RAMH0 PAGE = 1

   .econst             : > BOOT_SPACE    PAGE = 0 
   .switch             : > BOOT_SPACE    PAGE = 0

   csmpasswds          : > CSM_PWL      PAGE = 0
   csm_rsvd            : > CSM_RSVD     PAGE = 0
                                               
   .reset              : > RESET        PAGE = 0, TYPE = DSECT
   vectors             : > VECTORS      PAGE = 0, TYPE = DSECT
}
