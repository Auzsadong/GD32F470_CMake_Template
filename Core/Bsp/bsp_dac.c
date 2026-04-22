#include "bsp_dac.h"

/* --- 内部静态变量，用于中断模式下保存数据状态 --- */
static const uint8_t *s_dac_data = 0;
static uint32_t s_dac_len = 0;
static uint32_t s_dac_index = 0;
static uint32_t s_dac_current_pin = 0; // 记录当前使用的是PA4还是PA5

/* 内部辅助函数声明 */
static void _dac_timer5_config(uint8_t enable_interrupt);
static void _dac_dma_config(uint32_t dac_addr, const uint8_t *data, uint32_t len);

void GD32_dac_bsp_Start(uint32_t gpio_periph, uint32_t pin, DAC_TransferMode_Enum mode, const uint8_t *data, uint32_t len)
{
    uint32_t dac_periph = DAC0;
    uint32_t dac_out = DAC_OUT0;
    uint32_t dac_addr = 0x40007410; // 默认 DAC0_OUT0_R8DH

    /* 保存引脚信息，供中断里判断写哪个寄存器 */
    s_dac_current_pin = pin;

    /* 1. 开启时钟 */
    rcu_periph_clock_enable(RCU_DAC);
    if (gpio_periph == GPIOA) {
        rcu_periph_clock_enable(RCU_GPIOA);
    }

    /* 2. 配置 GPIO 为模拟引脚 */
    gpio_mode_set(gpio_periph, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, pin);

    /* 3. 映射 DAC 通道 */
    if (pin == GPIO_PIN_4) {
        dac_out = DAC_OUT0;
        dac_addr = 0x40007410;
    } else if (pin == GPIO_PIN_5) {
        dac_out = DAC_OUT1;
        dac_addr = 0x4000741C;
    }

    /* 4. 初始化 DAC 基本配置 */
    dac_deinit(dac_periph);
    dac_wave_mode_config(dac_periph, dac_out, DAC_WAVE_DISABLE);

    /* 5. 核心模式分支配置 */
    if (mode == DAC_DMA) {
        /* DMA 模式：定时器触发，DMA搬运 */
        rcu_periph_clock_enable(RCU_DMA0);
        dac_trigger_source_config(dac_periph, dac_out, DAC_TRIGGER_T5_TRGO);
        _dac_timer5_config(0); // 开启定时器，不开启中断
        _dac_dma_config(dac_addr, data, len);
        dac_dma_enable(dac_periph, dac_out);
        dac_trigger_enable(dac_periph, dac_out);

    } else if (mode == DAC_IT) {
        /* 普通中断模式：保存数据指针，定时器触发，CPU进中断搬运 */
        s_dac_data = data;
        s_dac_len = len;
        s_dac_index = 0;

        dac_trigger_source_config(dac_periph, dac_out, DAC_TRIGGER_T5_TRGO);
        dac_trigger_enable(dac_periph, dac_out);
        _dac_timer5_config(1); // 开启定时器，并开启更新中断

    } else if (mode == DAC_CPU) {
        /* 纯软件模式：无需定时器，由用户随时调函数写入 */
        dac_trigger_source_config(dac_periph, dac_out, DAC_TRIGGER_SOFTWARE);
        dac_trigger_enable(dac_periph, dac_out);
    }

    /* 6. 使能 DAC */
    dac_enable(dac_periph, dac_out);
}

void GD32_dac_bsp_SetData(uint32_t pin, uint8_t value)
{
    if (pin == GPIO_PIN_4) {
        /* 使用标准库 API 写入 8位右对齐 数据 */
        dac_data_set(DAC0, DAC_OUT0, DAC_ALIGN_8B_R, value);
        dac_software_trigger_enable(DAC0, DAC_OUT0);
    } else if (pin == GPIO_PIN_5) {
        dac_data_set(DAC0, DAC_OUT1, DAC_ALIGN_8B_R, value);
        dac_software_trigger_enable(DAC0, DAC_OUT1);
    }
}

/* --- 内部静态辅助函数 --- */

static void _dac_timer5_config(uint8_t enable_interrupt)
{
    timer_parameter_struct timer_initpara;
    rcu_periph_clock_enable(RCU_TIMER5);
    timer_deinit(TIMER5);

    timer_struct_para_init(&timer_initpara);
    timer_initpara.prescaler         = 99;
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = 999;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;

    timer_init(TIMER5, &timer_initpara);
    timer_master_output_trigger_source_select(TIMER5, TIMER_TRI_OUT_SRC_UPDATE);

    if (enable_interrupt) {
        nvic_irq_enable(TIMER5_DAC_IRQn, 1, 0); // 配置中断优先级
        timer_interrupt_enable(TIMER5, TIMER_INT_UP); // 开启更新中断
    }

    timer_enable(TIMER5);
}

static void _dac_dma_config(uint32_t dac_addr, const uint8_t *data, uint32_t len)
{
    dma_single_data_parameter_struct dma_struct;
    dma_flag_clear(DMA0, DMA_CH5, DMA_INTF_FEEIF | DMA_INTF_SDEIF | DMA_INTF_TAEIF | DMA_INTF_HTFIF | DMA_INTF_FTFIF);

    dma_channel_subperipheral_select(DMA0, DMA_CH5, DMA_SUBPERI7);
    dma_struct.periph_addr         = dac_addr;
    dma_struct.memory0_addr        = (uint32_t)data;
    dma_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_struct.number              = len;
    dma_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_struct.priority            = DMA_PRIORITY_ULTRA_HIGH;
    dma_struct.circular_mode       = DMA_CIRCULAR_MODE_ENABLE;

    dma_single_data_mode_init(DMA0, DMA_CH5, &dma_struct);
    dma_channel_enable(DMA0, DMA_CH5);
}

/* --- 中断服务函数 (接管CPU叫回操作) --- */
void TIMER5_DAC_IRQHandler(void)
{
    /* 检查是否是 TIMER5 的更新中断 */
    if (timer_interrupt_flag_get(TIMER5, TIMER_INT_FLAG_UP) != RESET) {
        timer_interrupt_flag_clear(TIMER5, TIMER_INT_FLAG_UP);

        /* 防御性编程：确保指针非空且长度有效 */
        if (s_dac_data != 0 && s_dac_len > 0) {

            /* 将数据写入对应通道 (使用标准库API替换直接操作寄存器) */
            if (s_dac_current_pin == GPIO_PIN_4) {
                dac_data_set(DAC0, DAC_OUT0, DAC_ALIGN_8B_R, s_dac_data[s_dac_index]);
            } else if (s_dac_current_pin == GPIO_PIN_5) {
                dac_data_set(DAC0, DAC_OUT1, DAC_ALIGN_8B_R, s_dac_data[s_dac_index]);
            }

            /* 游标自增并循环 */
            s_dac_index++;
            if (s_dac_index >= s_dac_len) {
                s_dac_index = 0;
            }
        }
    }
}