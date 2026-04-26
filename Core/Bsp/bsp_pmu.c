#include "bsp_pmu.h"
#include "bsp_led.h"
#include <stdio.h>

/**
 * @brief  配置 PA0 为 EXTI 线 0 下降沿中断
 */
void bsp_pmu_exti_init(void)
{
    /* 0. 强制设置一下 NVIC 分组，防止默认的优先级配置失效 */
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    /* 使能 GPIOA 和 SYSCFG 时钟 */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_SYSCFG);

    /* 配置 PA0 为浮空输入 */
    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_0);

    /* 连接 EXTI 线 0 到 PA0 */
    syscfg_exti_line_config(EXTI_SOURCE_GPIOA, EXTI_SOURCE_PIN0);

    /* 【关键修改】：改为双边沿触发 (EXTI_TRIG_BOTH)！
     * 这样无论你是按下还是松开，只要电平有变化，都能无死角强制唤醒 */
    exti_init(EXTI_0, EXTI_INTERRUPT, EXTI_TRIG_BOTH);

    exti_interrupt_flag_clear(EXTI_0);

    /* 使能 EXTI0 中断，优先级设为最高 (0) */
    nvic_irq_enable(EXTI0_IRQn, 0U, 0U);
}

/**
 * @brief  进入深度睡眠模式 (Deep-sleep)
 */
void bsp_pmu_enter_deepsleep(void)
{
    /* 1. 使能 PMU 时钟 */
    rcu_periph_clock_enable(RCU_PMU);

    /* 2. 清除唤醒标志 (虽然主要是 Standby 用，但清一下防干扰) */
    pmu_flag_clear(PMU_FLAG_WAKEUP);

    /* 3. 确保 EXTI 中断标志是干净的，防止刚进去就被瞬间弹起来 */
    exti_interrupt_flag_clear(EXTI_0);

    /* 4. 进入深度睡眠模式 (LDO低功耗，等待中断指令 WFI) */
    pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, PMU_LOWDRIVER_DISABLE, WFI_CMD);
    /* ========================================= */
    /* ======== 芯片在这里停止执行，开始睡觉 ======== */
    /* ========================================= */

    /* ======== 被 PA0 唤醒后，代码从这里继续跑 ======== */
    LED2.On();
    /* 5. 醒来第一件事：恢复系统时钟！！！ */
    bsp_pmu_restore_clock();
}

/**
 * @brief  深度睡眠唤醒后恢复系统时钟到最高频率
 * @note   深度睡眠会关闭 PLL，唤醒时系统默认使用内部 16M IRC 时钟。
 * 必须重新配置，否则唤醒后系统会跑得很慢 (16MHz)，导致串口乱码、延时错误。
 */
void bsp_pmu_restore_clock(void)
{
    /* 1. 重新开启外部高速晶振 (HXTAL) */
    rcu_osci_on(RCU_HXTAL);
    /* 等待 HXTAL 稳定 */
    rcu_osci_stab_wait(RCU_HXTAL);

    /* 2. 重新开启主 PLL */
    rcu_osci_on(RCU_PLL_CK);
    /* 等待 PLL 稳定 */
    rcu_osci_stab_wait(RCU_PLL_CK);

    /* 3. 重新选择 PLL 作为系统主时钟 (恢复到 240MHz) */
    rcu_system_clock_source_config(RCU_CKSYSSRC_PLLP);
}

/**
 * @brief  EXTI0 中断服务函数 (当 PA0 产生下降沿时触发)
 * @note   注意：这个函数通常可以放在 gd32f4xx_it.c 中。
 * 为了方便，放在这里也可以。如果报错重定义，请将它剪切到 gd32f4xx_it.c 中。
 */
void EXTI0_IRQHandler(void)
{
    if (exti_interrupt_flag_get(EXTI_0) != RESET) {
        /* 清除中断标志位 */
        exti_interrupt_flag_clear(EXTI_0);

        /* 可以在这里做一些轻量级的唤醒标记操作，但通常啥也不用干，清除标志即可 */
    }
}