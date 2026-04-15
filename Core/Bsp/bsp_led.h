#ifndef BSP_LED_H
#define BSP_LED_H

#include "gd32f4xx.h"

/* ================== LED 硬件映射宏定义 ================== */

// LED1
#define LED1_RCU     RCU_GPIOA
#define LED1_PORT    GPIOA
#define LED1_PIN     GPIO_PIN_4

// LED2
#define LED2_RCU     RCU_GPIOA
#define LED2_PORT    GPIOA
#define LED2_PIN     GPIO_PIN_5

// LED3
#define LED3_RCU     RCU_GPIOA
#define LED3_PORT    GPIOA
#define LED3_PIN     GPIO_PIN_6

// LED4
#define LED4_RCU     RCU_GPIOA
#define LED4_PORT    GPIOA
#define LED4_PIN     GPIO_PIN_7

// LED5
#define LED5_RCU     RCU_GPIOC
#define LED5_PORT    GPIOC
#define LED5_PIN     GPIO_PIN_4

// LED6
#define LED6_RCU     RCU_GPIOC
#define LED6_PORT    GPIOC
#define LED6_PIN     GPIO_PIN_5

/* ================== 抽象对象定义 ================== */

typedef struct {
    uint32_t rcu;
    uint32_t port;
    uint32_t pin;

    void (*Init)(void);
    void (*On)(void);
    void (*Off)(void);
    void (*Toggle)(void);
} LED_Device_t;

// 暴露 6 个 LED 实例
extern LED_Device_t LED1, LED2, LED3, LED4, LED5, LED6;

/* 批量操作接口 */
void BSP_LED_InitAll(void);
void BSP_LED_SystemTest(void);

#endif