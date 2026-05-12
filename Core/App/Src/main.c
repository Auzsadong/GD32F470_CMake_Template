#include "gd32f4xx_gpio.h"
#include "bsp_key.h"
#include "main.h"
#include "bsp_pmu.h"  // 1. 引入电源管理驱动
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gd30ad3344.h"

#define CONVERT_NUM  (1024)
uint8_t convertarr[CONVERT_NUM] = {};

#define APP_START_OFFSET 0x10000

/* ===== 新增: 全局实例化 GD30AD3344 设备句柄 ===== */
GD30AD3344_HandleTypeDef gd30_dev;

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

void My_Key5_Handler(KEY_ID_t id, KEY_Event_t evt) {
    if (id != KEY_ID_5) return;

    if (evt == KEY_EVENT_PRESS)
    {
        printf("[SYS] Key5 Pressed! Entering Deep-Sleep Mode...\r\n");

        OLED_NewFrame();
        OLED_PrintASCIIString(10, 0, "System", &afont16x8, OLED_COLOR_NORMAL);
        OLED_PrintASCIIString(10, 16, "Sleeping...", &afont16x8, OLED_COLOR_NORMAL);
        OLED_ShowFrame();

        delay_1ms(100);

        bsp_pmu_enter_deepsleep();

        printf("\r\n[SYS] Waked up from Deep-Sleep!\r\n");

        OLED_NewFrame();
        OLED_PrintASCIIString(10, 0, "System", &afont16x8, OLED_COLOR_NORMAL);
        OLED_PrintASCIIString(10, 16, "Waked up!", &afont16x8, OLED_COLOR_NORMAL);
        OLED_ShowFrame();
    }
}

void My_Key_Global_Handler(KEY_ID_t id, KEY_Event_t evt) {
    My_Key_Event_Handler(id, evt);
    My_Key2_Handler(id, evt);
    My_Key5_Handler(id, evt);
}

int main(void)
{
    /* 1. 重定向中断向量表 */
    nvic_vector_table_set(NVIC_VECTTAB_FLASH, APP_START_OFFSET);

    /* 2. 重新开启全局中断 */
    __enable_irq();

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

    /* ===== 新增: 初始化 GD30AD3344 ===== */
    GD30AD3344_Init(&gd30_dev);
    /* 确认配置：AIN0相对AIN3(外部2.5V参考)，量程±2.048V，100SPS，单次触发 */
    GD30AD3344_SetConfig(&gd30_dev, GD30_MUX_AIN0_AIN3, GD30_PGA_2_048V, GD30_DR_100SPS, GD30_MODE_SINGLE_SHOT);
    printf("[SYS] GD30AD3344 Initialized. AIN3 ref enabled.\r\n");
    /* ==================================== */

    /* DAC 波形生成逻辑 */
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
    uint32_t gd30_ms = 0; // ===== 新增: GD30 定时器变量 =====

    while(1) {
        BSP_KEY_Scan();
        delay_1ms(10);
        hb_ms += 10;
        gd30_ms += 10; // ===== 新增: 累加时间 =====

        /* ===== 新增: 每 500ms 读取一次外部 ADC ===== */
        if (gd30_ms >= 500) {
            gd30_ms = 0;

            // 1. 读取原始数据 (16位带符号整数，满量程 7FFFh)
            int16_t raw_val = GD30AD3344_ReadData_SingleShot(&gd30_dev);

            // 2. 将原始代码转换为差分电压差 (PGA = ±2.048V)
            // LSB 大小 = 2.048V / 32768 = 62.5uV
            float v_diff = (float)raw_val * (2.048f / 32768.0f);

            // 3. 计算绝对电压：V_AIN0 = V_diff + V_AIN3 (2.5V 外部参考)
            float v_ain0 = v_diff + 2.5f;

            // 4. 串口打印监视
            printf("[ADC] GD30 Raw: %6d | V_diff: %7.4f V | AIN0: %7.4f V\r\n", raw_val, v_diff, v_ain0);

            // 5. 显示在 OLED 上
            char oled_buf[32];
            snprintf(oled_buf, sizeof(oled_buf), "AIN0: %.3f V", v_ain0);
            OLED_NewFrame();
            OLED_PrintASCIIString(10, 0, "GD30AD3344", &afont16x8, OLED_COLOR_NORMAL);
            OLED_PrintASCIIString(10, 16, oled_buf, &afont16x8, OLED_COLOR_NORMAL);
            OLED_ShowFrame();
        }
        /* ================================================== */

        if (hb_ms >= 1000) {
            LED6.Toggle();
            hb_ms = 0;
        }
    }
}