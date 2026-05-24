#include "gd32f4xx_gpio.h"
#include "bsp_key.h"
#include "main.h"
#include "bsp_pmu.h"  // 1. 引入电源管理驱动
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gd30ad3344.h"

/* ========================================================================= */
/*                   二阶多项式标定参数（基于标准 PT100 拟合）                 */
/*                      公式: Temp = A * V^2 + B * V + C                      */
/* ========================================================================= */
#define CAL_POLY_A      21.72225942f
#define CAL_POLY_B      209.51587377f
#define CAL_POLY_C      -233.06876883f

#define CONVERT_NUM  (1024)
uint8_t convertarr[CONVERT_NUM] = {};

#define APP_START_OFFSET 0x10000

/* ===== 全局实例化 GD30AD3344 设备句柄 ===== */
GD30AD3344_HandleTypeDef gd30_dev;

/* ========================================================================= */
/*                       一维卡尔曼滤波器结构体与实体                          */
/* ========================================================================= */
typedef struct {
    float x;      // 滤波后的状态最优估计值 (温度)
    float p;      // 估计协方差
    float q;      // 过程噪声协方差
    float r;      // 测量噪声协方差
    uint8_t is_init; // 首次运行初始化标志
} Kalman_HandleTypeDef;

// 实例化全局温度卡尔曼滤波器
Kalman_HandleTypeDef t_kalman = {
    .x = 0.0f,
    .p = 1.0f,
    .q = 0.002f,
    .r = 0.35f,
    .is_init = 0
};

float Kalman_Filter(Kalman_HandleTypeDef *klm, float measurement) {
    if (!klm->is_init) {
        klm->x = measurement;
        klm->p = 1.0f;
        klm->is_init = 1;
        return measurement;
    }
    klm->p = klm->p + klm->q;
    float k_gain = klm->p / (klm->p + klm->r);
    klm->x = klm->x + k_gain * (measurement - klm->x);
    klm->p = (1.0f - k_gain) * klm->p;
    return klm->x;
}

