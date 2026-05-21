#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint16_t *buffer;
    uint32_t size;      
    uint32_t in;    
    uint32_t out;  
}ringbuffer_t;

void ringbuffer_init(ringbuffer_t *ringbuffer, uint16_t *buffer, uint32_t size);
void ringbuffer_in(ringbuffer_t *ringbuffer, const void *in, uint32_t len);
void ringbuffer_out(ringbuffer_t *ringbuffer, void *out, uint32_t len);
void ringbuffer_drop(ringbuffer_t *ringbuffer, uint32_t len);
uint16_t ringbuffer_peek(ringbuffer_t *ringbuffer);
uint32_t ringbuffer_used(ringbuffer_t *ringbuffer);
uint32_t ringbuffer_avail(ringbuffer_t *ringbuffer);

#endif
