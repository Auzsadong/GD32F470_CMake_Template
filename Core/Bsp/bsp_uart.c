#include "bsp_uart.h"

/* ================== 静态私有变量 ================== */
// 帧接收回调函数指针
static UART_RxFrameCallback_t s_Uart0_FrameCallback = NULL;

// DMA 专属接收缓冲区 (极其重要，DMA 会直接把数据写进这个数组)
static uint8_t s_Uart0_RxBuffer[UART0_RX_BUF_SIZE];

/* ================== 静态私有方法 ================== */
static void UART0_Init(uint32_t baudrate) {
    /* 1. 开启时钟 */
    rcu_periph_clock_enable(UART0_TX_RCU);
    rcu_periph_clock_enable(UART0_RX_RCU);
    rcu_periph_clock_enable(UART0_RCU);
    rcu_periph_clock_enable(UART0_DMA_RCU); // 开启 DMA 时钟

    /* 2. 配置 GPIO 复用功能 */
    gpio_af_set(UART0_TX_PORT, UART0_TX_AF, UART0_TX_PIN);
    gpio_af_set(UART0_RX_PORT, UART0_RX_AF, UART0_RX_PIN);

    /* 3. 配置 GPIO 模式 */
    gpio_mode_set(UART0_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, UART0_TX_PIN);
    gpio_output_options_set(UART0_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, UART0_TX_PIN);

    gpio_mode_set(UART0_RX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, UART0_RX_PIN);
    gpio_output_options_set(UART0_RX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, UART0_RX_PIN);

    /* 4. 配置 USART 参数 */
    usart_deinit(UART0_PERIPH);
    usart_baudrate_set(UART0_PERIPH, baudrate);
    usart_receive_config(UART0_PERIPH, USART_RECEIVE_ENABLE);
    usart_transmit_config(UART0_PERIPH, USART_TRANSMIT_ENABLE);

    /* --------------------------------------------------------- */
    /* 5. 核心配置：配置 DMA 接收 (无需 CPU 参与)                  */
    /* --------------------------------------------------------- */
    dma_single_data_parameter_struct dma_init_struct;
    dma_deinit(UART0_DMA, UART0_DMA_CH);

    // DMA 源地址：USART0 的数据寄存器
    dma_init_struct.periph_addr = (uint32_t)&USART_DATA(UART0_PERIPH);
    dma_init_struct.periph_inc  = DMA_PERIPH_INCREASE_DISABLE; // 外设地址不增加

    // DMA 目标地址：我们定义的内存数组
    dma_init_struct.memory0_addr = (uint32_t)s_Uart0_RxBuffer;
    dma_init_struct.memory_inc   = DMA_MEMORY_INCREASE_ENABLE; // 内存地址每次递增

    // 数据宽度：8位 (1个字节)
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    // 传输模式：普通模式 (收完一帧后手动重置)
    dma_init_struct.circular_mode = DMA_CIRCULAR_MODE_DISABLE;
    // 传输方向：外设 -> 内存
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    // 预计传输数量：最大缓冲区大小
    dma_init_struct.number = UART0_RX_BUF_SIZE;
    // 优先级：超高
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;

    // 初始化 DMA 并选择具体的子外设通道 (F4 必须选)
    dma_single_data_mode_init(UART0_DMA, UART0_DMA_CH, &dma_init_struct);
    dma_channel_subperipheral_select(UART0_DMA, UART0_DMA_CH, UART0_DMA_SUB);

    // 启动 DMA 通道，它开始在一旁默默等待数据了
    dma_channel_enable(UART0_DMA, UART0_DMA_CH);

    /* 6. 使能串口的 DMA 接收请求，并配置空闲中断 (IDLE) */
    usart_dma_receive_config(UART0_PERIPH, USART_RECEIVE_DMA_ENABLE); // 告诉串口把收到的数据扔给 DMA
    usart_interrupt_enable(UART0_PERIPH, USART_INT_IDLE);             // 开启总线空闲中断
    nvic_irq_enable(UART0_IRQ, 2, 0);

    /* 7. 最后使能串口 */
    usart_enable(UART0_PERIPH);
}

static void UART0_SendByte(uint8_t data) {
    usart_data_transmit(UART0_PERIPH, data);
    while(RESET == usart_flag_get(UART0_PERIPH, USART_FLAG_TBE));
}

static void UART0_SendString(const char* str) {
    while (*str) {
        UART0_SendByte(*str++);
    }
}

static void UART0_RegisterRxFrameCallback(UART_RxFrameCallback_t cb) {
    s_Uart0_FrameCallback = cb;
}

/* ================== GCC printf 重定向核心 ================== */
int _write(int file, char *ptr, int len) {
    for (int i = 0; i < len; i++) {
        UART0_SendByte(ptr[i]);
    }
    return len;
}

/* ================== 中断业务分发逻辑 (IDLE + DMA 结算) ================== */
void BSP_UART0_IRQHandler_Logic(void) {
    // 检查是否是空闲中断 (IDLE) —— 意味着一帧数据收完了
    if(RESET != usart_interrupt_flag_get(UART0_PERIPH, USART_INT_FLAG_IDLE)) {

        // 1. 清除 IDLE 标志位：GD32 的规矩是先读取 STAT0，再读取 DATA 寄存器
        usart_data_receive(UART0_PERIPH);

        // 2. 停掉 DMA，防止我们在处理时又有新数据冲进来把数组搞乱
        dma_channel_disable(UART0_DMA, UART0_DMA_CH);

        // 3. 计算这一帧的真实长度
        // 算法：总缓冲区大小 - DMA通道还没搬运完的数据量
        uint16_t rx_len = UART0_RX_BUF_SIZE - dma_transfer_number_get(UART0_DMA, UART0_DMA_CH);

        // 4. 将这一整帧数据抛给应用层回调
        if(s_Uart0_FrameCallback != NULL && rx_len > 0) {
            s_Uart0_FrameCallback(s_Uart0_RxBuffer, rx_len);
        }

        // 5. 重置 DMA，准备迎接下一帧
        dma_transfer_number_config(UART0_DMA, UART0_DMA_CH, UART0_RX_BUF_SIZE);
        dma_channel_enable(UART0_DMA, UART0_DMA_CH);
    }
}

/* ================== 实例化对象 ================== */
UART_Device_t DebugUART = {
    .Init = UART0_Init,
    .SendByte = UART0_SendByte,
    .SendString = UART0_SendString,
    .RegisterRxFrameCallback = UART0_RegisterRxFrameCallback
};