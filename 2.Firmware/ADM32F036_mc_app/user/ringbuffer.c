#include "ringbuffer.h"

/**
 * @brief 静态创建一个环形缓冲区
 * @param ringbuffer 环形缓冲区控制块指针
 * @param buffer 传入外部静态创建的环形缓冲区需要的内存指针
 * @param buffer_size 环形缓冲区大小
 */
void ringbuffer_init(ringbuffer_t *ringbuffer, uint16_t *buffer, uint32_t size)
{
    ringbuffer->buffer= buffer;
    ringbuffer->size = size;
    ringbuffer->in = 0;
    ringbuffer->out = 0;
}

/**
 * @brief 将数据放入环形缓冲区
 * @param ringbuffer 环形缓冲区控制块指针
 * @param in 待添加进缓冲区的数据缓冲区的指针
 * @param len 待添加进缓冲区的数据缓冲区的长度(Byte)
 */
#pragma CODE_SECTION(ringbuffer_in, "ramfuncs");
void ringbuffer_in(ringbuffer_t *ringbuffer, const void *in, uint32_t len)
{
    uint32_t index = ringbuffer->in & (ringbuffer->size - 1);
    uint32_t tail = ringbuffer->size - index;

    if(tail >= len) 
    {
        memcpy(ringbuffer->buffer + index, in, len);
    } 
    else 
    {
        memcpy(ringbuffer->buffer + index, in, tail);
        memcpy(ringbuffer->buffer, (uint16_t*)in + tail, len - tail);
    }

    ringbuffer->in += len;
}

/**
 * @brief 从环形缓冲区中取数据
 * @param ringbuffer 环形缓冲区控制块指针
 * @param out 用来接数据的缓冲区指针
 * @param len 要取的数据长度
 */
#pragma CODE_SECTION(ringbuffer_out, "ramfuncs");
void ringbuffer_out(ringbuffer_t *ringbuffer, void *out, uint32_t len)
{
    uint32_t index = ringbuffer->out & (ringbuffer->size - 1);
    uint32_t tail = ringbuffer->size - index;

    if(tail >= len) 
    {
        memcpy(out, ringbuffer->buffer + index, len);
    } 
    else 
    {
        memcpy(out, ringbuffer->buffer + index, tail);
        memcpy((uint16_t*)out + tail, ringbuffer->buffer, len - tail);
    }

    ringbuffer->out += len;
}

/**
 * @brief 丢弃环形缓冲区中的数据（只移动out指针，不拷贝数据）
 * @param ringbuffer 环形缓冲区控制块指针
 * @param len 要丢弃的数据长度
 */
void ringbuffer_drop(ringbuffer_t *ringbuffer, uint32_t len)
{
    ringbuffer->out += len;
}

/**
 * @brief 偷看环形缓冲区的一个字节（不移动 out 指针）
 * @param ringbuffer 环形缓冲区控制块指针
 * @return 当前 out 指针位置的字节
 */
uint16_t ringbuffer_peek(ringbuffer_t *ringbuffer)
{
    return ringbuffer->buffer[ringbuffer->out & (ringbuffer->size - 1)];
}

/**
 * @brief 查看环形缓冲区使用空间
 * @param ringbuffer 环形缓冲区控制块指针
 * @return 环形缓冲区已使用空间
 */
#pragma CODE_SECTION(ringbuffer_used, "ramfuncs");
uint32_t ringbuffer_used(ringbuffer_t *ringbuffer)
{
    return ringbuffer->in - ringbuffer->out;
}

/**
 * @brief 查看环形缓冲区剩余空间
 * @param ringbuffer 环形缓冲区控制块指针
 * @return 环形缓冲区剩余空间
 */
#pragma CODE_SECTION(ringbuffer_avail, "ramfuncs");
uint32_t ringbuffer_avail(ringbuffer_t *ringbuffer)
{
    return ringbuffer->size - ringbuffer_used(ringbuffer);
}
