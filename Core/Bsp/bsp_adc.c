#include "bsp_adc.h"
#include <stddef.h>
/* ======================================================== */
/* --- 内部私有数据结构与缓存 --- */
/* ======================================================== */

/* 为3个ADC分别开辟独立的DMA/IT缓冲区 */
static __IO uint16_t adc0_buffer[2] = {0};
static __IO uint16_t adc1_buffer[2] = {0};
static __IO uint16_t adc2_buffer[2] = {0};

/* 【新增】记录每个外设的工作模式和转换完成标志 (索引0:ADC0, 1:ADC1, 2:ADC2) */
static adc_transfer_mode_t s_adc_mode[3] = {ADC_TRANS_MODE_POLLING, ADC_TRANS_MODE_POLLING, ADC_TRANS_MODE_POLLING};
static volatile uint8_t    s_adc_ready[3] = {0};

/* 内部用于传递引脚信息的轻量结构体 */
typedef struct {
    uint32_t rcu_gpio;
    uint32_t gpio_port;
    uint32_t gpio_pin;
    uint8_t  adc_channel;
} internal_adc_ch_t;

/* ======================================================== */
/* --- 内部私有函数：引脚映射与核心初始化逻辑 --- */
/* ======================================================== */

/**
 * @brief 内部引脚映射查找表
 */
static uint8_t _bsp_get_channel_map(uint32_t port, uint32_t pin, uint32_t *rcu_clk)
{
    if (port == GPIOA) {
        *rcu_clk = RCU_GPIOA;
        switch (pin) {
            case GPIO_PIN_0: return ADC_CHANNEL_0;
            case GPIO_PIN_1: return ADC_CHANNEL_1;
            case GPIO_PIN_2: return ADC_CHANNEL_2;
            case GPIO_PIN_3: return ADC_CHANNEL_3;
            case GPIO_PIN_4: return ADC_CHANNEL_4;
            case GPIO_PIN_5: return ADC_CHANNEL_5;
            case GPIO_PIN_6: return ADC_CHANNEL_6;
            case GPIO_PIN_7: return ADC_CHANNEL_7;
        }
    } else if (port == GPIOB) {
        *rcu_clk = RCU_GPIOB;
        if (pin == GPIO_PIN_0) return ADC_CHANNEL_8;
        if (pin == GPIO_PIN_1) return ADC_CHANNEL_9;
    } else if (port == GPIOC) {
        *rcu_clk = RCU_GPIOC;
        switch (pin) {
            case GPIO_PIN_0: return ADC_CHANNEL_10;
            case GPIO_PIN_1: return ADC_CHANNEL_11;
            case GPIO_PIN_2: return ADC_CHANNEL_12;
            case GPIO_PIN_3: return ADC_CHANNEL_13;
            case GPIO_PIN_4: return ADC_CHANNEL_14;
            case GPIO_PIN_5: return ADC_CHANNEL_15;
        }
    }
    *rcu_clk = RCU_GPIOA;
    return ADC_CHANNEL_0;
}

/**
 * @brief 核心硬件配置函数 (负责将传入的参数写入底层寄存器)
 */
