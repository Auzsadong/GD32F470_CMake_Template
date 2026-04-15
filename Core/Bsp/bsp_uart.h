#ifndef BSP_UART_H
#define BSP_UART_H

#include "gd32f4xx.h"
#include <stdio.h> // printf 需要

/* ================== USART0 硬件映射宏定义 ================== */
// TX 引脚: PA9
#define UART0_TX_RCU     RCU_GPIOA
#define UART0_TX_PORT    GPIOA
#define UART0_TX_PIN     GPIO_PIN_9
#define UART0_TX_AF      GPIO_AF_7

// RX 引脚: PA10
#define UART0_RX_RCU     RCU_GPIOA
#define UART0_RX_PORT    GPIOA
#define UART0_RX_PIN     GPIO_PIN_10
#define UART0_RX_AF      GPIO_AF_7

// USART 外设
#define UART0_RCU        RCU_USART0
#define UART0_PERIPH     USART0
#define UART0_IRQ        USART0_IRQn

/* ================== 回调函数类型定义 ================== */
// 定义一个函数指针类型，用于接收到数据时回调
typedef void (*UART_RxCallback_t)(uint8_t data);

/* ================== 抽象对象定义 ================== */
typedef struct {
    void (*Init)(uint32_t baudrate);
    void (*SendByte)(uint8_t data);
    void (*SendString)(const char* str);
    void (*RegisterRxCallback)(UART_RxCallback_t cb);
} UART_Device_t;

/* 暴露给外部的串口实例 (比如作为调试串口) */
extern UART_Device_t DebugUART;

/* 暴露给中断系统的底层处理逻辑 */
void BSP_UART0_IRQHandler_Logic(void);

#endif /* BSP_UART_H */