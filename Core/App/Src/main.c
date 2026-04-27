#include "gd32f4xx_gpio.h"
#include "bsp_key.h"
#include "main.h"
#include "bsp_pmu.h"  // 1. 引入电源管理驱动
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CONVERT_NUM  (1024)
uint8_t convertarr[CONVERT_NUM] = {};

/* 新增：App 向量表偏移量 (0x08010000 距离 0x08000000 的偏移量) */
#define APP_START_OFFSET 0x10000

/* --- 原有 Uart 处理函数保持不变 --- */
void My_Uart_Frame_Handler(uint8_t* buffer, uint16_t length) {
    printf("\r\n[UART_RX] Received %d Bytes. Content: ", length);
    for(int i = 0; i < length; i++) {
       DebugUART.SendByte(buffer[i]);
    }
    printf("\r\n");

    if (length >= 6 &&
       buffer[0] == 'L' && buffer[1] == 'E' && buffer[2] == 'D' &&
       buffer[3] == '_' && buffer[4] == 'O' && buffer[5] == 'N') {
       LED1.On();
       printf(">> Command Executed: LED1 is ON!\r\n");
    }
    if (length >= 7 &&
       buffer[0] == 'L' && buffer[1] == 'E' && buffer[2] == 'D' &&
       buffer[3] == '_' && buffer[4] == 'O' && buffer[5] == 'F'&&buffer[6] == 'F') {
       LED1.Off();
       printf(">> Command Executed: LED1 is OFF!\r\n");
    }
    if (strncmp((const char*)buffer, "RTC ", 4) == 0) {
       uart_parse_and_set_rtc((const char*)buffer);
    }
}

/* --- 按键回调函数组 --- */

// Key1 处理
void My_Key_Event_Handler(KEY_ID_t id, KEY_Event_t evt) {
    if (id != KEY_ID_1) return;
    if (evt == KEY_EVENT_PRESS) {
       LED1.On();
       printf("[KEY] KEY1 PRESS -> LED1 ON\r\n");
    } else if (evt == KEY_EVENT_LONG) {
       LED1.Off();
       printf("[KEY] KEY1 LONG -> LED1 OFF\r\n");
    }
}

// Key2 处理
static uint32_t key2_count = 0;
void My_Key2_Handler(KEY_ID_t id, KEY_Event_t evt) {
    if (id != KEY_ID_2) return;
    if (evt == KEY_EVENT_PRESS) {
       key2_count++;
       char buf[32];
       snprintf(buf, sizeof(buf), "%lu", (unsigned long)key2_count);

       OLED_NewFrame();
       OLED_PrintASCIIString(10, 0, "GD32F4", &afont16x8, OLED_COLOR_NORMAL);
       OLED_PrintASCIIString(10, 16, buf, &afont16x8, OLED_COLOR_NORMAL);
       OLED_ShowFrame();
       printf("[KEY] KEY2 PRESS -> count=%lu\r\n", (unsigned long)key2_count);
    }
}

// 2. 新增：Key5 处理函数 (进入休眠)
void My_Key5_Handler(KEY_ID_t id, KEY_Event_t evt) {
    if (id != KEY_ID_5) return;

    if (evt == KEY_EVENT_PRESS)
    {
        printf("[SYS] Key5 Pressed! Entering Deep-Sleep Mode...\r\n");

        OLED_NewFrame();
        OLED_PrintASCIIString(10, 0, "System", &afont16x8, OLED_COLOR_NORMAL);
        OLED_PrintASCIIString(10, 16, "Sleeping...", &afont16x8, OLED_COLOR_NORMAL);
        OLED_ShowFrame();

        // 等待串口数据发送完毕
        delay_1ms(100);

        /* ==== 调用深度睡眠 ==== */
        bsp_pmu_enter_deepsleep();

        /* ==== 醒来后的代码 ==== */
        printf("\r\n[SYS] Waked up from Deep-Sleep!\r\n");

        // 醒来后重新点亮屏幕或做其他恢复操作
        OLED_NewFrame();
        OLED_PrintASCIIString(10, 0, "System", &afont16x8, OLED_COLOR_NORMAL);
        OLED_PrintASCIIString(10, 16, "Waked up!", &afont16x8, OLED_COLOR_NORMAL);
        OLED_ShowFrame();
    }
}

