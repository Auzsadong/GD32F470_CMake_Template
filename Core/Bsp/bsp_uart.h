#ifndef BSP_UART_H
#define BSP_UART_H

#include "gd32f4xx.h"
#include <stdio.h>

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

/* ================== DMA 硬件映射宏定义 (GD32F4 特有) ================== */
// USART0_RX 固定映射到 DMA1 的通道2，子外设4
#define UART0_DMA_RCU    RCU_DMA1
#define UART0_DMA        DMA1
#define UART0_DMA_CH     DMA_CH2
#define UART0_DMA_SUB    DMA_SUBPERI4

/* ================== 接收缓冲区配置 ================== */
// 定义单帧最大接收长度 (可根据实际协议改大，比如 512, 1024)
#define UART0_RX_BUF_SIZE  256

/* ================== 回调函数类型定义 ================== */
// 升级版帧回调：传递缓冲区首地址和实际接收到的数据长度
typedef void (*UART_RxFrameCallback_t)(uint8_t* buffer, uint16_t length);

/* ================== 抽象对象定义 ================== */
typedef struct {
    void (*Init)(uint32_t baudrate);
    void (*SendByte)(uint8_t data);
    void (*SendString)(const char* str);
    void (*RegisterRxFrameCallback)(UART_RxFrameCallback_t cb);
} UART_Device_t;

/* 暴露给外部的串口实例 */
extern UART_Device_t DebugUART;

/* 暴露给系统中断的底层处理逻辑 */
void BSP_UART0_IRQHandler_Logic(void);

#endif /* BSP_UART_H */