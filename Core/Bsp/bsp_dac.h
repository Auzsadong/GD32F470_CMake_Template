#ifndef __BSP_DAC_H
#define __BSP_DAC_H

#include "gd32f4xx.h"

/* 定义 DAC 数据传输模式枚举 */
typedef enum {
    DAC_IT = 0,    // 普通CPU中断模式 (利用TIMER中断周期性搬运数据)
    DAC_DMA,       // DMA 硬件自动搬运模式
    DAC_CPU        // 纯软件手动触发模式
} DAC_TransferMode_Enum;

/**
 * @brief  初始化并启动 DAC 输出
 * @param  gpio_periph: GPIO 端口 (如 GPIOA)
 * @param  pin:         GPIO 引脚 (如 GPIO_PIN_4)
 * @param  mode:        传输模式 (DAC_IT / DAC_DMA / DAC_CPU)
 * @param  data:        要转换的数据数组指针
 * @param  len:         数据数组的长度
 */
void GD32_dac_bsp_Start(uint32_t gpio_periph, uint32_t pin, DAC_TransferMode_Enum mode, const uint8_t *data, uint32_t len);

/* 供 DAC_CPU 模式使用的单次手动输出接口 */
void GD32_dac_bsp_SetData(uint32_t pin, uint8_t value);

/**
 * @brief  动态设置 DAC 输出波形的频率
 * @param  dac_periph: DAC 外设 (如 DAC0)
 * @param  freq:       期望输出的完整波形频率 (单位: Hz, 支持浮点数)
 */
void GD32_DAC_TIM5_Base(uint32_t dac_periph, float freq);

#endif /* __BSP_DAC_H */