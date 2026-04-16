#ifndef __BSP_ADC_H
#define __BSP_ADC_H

#include "gd32f4xx.h"

/* 传输模式选择枚举 */
typedef enum {
    ADC_TRANS_MODE_POLLING = 0, /* 轮询模式 (当前未实现具体逻辑，留作扩展) */
    ADC_TRANS_MODE_IT,          /* 中断模式 */
    ADC_TRANS_MODE_DMA          /* DMA模式 */
} adc_transfer_mode_t;

/**
 * @brief 单通道独立ADC初始化
 * @param adc_periph: ADC0, ADC1, ADC2
 * @param gpio_port:  例如 GPIOC
 * @param gpio_pin:   例如 GPIO_PIN_0
 * @param mode:       例如 ADC_TRANS_MODE_DMA
 */
void bsp_adc_Start_Init(uint32_t adc_periph, uint32_t gpio_port, uint32_t gpio_pin, adc_transfer_mode_t mode);

/**
 * @brief 双通道复用ADC初始化
 * @param adc_periph: ADC0, ADC1, ADC2
 * @param port1/pin1: 第1个通道的引脚
 * @param port2/pin2: 第2个通道的引脚
 */
void bsp_adc_Start_Two_Init(uint32_t adc_periph,
                            uint32_t port1, uint32_t pin1,
                            uint32_t port2, uint32_t pin2,
                            adc_transfer_mode_t mode);

/**
 * @brief 获取对应ADC、对应序列的采样值
 * @param adc_periph: ADC0, ADC1, ADC2
 * @param index:      获取第几个通道的值 (单通道传0，双通道传0或1)
 */
uint16_t bsp_get_adc_value(uint32_t adc_periph, uint8_t index);

#endif /* __BSP_ADC_H */