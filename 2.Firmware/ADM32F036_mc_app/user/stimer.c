#include "stimer.h"

/**
 * @brief 初始化stimer模块
 * @param stimer 控制块指针
 */
void stimer_init(stimer_t *stimer)
{
    memset(stimer, 0, sizeof(stimer_t));
}

/**
 * @brief 在这个模块中注册新的任务
 * @param stimer 控制块指针
 * @param priority 任务优先级：0~(TASK_NUM_MAX-1) 当一个执行周期中有多个待处理任务时，根据优先级逐个执行
 * @param period 任务的执行周期：单位1ms
 * @param phase 任务执行的相位：考虑到多个相同周期任务的错峰执行需求，0：第一个心跳开始执行，1：第二个心跳开始执行
 * @param taskCallback 待执行的任务回调函数指针
 */
void stimer_addTask(stimer_t *stimer, uint16_t priority, uint16_t period, uint16_t phase, taskCallback_t taskCallback)
{
    stimer->task[priority].period = period;
    stimer->task[priority].phase = phase;
    stimer->task[priority].phase_counter = phase;
    stimer->task[priority].taskCallback = taskCallback;
}

/**
 * @brief 需要定时1ms执行以驱动模块运行
 * @param stimer 控制块指针
 */
void stimer_heartBeat(stimer_t *stimer)
{
    stimer->tick_counter++;
}

/**
 * @brief 任务调度器
 * @param stimer 控制块指针
 * @note 推荐一种使用的策略：创建两个软定时器模块，主循环放一个，定时器中断里面放一个
 *       用于主循环中的软定时器模块中添加耗时可能超出1ms且非实时性的任务例如can通信协议处理的问题
 *       用于中断的软定时器模块中放哪些定时确定且耗时短的例如led闪烁的任务
 */
void stimer_loop(stimer_t *stimer)
{
    if(stimer->tick_counter != stimer->tick_counter_pre)
    {
        stimer->tick_counter_pre = stimer->tick_counter;
        for(uint16_t i=0; i<TASK_NUM_MAX; i++)
        {
            task_t *task = &stimer->task[i];
            if(task->taskCallback != NULL)
            {
                if(task->phase_counter > 0)
                {
                    task->phase_counter--;
                }
                else
                {
                    if(++task->period_counter >= task->period)
                    {
                        task->period_counter = 0;
                        task->taskCallback();
                    }
                }
            }
        }
    }
}

