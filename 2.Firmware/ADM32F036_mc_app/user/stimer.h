#ifndef STIMER_H
#define STIMER_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define TASK_NUM_MAX 16

typedef void(*taskCallback_t)(void);

typedef struct{
    uint16_t period; 
    uint16_t period_counter;
    uint16_t phase;
    uint16_t phase_counter;
    taskCallback_t taskCallback; 
}task_t;

typedef struct{
    volatile uint32_t tick_counter; 
    volatile uint32_t tick_counter_pre; 
    task_t task[TASK_NUM_MAX];
}stimer_t;

void stimer_init(stimer_t *stimer);
void stimer_addTask(stimer_t *stimer, uint16_t priority, uint16_t period, uint16_t phase, taskCallback_t taskCallback);
void stimer_heartBeat(stimer_t *stimer);
void stimer_loop(stimer_t *stimer);

#endif
