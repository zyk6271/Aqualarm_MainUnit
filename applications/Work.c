/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-11-22     Rick       the first version
 */
#include <rtthread.h>
#include <rtdevice.h>
#include "device.h"
#include "led.h"
#include "work.h"
#include "pin_config.h"
#include "led.h"
#include "status.h"
#include "moto.h"
#include "Flashwork.h"
#include "gateway.h"

#define DBG_TAG "work"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

extern uint8_t ValveStatus;

uint8_t WarningNowStatus = 0;
uint8_t WarningPastStatus = 0;
uint8_t WarningStatus = 0;
uint8_t ValvePastStatus = 0;
uint8_t Water_Alarm_Pause;

rt_thread_t WaterScan_t = RT_NULL;

void WarningWithPeak(uint8_t status)
{
    if (Detect_Learn())
    {
        switch (status)
        {
        case 0: //测水线掉落恢复
            WarUpload_GW(1, 0, 3, 0); //掉落消除报警
            if (GetNowStatus() == Open || GetNowStatus() == Close || GetNowStatus() == MasterLostPeak)
            {
                BackToNormal();
                loss_led_stop();
            }
            break;
        case 1: //测水线掉落
            Warning_Enable_Num(3);
            LOG_D("MasterLostPeakWarning\r\n");
            break;
        case 2: //测水线短路
            Warning_Enable_Num(4);
            break;
        case 3: //测水线短路解除
            MasterStatusChangeToDeAvtive();
            break;
        }
    }
}
void WaterScan_Clear(void)
{
    WarningPastStatus = 0;
    WarningNowStatus = 0;
    WarningStatus = 0;
}
uint8_t Get_Peak_ON_Level(void)
{
    if (!Water_Alarm_Pause)
    {
        return rt_pin_read(Peak_ON);
    }
    else
    {
        return 1;
    }
}
uint8_t Get_Peak_LOSS_Level(void)
{
    if (!Water_Alarm_Pause)
    {
        return rt_pin_read(Peak_Loss);
    }
    else
    {
        return 0;
    }
}
void WaterScan_Callback(void *parameter)
{
    uint8_t Peak_ON_Level = 0;
    uint8_t Peak_Loss_Level = 0;

    LOG_D("WaterScan Init Success\r\n");
    while (1) //插入是0，短路是0
    {
        Peak_ON_Level = Get_Peak_ON_Level();
        Peak_Loss_Level = Get_Peak_LOSS_Level();
        if (Peak_Loss_Level != 0)
        {
            WarningNowStatus = 1; //测水线掉落
            LOG_W("Peak_Loss is active\r\n");
        }
        else
        {
            if (Peak_ON_Level == 0)
            {
                WarningNowStatus = 2; //测水线短路
                LOG_W("Peak_ON is active\r\n");
            }
            else
                WarningNowStatus = 0; //状态正常
        }
        if (WarningNowStatus != WarningPastStatus)
        {
            if (WarningPastStatus == 2 && WarningNowStatus == 0)
            {
                if (WarningStatus != 1 << 0)
                {
                    WarningStatus = 1 << 0;
                    WarningWithPeak(3);
                    LOG_W("Change Status to Deactive\r\n");
                }
            }
            else if (WarningPastStatus == 2 && WarningNowStatus == 1)
            {
                if (WarningStatus != 1 << 1)
                {
                    WarningStatus = 1 << 1;
                }
            }
            else if (WarningPastStatus == 0 && WarningNowStatus == 1)
            {
                if (WarningStatus != 1 << 2)
                {
                    WarningWithPeak(1);
                    WarningPastStatus = WarningNowStatus;
                    WarningStatus = 1 << 2;
                }
            }
            else if (WarningPastStatus == 0 && WarningNowStatus == 2)
            {
                if (WarningStatus != 1 << 3)
                {
                    WarningWithPeak(2);
                    WarningPastStatus = WarningNowStatus;
                    WarningStatus = 1 << 3;
                }
            }
            else if (WarningPastStatus == 1 && WarningNowStatus == 0)
            {
                if (WarningStatus != 1 << 4)
                {
                    WarningWithPeak(0);
                    WarningPastStatus = WarningNowStatus;
                    WarningStatus = 1 << 4;
                }
            }
        }
        rt_thread_mdelay(500);
    }
}
void WaterScan_IO_Init(void)
{
    Water_Alarm_Pause = 0;
}
void WaterScan_IO_DeInit(void)
{
    Water_Alarm_Pause = 1;
}
void WaterScan_Init(void)
{
    rt_pin_mode(Peak_ON, PIN_MODE_INPUT);
    rt_pin_mode(Peak_Loss, PIN_MODE_INPUT);
    WaterScan_IO_Init();
    WaterScan_t = rt_thread_create("WaterScan", WaterScan_Callback, RT_NULL, 2048, 10, 10);
    if (WaterScan_t != RT_NULL)
    {
        rt_thread_startup(WaterScan_t);
    }
}
void AliveIncrease(void) //心跳使counter增加
{
    Update_All_Time();
}
void AliveDetect(void) //counter检测
{
    Update_All_Time();
}