static void _bsp_adc_hardware_config(uint32_t adc_periph, internal_adc_ch_t *channels, uint8_t count, adc_transfer_mode_t mode, __IO uint16_t *buf)
{
    uint8_t i;

    /* 1. 时钟配置 */
    if(adc_periph == ADC0)      rcu_periph_clock_enable(RCU_ADC0);
    else if(adc_periph == ADC1) rcu_periph_clock_enable(RCU_ADC1);
    else if(adc_periph == ADC2) rcu_periph_clock_enable(RCU_ADC2);

    adc_clock_config(ADC_ADCCK_PCLK2_DIV8);

    /* 【新增】：记录当前 ADC 的工作模式，供后续中断或读取时做智能判断 */
    if(adc_periph == ADC0)      s_adc_mode[0] = mode;
    else if(adc_periph == ADC1) s_adc_mode[1] = mode;
    else if(adc_periph == ADC2) s_adc_mode[2] = mode;

    /* 2. GPIO 模拟输入配置 */
    for(i = 0; i < count; i++) {
        rcu_periph_clock_enable(channels[i].rcu_gpio);
        gpio_mode_set(channels[i].gpio_port, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, channels[i].gpio_pin);
    }

    /* 3. ADC 基础参数配置 */
    adc_sync_mode_config(ADC_SYNC_MODE_INDEPENDENT);
    adc_data_alignment_config(adc_periph, ADC_DATAALIGN_RIGHT);

    /* 核心逻辑：超过1个通道开启扫描模式，否则关闭 */
    adc_special_function_config(adc_periph, ADC_SCAN_MODE, (count > 1) ? ENABLE : DISABLE);

    // 【修改点】：中断模式下必须关闭连续转换，实现“点一下，转一下”
    if(mode == ADC_TRANS_MODE_IT) {
        adc_special_function_config(adc_periph, ADC_CONTINUOUS_MODE, DISABLE);
    } else {
        adc_special_function_config(adc_periph, ADC_CONTINUOUS_MODE, ENABLE);
    }

    // （已帮你删除了此处原先重复的一行代码）
    adc_channel_length_config(adc_periph, ADC_ROUTINE_CHANNEL, count);

    /* 4. 通道序列挂载 */
    for(i = 0; i < count; i++) {
        adc_routine_channel_config(adc_periph, i, channels[i].adc_channel, ADC_SAMPLETIME_15);
    }
    adc_external_trigger_config(adc_periph, ADC_ROUTINE_CHANNEL, EXTERNAL_TRIGGER_DISABLE);

    /* 5. 传输模式配置 (DMA / IT) */
    if(mode == ADC_TRANS_MODE_DMA) {
        dma_single_data_parameter_struct dma_init_struct;
        uint32_t dma_periph = DMA1;
        dma_channel_enum dma_ch = DMA_CH0;
        uint32_t subperi = DMA_SUBPERI0;

        /* GD32F4的DMA映射：ADC0->CH0(Sub0), ADC1->CH2(Sub1), ADC2->CH1(Sub2) */
        // 注意：如果你最终还是要用 ADC1 的 DMA，并且想避开串口UART0的冲突，请把下面的 DMA_CH2 改成 DMA_CH3，subperi改成 DMA_SUBPERI0
        rcu_periph_clock_enable(RCU_DMA1);
        if(adc_periph == ADC0)      { dma_ch = DMA_CH0; subperi = DMA_SUBPERI0; }
        else if(adc_periph == ADC1) { dma_ch = DMA_CH2; subperi = DMA_SUBPERI1; }
        else if(adc_periph == ADC2) { dma_ch = DMA_CH1; subperi = DMA_SUBPERI2; }

        dma_deinit(dma_periph, dma_ch);
        dma_single_data_para_struct_init(&dma_init_struct);

        dma_init_struct.periph_addr         = (uint32_t)(adc_periph + 0x4CU);
        dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
        dma_init_struct.memory0_addr        = (uint32_t)buf;
        dma_init_struct.memory_inc          = (count > 1) ? DMA_MEMORY_INCREASE_ENABLE : DMA_MEMORY_INCREASE_DISABLE;
        dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_16BIT;
        dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
        dma_init_struct.number              = count;
        dma_init_struct.priority            = DMA_PRIORITY_HIGH;
        dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_ENABLE;

        dma_single_data_mode_init(dma_periph, dma_ch, &dma_init_struct);
        dma_channel_subperipheral_select(dma_periph, dma_ch, subperi);

        dma_channel_enable(dma_periph, dma_ch);
        adc_dma_mode_enable(adc_periph);
        adc_dma_request_after_last_enable(adc_periph);
    }
    else if (mode == ADC_TRANS_MODE_IT) {
        nvic_irq_enable(ADC_IRQn, 1, 0);
        adc_interrupt_enable(adc_periph, ADC_INT_EOC);
    }

    /* 6. 使能、校准、启动 */
    adc_enable(adc_periph);
    for(volatile uint32_t j=0; j<10000; j++);
    adc_calibration_enable(adc_periph);

    // 【修改点】：如果是中断模式，初始化阶段先不“开火”，把触发操作留给读数据的接口
    if (mode != ADC_TRANS_MODE_IT) {
        adc_software_trigger_enable(adc_periph, ADC_ROUTINE_CHANNEL);
    }
}

/* ======================================================== */
/* --- 暴露给应用层的极简接口 --- */
/* ======================================================== */

