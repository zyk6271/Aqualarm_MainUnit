/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-11-21     Rick       the first version
 */
#include <rtthread.h>
#include <rtdevice.h>
#include "button.h"
#include "device.h"
#include "pin_config.h"
#include "led.h"

#define DBG_TAG "device"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

rt_sem_t K0_Sem = RT_NULL;
rt_sem_t K0_Long_Sem = RT_NULL;
rt_sem_t K1_Sem = RT_NULL;
rt_sem_t K1_Long_Sem = RT_NULL;
rt_sem_t K0_K1_Long_Sem = RT_NULL;

rt_thread_t button_task = RT_NULL;

uint16_t K0_Long_Sem_Counter = 0;
uint16_t K1_Long_Sem_Counter = 0;
uint8_t K0_OnceFlag = 0;
uint8_t K1_OnceFlag = 0;
uint8_t K0_K1_OnceFlag = 0;

uint8_t Key_Pause_Flag;
uint8_t ANT_SW_Status, ANT_SW_Status_Temp = 0;

extern uint8_t Factory_Flag;

void Key_SemInit(void)
{
    K0_Sem = rt_sem_create("K0", 0, RT_IPC_FLAG_FIFO);
    K0_Long_Sem = rt_sem_create("K0_Long", 0, RT_IPC_FLAG_FIFO);
    K1_Sem = rt_sem_create("K1", 0, RT_IPC_FLAG_FIFO);
    K1_Long_Sem = rt_sem_create("K1_Long", 0, RT_IPC_FLAG_FIFO);
    K0_K1_Long_Sem = rt_sem_create("K0_K1_Long_Sem", 0, RT_IPC_FLAG_FIFO);
}
void Key_Pin_Init(void)
{
    rt_pin_mode(K0, PIN_MODE_INPUT);
    rt_pin_mode(K1, PIN_MODE_INPUT);
    rt_pin_mode(ANT_INT, PIN_MODE_OUTPUT);
    rt_pin_mode(ANT_EXT, PIN_MODE_OUTPUT);
    rt_pin_mode(ANT_SW, PIN_MODE_INPUT);
}
void Key_IO_Init(void)
{
    Key_Pause_Flag = 0;
}
void Key_IO_DeInit(void)
{
    Key_Pause_Flag = 1;
}
void K0_Sem_Release(void *parameter)
{
    rt_sem_release(K0_Sem);
    LOG_D("K0 is Down\r\n");
}
void K1_Sem_Release(void *parameter)
{
    rt_sem_release(K1_Sem);
    LOG_D("K1 is Down\r\n");
}
void K0_LongSem_Release(void *parameter)
{
    if (K0_OnceFlag == 0)
    {
        if (K0_Long_Sem_Counter > 6)
        {
            if (K1_Long_Sem_Counter == 0)
            {
                K0_OnceFlag = 1;
                rt_sem_release(K0_Long_Sem);
                LOG_D("K0 is Long\r\n");
            }
        }
        else
        {
            LOG_I("K0 Long Counter is %d", K0_Long_Sem_Counter++);
        }
    }
}
void K1_LongSem_Release(void *parameter)
{
    if (K1_OnceFlag == 0)
    {
        if (K1_Long_Sem_Counter > 6)
        {
            K1_OnceFlag = 1;
            if (K0_Long_Sem_Counter > 5)
            {
                rt_sem_release(K0_K1_Long_Sem);
                LOG_D("K0_K1 is Long\r\n");
            }
            else
            {
                rt_sem_release(K1_Long_Sem);
                LOG_D("K1 is Long\r\n");
            }
        }
        else
        {
            LOG_I("K1 Long Counter is %d", K1_Long_Sem_Counter++);
        }
    }
}
void k0_K1_LongSem_Release(void)
{
    if (K0_K1_OnceFlag == 0)
    {
        K0_K1_OnceFlag = 1;
        rt_sem_release(K0_K1_Long_Sem);
        LOG_D("K0_K1 is Down\r\n");
    }
}
void K0_LongFree_Release(void *parameter)
{
    K0_OnceFlag = 0;
    K0_K1_OnceFlag = 0;
    K0_Long_Sem_Counter = 0;
    LOG_D("K0 is LongFree\r\n");
}
void K1_LongFree_Release(void *parameter)
{
    K1_OnceFlag = 0;
    K1_Long_Sem_Counter = 0;
    LOG_D("K1 is LongFree\r\n");
}
uint8_t Read_K0_Level(void)
{
    if(Key_Pause_Flag)
    {
        return 1;
    }
    else
    {
        return rt_pin_read(K0);
    }
}
uint8_t Read_K1_Level(void)
{
    if(Key_Pause_Flag)
    {
        return 1;
    }
    else
    {
        return rt_pin_read(K1);
    }
}
void RF_Switch_Pin_Init(void)
{
    ANT_SW_Status = rt_pin_read(ANT_SW);
    rt_pin_write(ANT_EXT, !ANT_SW_Status);
    rt_pin_write(ANT_INT, ANT_SW_Status);
    LOG_I("ANT_EXT is %d , ANT_INT is %d\r\n", !ANT_SW_Status, ANT_SW_Status);
}
void RF_Switch(void)
{
    ANT_SW_Status_Temp = rt_pin_read(ANT_SW);
    if (ANT_SW_Status_Temp != ANT_SW_Status)
    {
        beep_once();
        ANT_SW_Status = ANT_SW_Status_Temp;
        rt_pin_write(ANT_EXT, !ANT_SW_Status);
        rt_pin_write(ANT_INT, ANT_SW_Status);
        LOG_I("ANT_EXT is %d , ANT_INT is %d\r\n", !ANT_SW_Status, ANT_SW_Status);
    }
}
void button_task_entry(void *parameter)
{
    Key_Pin_Init();
    RF_Switch_Pin_Init();
    Button_t Key0;
    Button_t Key1;
    Button_Create("Key0", &Key0, Read_K0_Level, 0);
    Button_Create("Key1", &Key1, Read_K1_Level, 0);
    Button_Attach(&Key0, BUTTON_DOWM, K0_Sem_Release);
    Button_Attach(&Key1, BUTTON_DOWM, K1_Sem_Release);
    Button_Attach(&Key0, BUTTON_LONG, K0_LongSem_Release);
    Button_Attach(&Key1, BUTTON_LONG, K1_LongSem_Release);
    Button_Attach(&Key0, BUTTON_LONG_FREE, K0_LongFree_Release);
    Button_Attach(&Key1, BUTTON_LONG_FREE, K1_LongFree_Release);
    LOG_D("Button Init Success\r\n");
    while (1)
    {
        RF_Switch();
        Button_Process();
        rt_thread_mdelay(10);
    }
}
void Button_Init(void)
{
    button_task = rt_thread_create("button_task", button_task_entry, RT_NULL, 1024, 5, 10);
    rt_thread_startup(button_task);
}
