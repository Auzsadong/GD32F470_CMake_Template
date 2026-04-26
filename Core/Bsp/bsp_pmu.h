#ifndef __BSP_PMU_H
#define __BSP_PMU_H

#include "gd32f4xx.h"

/* 配置 PA0 为 EXTI 下降沿中断（用于深度睡眠唤醒） */
void bsp_pmu_exti_init(void);

/* 进入深度睡眠模式 */
void bsp_pmu_enter_deepsleep(void);

/* 唤醒后恢复系统时钟 (非常重要!) */
void bsp_pmu_restore_clock(void);

#endif /* __BSP_PMU_H */