void bsp_adc_Start_Init(uint32_t adc_periph, uint32_t gpio_port, uint32_t gpio_pin, adc_transfer_mode_t mode)
{
    internal_adc_ch_t ch[1];
    __IO uint16_t *target_buf;

    ch[0].adc_channel = _bsp_get_channel_map(gpio_port, gpio_pin, &ch[0].rcu_gpio);
    ch[0].gpio_port   = gpio_port;
    ch[0].gpio_pin    = gpio_pin;

    /* 自动分配缓冲 */
    if(adc_periph == ADC0) target_buf = adc0_buffer;
    else if(adc_periph == ADC1) target_buf = adc1_buffer;
    else target_buf = adc2_buffer;

    _bsp_adc_hardware_config(adc_periph, ch, 1, mode, target_buf);
}

void bsp_adc_Start_Two_Init(uint32_t adc_periph,
                            uint32_t port1, uint32_t pin1,
                            uint32_t port2, uint32_t pin2,
                            adc_transfer_mode_t mode)
{
    internal_adc_ch_t ch[2];
    __IO uint16_t *target_buf;

    ch[0].adc_channel = _bsp_get_channel_map(port1, pin1, &ch[0].rcu_gpio);
    ch[0].gpio_port   = port1;
    ch[0].gpio_pin    = pin1;

    ch[1].adc_channel = _bsp_get_channel_map(port2, pin2, &ch[1].rcu_gpio);
    ch[1].gpio_port   = port2;
    ch[1].gpio_pin    = pin2;

    if(adc_periph == ADC0) target_buf = adc0_buffer;
    else if(adc_periph == ADC1) target_buf = adc1_buffer;
    else target_buf = adc2_buffer;

    _bsp_adc_hardware_config(adc_periph, ch, 2, mode, target_buf);
}

uint16_t bsp_get_adc_value(uint32_t adc_periph, uint8_t index)
{
    if(index > 1) return 0;

    // 1. 获取对应的数组索引
    uint8_t a_idx = (adc_periph == ADC0) ? 0 : ((adc_periph == ADC1) ? 1 : 2);

    // 2. 如果是中断模式，自动触发并同步等待
    if(s_adc_mode[a_idx] == ADC_TRANS_MODE_IT) {
        s_adc_ready[a_idx] = 0; // 清除标志

        // 自动触发转换
        adc_software_trigger_enable(adc_periph, ADC_ROUTINE_CHANNEL);

        // 超时防死锁机制 (ADC转换极快，正常只需等待几微秒)
        uint32_t timeout = 0x1FFFF;
        while(s_adc_ready[a_idx] == 0 && timeout > 0) {
            timeout--;
        }

        // 如果 timeout == 0，说明硬件出问题了没进中断，此处也可做错误报警
    }

    // 3. 返回缓冲区的最新值 (DMA 或刚被中断更新的值)
    if(adc_periph == ADC0) return adc0_buffer[index];
    if(adc_periph == ADC1) return adc1_buffer[index];
    if(adc_periph == ADC2) return adc2_buffer[index];

    return 0;
}
/* ======================================================== */
/* --- ADC 中断服务函数 --- */
/* ======================================================== */
/* ======================================================== */
/* --- 完全解耦的中断服务函数 --- */
/* ======================================================== */
void ADC_IRQHandler(void)
{
    // ADC0
    if(SET == adc_interrupt_flag_get(ADC0, ADC_INT_FLAG_EOC)) {
        adc_interrupt_flag_clear(ADC0, ADC_INT_FLAG_EOC);
        // 【注意】：这里是 routine 不是 regular
        adc0_buffer[0] = adc_routine_data_read(ADC0);
        s_adc_ready[0] = 1; // 置位完成标志
    }

    // ADC1
    if(SET == adc_interrupt_flag_get(ADC1, ADC_INT_FLAG_EOC)) {
        adc_interrupt_flag_clear(ADC1, ADC_INT_FLAG_EOC);
        adc1_buffer[0] = adc_routine_data_read(ADC1);
        s_adc_ready[1] = 1;
    }

    // ADC2
    if(SET == adc_interrupt_flag_get(ADC2, ADC_INT_FLAG_EOC)) {
        adc_interrupt_flag_clear(ADC2, ADC_INT_FLAG_EOC);
        adc2_buffer[0] = adc_routine_data_read(ADC2);
        s_adc_ready[2] = 1;
    }
}