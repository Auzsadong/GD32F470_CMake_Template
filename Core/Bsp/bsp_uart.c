#include "bsp_uart.h"

/* ================== 静态私有变量 ================== */
// 用于存放外部注册进来的接收回调函数
static UART_RxCallback_t s_Uart0_RxCallback = NULL;

/* ================== 静态私有方法 ================== */
static void UART0_Init(uint32_t baudrate) {
    /* 1. 开启时钟 */
    rcu_periph_clock_enable(UART0_TX_RCU);
    rcu_periph_clock_enable(UART0_RX_RCU);
    rcu_periph_clock_enable(UART0_RCU);

    /* 2. 配置 GPIO 复用功能 (AF) - 这是 F4 系列必须的步骤 */
    gpio_af_set(UART0_TX_PORT, UART0_TX_AF, UART0_TX_PIN);
    gpio_af_set(UART0_RX_PORT, UART0_RX_AF, UART0_RX_PIN);

    /* 3. 配置 GPIO 模式 */
    // TX: 复用推挽输出
    gpio_mode_set(UART0_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, UART0_TX_PIN);
    gpio_output_options_set(UART0_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, UART0_TX_PIN);
    // RX: 复用输入 (拉高)
    gpio_mode_set(UART0_RX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, UART0_RX_PIN);
    gpio_output_options_set(UART0_RX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, UART0_RX_PIN);

    /* 4. 配置 USART 外设参数 */
    usart_deinit(UART0_PERIPH);
    usart_baudrate_set(UART0_PERIPH, baudrate);
    usart_receive_config(UART0_PERIPH, USART_RECEIVE_ENABLE);
    usart_transmit_config(UART0_PERIPH, USART_TRANSMIT_ENABLE);
    usart_enable(UART0_PERIPH);

    /* 5. 配置接收中断 (RXNE) */
    usart_interrupt_enable(UART0_PERIPH, USART_INT_RBNE);
    nvic_irq_enable(UART0_IRQ, 2, 0); // 抢占优先级2，子优先级0
}

static void UART0_SendByte(uint8_t data) {
    // 发送数据
    usart_data_transmit(UART0_PERIPH, data);
    // 等待发送缓冲区空标志位 (TBE) 变为 Set
    while(RESET == usart_flag_get(UART0_PERIPH, USART_FLAG_TBE));
}

static void UART0_SendString(const char* str) {
    while (*str) {
        UART0_SendByte(*str++);
    }
}

static void UART0_RegisterRxCallback(UART_RxCallback_t cb) {
    s_Uart0_RxCallback = cb;
}

/* ================== GCC printf 重定向核心 ================== */
/* 当你调用 printf 时，C 标准库最终会调用这个 _write 函数 */
int _write(int file, char *ptr, int len) {
    for (int i = 0; i < len; i++) {
        // 这里默认将 printf 绑定到 DebugUART(USART0) 上
        UART0_SendByte(ptr[i]);
    }
    return len;
}

/* ================== 中断业务分发逻辑 ================== */
/* 这个函数将在 gd32f4xx_it.c 的真实中断服务函数中被调用 */
void BSP_UART0_IRQHandler_Logic(void) {
    // 检查是否是接收非空中断 (RBNE)
    if(RESET != usart_interrupt_flag_get(UART0_PERIPH, USART_INT_FLAG_RBNE)){
        // 读取数据，这会自动清除中断标志位
        uint8_t data = (uint8_t)usart_data_receive(UART0_PERIPH);

        // 如果应用层注册了回调函数，就执行回调，把数据甩出去
        if (s_Uart0_RxCallback != NULL) {
            s_Uart0_RxCallback(data);
        }
    }
}

/* ================== 实例化对象 ================== */
UART_Device_t DebugUART = {
    .Init = UART0_Init,
    .SendByte = UART0_SendByte,
    .SendString = UART0_SendString,
    .RegisterRxCallback = UART0_RegisterRxCallback
};