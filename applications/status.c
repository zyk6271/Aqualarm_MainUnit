/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-11-27     Rick       the first version
 */
#include <rtthread.h>
#include "pin_config.h"
#include "key.h"
#include "led.h"
#include "status.h"
#include "flashwork.h"
#include "radio_encoder.h"
#include "Moto.h"
#include "work.h"
#include "string.h"
#include "rthw.h"
#include "gateway.h"

#define DBG_TAG "status"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

enum Device_Status Now_Status = Close;

extern uint8_t ValveStatus;
extern uint32_t Self_Id;

WariningEvent NowStatusEvent;
WariningEvent SlaverLowPowerEvent;
WariningEvent SlaverUltraLowPowerEvent;
WariningEvent SlaverWaterAlarmActiveEvent;
WariningEvent MasterLostPeakEvent;
WariningEvent MasterWaterAlarmActiveEvent;
WariningEvent OfflineEvent;
WariningEvent NTCWarningEvent;
WariningEvent Moto1FailEvent;
WariningEvent Moto2FailEvent;

rt_timer_t Delay_Timer = RT_NULL;

void Warning_Enable(WariningEvent event)
{
    if(event.priority >= NowStatusEvent.priority)
    {
        NowStatusEvent.last_id = event.warning_id;
        NowStatusEvent.priority = event.priority;
        if(event.callback!=RT_NULL)
        {
            NowStatusEvent.callback = event.callback;
            NowStatusEvent.callback(RT_NULL);
        }
        LOG_D("Warning_Enable Success,warning id is %d , now priority is %d\r\n",event.warning_id,event.priority);
    }
    else
    {
        LOG_D("Warning_Enable Fail last is %d Now is %d\r\n",NowStatusEvent.priority,event.priority);
    }
}
void Warning_Enable_Num(uint8_t id)
{
    switch(id)
    {
    case 1:Warning_Enable(SlaverUltraLowPowerEvent);break;
    case 2:Warning_Enable(SlaverWaterAlarmActiveEvent);break;
    case 3:Warning_Enable(MasterLostPeakEvent);break;
    case 4:Warning_Enable(MasterWaterAlarmActiveEvent);break;
    case 5:Warning_Enable(OfflineEvent);break;
    case 6:Warning_Enable(Moto1FailEvent);break;
    case 7:Warning_Enable(SlaverLowPowerEvent);break;
    case 8:Warning_Enable(NTCWarningEvent);break;
    case 9:Warning_Enable(Moto2FailEvent);break;
    }
}
void Warning_Disable(void)
{
    NowStatusEvent.last_id = 0;
    NowStatusEvent.priority = 0;
    BackToNormal();
    LOG_I("Warning is Disable\r\n");
}
void SlaverLowBatteryWarning(void *parameter)
{
    led_slave_low_start();
    //Now_Status = SlaverLowPower;
    LOG_I("SlaverLowBatteryWarning\r\n");
}
void SlaverUltraLowBatteryWarning(void *parameter)
{
    Moto_Close(OtherOff);
    led_slave_low_start();
    Now_Status = SlaverUltraLowPower;
    LOG_I("SlaverUltraLowBatteryWarning\r\n");
}
void SlaverWaterAlarmWarning(void *parameter)
{
    Moto_Close(OtherOff);
    led_water_alarm_start();
    Now_Status = SlaverWaterAlarmActive;
    LOG_I("SlaverWaterAlarmWarning\r\n");
}
void MasterLostPeakWarning(void *parameter)
{
    Now_Status = MasterLostPeak;
    WarUpload_GW(1,0,3,1);//掉落报警
    led_master_lost_start();
    LOG_I("MasterLostPeakWarning\r\n");
}
void MasterStatusChangeToDeAvtive(void)
{
    Now_Status = MasterWaterAlarmDeActive;
    LOG_I("MasterStatusChangeToDeAvtive\r\n");
}
void MasterWaterAlarmWarning(void *parameter)
{
    Moto_Close(NormalOff);
    WarUpload_GW(1,0,1,1);//主控水警
    led_water_alarm_start();
    Now_Status = MasterWaterAlarmActive;
    LOG_I("MasterWaterAlarmWarning\r\n");
}
void NTCWarningEvent_Callback(void *parameter)
{
    Moto_Close(NormalOff);
    WarUpload_GW(1,0,8,1);//NTC报警
    led_ntc_alarm();
    Now_Status = NTCWarning;
    LOG_I("NTCWarning\r\n");
}
void Delay_Timer_Callback(void *parameter)
{
    Moto_Close(OtherOff);
    ControlUpload_GW(1,0,1,0);
    LOG_D("Delay_Timer_Callback is Now\r\n");
}
void Remote_Open(void)
{
    if(Now_Status==Close || Now_Status==Open)
    {
        LOG_D("Remote_Open\r\n");
        Moto_Open(OtherOpen);
    }
    else {
        LOG_I("Remote_Open Fail,Now is %d",Now_Status);
    }
}
void Remote_Close(void)
{
    Moto_Close(OtherOff);
}
void Delay_Timer_OpenDoor(uint32_t device_id)
{
    if(GetNowStatus()==Close || GetNowStatus()==Open)
    {
        rt_timer_start(Delay_Timer);
        ControlUpload_GW(1,device_id,3,1);
    }
}
void Delay_Timer_Stop(void)
{
    rt_timer_stop(Delay_Timer);
}
void Delay_Timer_CloseDoor(uint32_t device_id)
{
    ControlUpload_GW(1,device_id,3,0);
    rt_timer_stop(Delay_Timer);
}
void OfflineWarning(void *parameter)
{
    if(Now_Status!=Offline)
    {
        Moto_Close(NormalOff);
        Now_Status = Offline;
        LOG_I("OfflineWarning\r\n");
        led_offline_start();
    }
    else
    {
        LOG_I("Already OfflineWarning Now\r\n");
    }
}
void Moto1FailCallback(void *parameter)
{
    WarUpload_GW(1,0,2,2);//MOTO1报警
    led_moto_fail_start();
    Now_Status = MotoFail;
    LOG_I("MotoFail\r\n");
}
void Moto2FailCallback(void *parameter)
{
    WarUpload_GW(1,0,2,3);//MOTO2报警
    led_moto_fail_start();
    Now_Status = MotoFail;
    LOG_I("MotoFail\r\n");
}
void OfflineDisableWarning(void)
{
    if(Now_Status == Offline)
    {
        Warning_Disable();
        LOG_I("Disable OfflineWarning\r\n");
    }
}
void RadioInitFail(void)
{
    rt_hw_cpu_reset();
}
void WarningEventInit(uint8_t warning_id,uint8_t priority,WariningEvent *event,void (*callback)(void*))
{
    memset(event,0,sizeof(&event));
    event->warning_id = warning_id;
    event->last_id = 0;
    event->priority = priority;
    event->callback = callback;
}
void WarningInit(void)
{
    WarningEventInit(7,4,&SlaverLowPowerEvent,SlaverLowBatteryWarning);
    WarningEventInit(2,8,&SlaverWaterAlarmActiveEvent,SlaverWaterAlarmWarning);
    WarningEventInit(3,1,&MasterLostPeakEvent,MasterLostPeakWarning);
    WarningEventInit(4,7,&MasterWaterAlarmActiveEvent,MasterWaterAlarmWarning);
    WarningEventInit(5,5,&OfflineEvent,OfflineWarning);
    WarningEventInit(6,3,&Moto1FailEvent,Moto1FailCallback);
    WarningEventInit(9,3,&Moto2FailEvent,Moto2FailCallback);
    WarningEventInit(1,6,&SlaverUltraLowPowerEvent,SlaverUltraLowBatteryWarning);
    WarningEventInit(8,2,&NTCWarningEvent,NTCWarningEvent_Callback);
    WarningEventInit(0,0,&NowStatusEvent,RT_NULL);//本地存储器
    Delay_Timer = rt_timer_create("Delay_Timer", Delay_Timer_Callback, RT_NULL, 4*60*60*1000,RT_TIMER_FLAG_SOFT_TIMER|RT_TIMER_FLAG_ONE_SHOT);
}
uint8_t Detect_Learn(void)
{
    if(Now_Status!=Learn)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
void BackToNormal(void)
{
    WaterScan_Clear();
    beep_stop();
    led_warn_off();
    if(ValveStatus)
    {
        Now_Status = Open;
    }
    else
    {
        Now_Status = Close;
    }
}
uint8_t GetNowStatus(void)
{
    return Now_Status;
}
void SetNowStatus(uint8_t value)
{
    Now_Status = value;
}
