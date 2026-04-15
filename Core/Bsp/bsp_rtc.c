#include "bsp_rtc.h"
#include <stdio.h>

rtc_parameter_struct rtc_initpara;
static __IO uint32_t prescaler_a = 0, prescaler_s = 0;

/*!
    \brief      RTC 时钟预配置 (内部调用)
*/
static void bsp_rtc_pre_config(void)
{
    #if defined (RTC_CLOCK_SOURCE_IRC32K)
        rcu_osci_on(RCU_IRC32K);
        rcu_osci_stab_wait(RCU_IRC32K);
        rcu_rtc_clock_config(RCU_RTCSRC_IRC32K);
        prescaler_s = 0x13F;
        prescaler_a = 0x63;
    #elif defined (RTC_CLOCK_SOURCE_LXTAL)
        rcu_osci_on(RCU_LXTAL);
        rcu_osci_stab_wait(RCU_LXTAL);
        rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);
        prescaler_s = 0xFF;
        prescaler_a = 0x7F;
    #endif

    rcu_periph_clock_enable(RCU_RTC);
    rtc_register_sync_wait();
}

/*!
    \brief      设置默认时间 (替代原厂的串口阻塞输入)
*/
static void bsp_rtc_setup_default(void)
{
    rtc_initpara.factor_asyn = prescaler_a;
    rtc_initpara.factor_syn = prescaler_s;
    rtc_initpara.year = 0x24;            // 2024年 (示例)
    rtc_initpara.month = RTC_JAN;
    rtc_initpara.date = 0x01;
    rtc_initpara.day_of_week = RTC_MONDAY;
    rtc_initpara.display_format = RTC_24HOUR;
    rtc_initpara.am_pm = RTC_AM;
    rtc_initpara.hour = 0x12;
    rtc_initpara.minute = 0x00;
    rtc_initpara.second = 0x00;

    if(ERROR != rtc_init(&rtc_initpara)){
        RTC_BKP0 = BKP_VALUE;
    }
}

/*!
    \brief      BSP RTC 初始化接口
    \note       在系统初始化阶段调用一次即可
*/
void bsp_rtc_init(void)
{
    uint32_t rtcsrc_flag = 0;

    /* 1. 使能 PMU 时钟并开启备份域写权限 */
    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    /* 2. 时钟预配置 */
    bsp_rtc_pre_config();

    /* 3. 检查是否是第一次配置 (掉电或首次烧录) */
    rtcsrc_flag = GET_BITS(RCU_BDCTL, 8, 9);
    if ((BKP_VALUE != RTC_BKP0) || (0x00 == rtcsrc_flag)){
        bsp_rtc_setup_default(); // 写入默认时间
    }

    rcu_all_reset_flag_clear();
}

/*!
    \brief      设定指定秒数后的单次唤醒
    \param[in]  seconds: 多少秒后唤醒
    \note       如果你不需要周期唤醒，需在中断服务函数中将其关闭
*/
void bsp_rtc_set_wakeup(uint32_t seconds)
{
    /* 如果传入0，直接关闭唤醒功能 */
    if (seconds == 0) {
        rtc_wakeup_disable();
        return;
    }

    /* 配置 EXTI 22 线 (RTC 唤醒专用) */
    exti_flag_clear(EXTI_22);
    exti_init(EXTI_22, EXTI_INTERRUPT, EXTI_TRIG_RISING);
    nvic_irq_enable(RTC_WKUP_IRQn, 0, 0);

    /* 配置前先关闭并清除标志位 */
    rtc_wakeup_disable();
    rtc_flag_clear(RTC_FLAG_WT);

    /* 使用 ck_spre 时钟 (默认1Hz) */
    rtc_wakeup_clock_set(WAKEUP_CKSPRE);

    /* 设置重装载值。官方手册规定，实际周期 = value + 1 */
    rtc_wakeup_timer_set(seconds - 1);

    /* 使能中断和定时器 */
    rtc_interrupt_enable(RTC_INT_WAKEUP);
    rtc_wakeup_enable();
}

/*!
    \brief      获取并打印当前时间 (调试用)
*/
void bsp_rtc_get_time(void)
{
    rtc_current_time_get(&rtc_initpara);
    printf("Time: %0.2x:%0.2x:%0.2x \n\r", rtc_initpara.hour, rtc_initpara.minute, rtc_initpara.second);
}