/* ========================================================================= */
/*               新增: 动态零点校准全局变量 (消除电流源/温漂)                   */
/* ========================================================================= */
volatile float g_cal_temp_offset = 0.0f;
volatile float t_filtered_latest = 0.0f; // 用于给按键中断同步最新的滤波后温度

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
       buffer[3] == '_' && buffer[4] == 'O' && buffer[5] == 'F' && buffer[6] == 'F') {
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
    if (evt == KEY_EVENT_PRESS) {
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

/* ===== 新增: KEY6 动态零点校准回调函数 ===== */
void My_Key6_Handler(KEY_ID_t id, KEY_Event_t evt) {
    if (id != KEY_ID_6) return;
    if (evt == KEY_EVENT_PRESS) {
        // 捕获当前的滤波温度值，计算将其强制归零所需的偏差补偿量
        g_cal_temp_offset = 0.0f - t_filtered_latest;

        printf("\r\n[CAL-SYS] !!! KEY6 Pressed: Zero-Point Calibration Active !!!\r\n");
        printf("[CAL-SYS] Current Filtered Temp: %.2f C -> Calibrated to: 0.00 C\r\n", t_filtered_latest);
        printf("[CAL-SYS] New Dynamic Offset: %.4f C\r\n\r\n", g_cal_temp_offset);

        // OLED 界面闪烁提示校准成功
        OLED_NewFrame();
        OLED_PrintASCIIString(10, 0,  "Calibration", &afont16x8, OLED_COLOR_NORMAL);
        OLED_PrintASCIIString(10, 16, "Zero-Point OK!", &afont16x8, OLED_COLOR_NORMAL);
        OLED_ShowFrame();
        delay_1ms(300); // 稍微延时阻挡一下显示
    }
}

void My_Key_Global_Handler(KEY_ID_t id, KEY_Event_t evt) {
    My_Key_Event_Handler(id, evt);
    My_Key2_Handler(id, evt);
    My_Key5_Handler(id, evt);
    My_Key6_Handler(id, evt); // 将 Key6 挂载到全局分发中
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

    // RS485使能
    gpio_mode_set(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8);
    gpio_output_options_set(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8);
    gpio_bit_set(GPIOE, GPIO_PIN_8);
    printf("\r\n--- GD32F470 RS485 UART is ready ---\r\n");

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

    /* ===== 初始化 GD30AD3344 ===== */
    GD30AD3344_Init(&gd30_dev);
    GD30AD3344_SetConfig(&gd30_dev, GD30_MUX_AIN0_AIN3, GD30_PGA_2_048V, GD30_DR_100SPS, GD30_MODE_SINGLE_SHOT);
    printf("[SYS] GD30AD3344 Initialized. AIN3 ref enabled.\r\n");

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

    uint32_t gd30_ms = 0;
    while(1) {
        BSP_KEY_Scan();
        delay_1ms(10);
        gd30_ms += 10;

        /* 每 500ms 执行一次温度解算与数据上报 */
        if (gd30_ms >= 500) {
            gd30_ms = 0;

            // 1. 获取外置 ADC 原始电压
            int16_t raw_val = GD30AD3344_ReadData_SingleShot(&gd30_dev);
            float v_diff = (float)raw_val * (2.048f / 32768.0f);
            float v_ain0 = v_diff + 2.5f;

            // 2. 应用二阶多项式得到原始温度测量值
            float temp_raw = (CAL_POLY_A * v_ain0 * v_ain0) + (CAL_POLY_B * v_ain0) + CAL_POLY_C;

            // 3. 【新增：防脉冲软限幅】剔除 ADC 的突发型尖峰毛刺干扰
            static float s_last_valid_raw = 0.0f;
            static uint8_t s_first_run = 1;
            if (s_first_run) {
                s_last_valid_raw = temp_raw;
                s_first_run = 0;
            }

            // 如果单次跳变超过 1.0°C，且不属于手动切换电阻的大跳变(5.0°C)，则判定为坏点，进行限幅
            float delta = fabsf(temp_raw - s_last_valid_raw);
            if (delta > 1.0f && delta < 5.0f) {
                // 将坏点向历史值限幅拉回，不让尖峰破坏卡尔曼状态
                temp_raw = s_last_valid_raw + (temp_raw > s_last_valid_raw ? 0.1f : -0.1f);
            }
            s_last_valid_raw = temp_raw;

            // 4. 【加大卡尔曼滤波幅度】动态调整超参数 r
            // 直接动态重设 r 为 2.0f (大幅削弱 Raw 测量的权重，强制平滑)
            t_kalman.r = 2.0f;
            t_filtered_latest = Kalman_Filter(&t_kalman, temp_raw);

            // 当切换电阻档位导致温度发生巨大突变时(手动换挡)，重置卡尔曼滤波器
            if (delta > 5.0f) {
                t_kalman.is_init = 0;
                t_filtered_latest = Kalman_Filter(&t_kalman, temp_raw);
                s_last_valid_raw = temp_raw;
            }

            // 5. 应用由 Key6 实时校准生成的动态零点偏置
            float temp_final = t_filtered_latest + g_cal_temp_offset;

            // 6. 串口数据交互
            printf("[CAL-NEW] Voltage: %.4f V | Raw: %.2f C | Filtered: %.2f C | Final: %.2f C\r\n",
                    v_ain0, temp_raw, t_filtered_latest, temp_final);

            // 7. OLED 实时显示
            char str_v[20], str_t[20];
            snprintf(str_v, sizeof(str_v), "V: %.4f V", v_ain0);
            snprintf(str_t, sizeof(str_t), "T: %.2f C", temp_final);

            OLED_NewFrame();
            OLED_PrintASCIIString(10, 0,  "GD30 Strong LPF", &afont16x8, OLED_COLOR_NORMAL);
            OLED_PrintASCIIString(10, 16, str_v, &afont16x8, OLED_COLOR_NORMAL);
            OLED_PrintASCIIString(10, 32, str_t, &afont16x8, OLED_COLOR_NORMAL);
            OLED_ShowFrame();
        }
        // 原有的心跳灯逻辑
        static uint32_t hb_ms = 0;
        hb_ms += 10;
        if (hb_ms >= 1000) {
            LED6.Toggle();
            hb_ms = 0;
        }
    }
}