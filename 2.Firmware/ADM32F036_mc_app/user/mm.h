#ifndef MM_H
#define MM_H 

/*          
 *        扇区				  地址范围			 长度(16bit)		分配     
 *  扇区H (8K x 16)     0x3E8000 - 0x3E9FFF       0x2000           APP1
 *  扇区G (8K x 16)     0x3EA000 - 0x3EBFFF       0x2000           APP2
 *  扇区F (8K x 16)     0x3EC000 - 0x3EDFFF       0x2000           APP3
 *  扇区E (8K x 16)     0x3EE000 - 0x3EFFFF       0x2000          CONFIG
 *  扇区D (8K x 16)     0x3F0000 - 0x3F1FFF       0x2000         DOWNLOAD1	
 *  扇区C (15K x 16)    0x3F2000 - 0x3F5BFF       0x3C00         DOWNLOAD2
 *  扇区B (1K x 16)     0x3F5C00 - 0x3F5FFF       0x400          DOWNLOAD3
 *  扇区A (8K x 16)     0x3F6000 - 0x3F7FFF       0x2000    BOOTLOADER+CSM+BEGIN
 */

#define BOOTLOADER_SIZE 0x1F80 // bootloader程序大小，减去扇区A最后面的0x3F7F80~0x3F7FFF的CSM和BEGIN段占用的共128个地址共8*1024-128=8064（0x1F80）个
#define APP_SIZE 0x6000 // 分配3个扇区共24K个地址给app程序空间
#define DOWNLOAD_SIZE 0x6000 // 分配3个扇区共24K个地址给固件更新缓冲区
#define CONFIG_SIZE 0x2000 // 分配一个扇区给用户配置区

#define BOOTLOADER_ADDR 0x3F6000 
#define APP_ADDR 0x3E8000
#define DOWNLOAD_ADDR 0x3F0000
#define CONFIG_ADDR 0x3EE000

#endif