/* 全局按键分发器 */
void My_Key_Global_Handler(KEY_ID_t id, KEY_Event_t evt) {
    My_Key_Event_Handler(id, evt);
    My_Key2_Handler(id, evt);
    My_Key5_Handler(id, evt); // 3. 注册 Key5 到分发器
}


int main(void)
{
    /* ==================== Bootloader 适配核心代码 ==================== */
    /* 1. 重定向中断向量表 (极其重要，否则一旦触发中断直接跑飞) */
    nvic_vector_table_set(NVIC_VECTTAB_FLASH, APP_START_OFFSET);

    /* 2. 重新开启全局中断 (因为 Bootloader 跳转前把它关了) */
    __enable_irq();
    /* ================================================================= */
    
    systick_config();

    rcu_periph_clock_enable(RCU_PMU);
    pmu_flag_clear(PMU_FLAG_WAKEUP);
    pmu_flag_clear(PMU_FLAG_STANDBY);

    /* 硬件初始化 */
    BSP_LED_InitAll();
    BSP_KEY_InitAll();

    bsp_pmu_exti_init();

    DebugUART.Init(115200);
    DebugUART.RegisterRxFrameCallback(My_Uart_Frame_Handler);
    BSP_KEY_RegisterCallback(My_Key_Global_Handler);

    printf("\r\n========================================\r\n");
    printf("=   GD32F470 System Boot Successful!   =\r\n");
    printf("=   System Clock: %ld Hz         =\r\n", rcu_clock_freq_get(CK_SYS));
    OLED_PrintASCIIString(10, 16, "READY", &afont16x8, OLED_COLOR_NORMAL);
    printf("========================================\r\n");

    OLED_Init(I2C0, GPIOB, GPIO_PIN_8, GPIOB, GPIO_PIN_9);
    OLED_NewFrame();
    OLED_PrintASCIIString(10, 0, "GD32F4", &afont16x8, OLED_COLOR_NORMAL);

    OLED_PrintASCIIString(10, 16, "READY", &afont16x8, OLED_COLOR_NORMAL);
    OLED_ShowFrame();

    bsp_rtc_init();
    bsp_rtc_get_time();
    BSP_LED_SystemTest();

    bsp_adc_Start_Init(ADC0, GPIOC, GPIO_PIN_0, ADC_TRANS_MODE_IT);
    bsp_adc_Start_Init(ADC1, GPIOC, GPIO_PIN_2, ADC_TRANS_MODE_IT);

    /* DAC 波形生成逻辑保持不变 */
#define PI  3.14159265358979f
    for (int i = 0; i < CONVERT_NUM; i++) {
        float radians = 2.0f * PI * (float)i / (float)CONVERT_NUM;
        convertarr[i] = (uint8_t)(fabsf(sinf(radians)) * 255.0f);
    }
    GD32_dac_bsp_Start(GPIOA, GPIO_PIN_4, DAC_DMA, convertarr, CONVERT_NUM);
    GD32_DAC_TIM5_Base(DAC0, 5000);

    uint16_t adc0_val = bsp_get_adc_value(ADC0, 0);
    uint16_t adc1_val = bsp_get_adc_value(ADC1, 0);
    printf("ADC0 Voltage = %.2f V | ADC1 Voltage = %.2f V\r\n",
            adc0_val * 3.3 / 4096.0, adc1_val * 3.3 / 4096.0);

    uint32_t hb_ms = 0;
    while(1) {
        BSP_KEY_Scan();
        delay_1ms(10);
        hb_ms += 10;

        if (hb_ms >= 1000) {
            LED6.Toggle();
            hb_ms = 0;
        }
    }
}