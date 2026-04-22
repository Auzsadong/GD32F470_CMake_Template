#include "gd32f4xx_gpio.h"
#include "main.h"
#include <math.h>


void My_Uart_Frame_Handler(uint8_t* buffer, uint16_t length) {
	// 打印收到了多少个字节
	printf("\r\n[UART_RX] Received %d Bytes. Content: ", length);

	// 把收到的整帧数据原样回传 (回显)
	for(int i = 0; i < length; i++) {
		DebugUART.SendByte(buffer[i]);
	}
	printf("\r\n");

	// 简易指令解析演示：如果收到 "LED_ON"
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
	// 如果字符串是以 "RTC " 开头的
	if (strncmp((const char*)buffer, "RTC ", 4) == 0) {
		uart_parse_and_set_rtc((const char*)buffer);
	}

}
#define CONVERT_NUM  (1024)
 uint8_t convertarr[CONVERT_NUM] = {};
int main(void)
{
	systick_config();

	/* 1. 一键初始化所有 LED */
	BSP_LED_InitAll();

	DebugUART.Init(115200);
	DebugUART.RegisterRxFrameCallback(My_Uart_Frame_Handler);

	/* 2. 炫酷开机自检与打印 */
	printf("\r\n========================================\r\n");
	printf("=   GD32F470 System Boot Successful!   =\r\n");
	printf("=   System Clock: %ld Hz         =\r\n", rcu_clock_freq_get(CK_SYS));
	printf("========================================\r\n");



	/* 2. 初始化 RTC 模块（自动识别是否需要冷启动配置） */
	bsp_rtc_init();

	bsp_rtc_get_time();
	/* 2. 炫酷的 6 灯流水自检 */
	BSP_LED_SystemTest();

	bsp_adc_Start_Init(ADC0, GPIOC, GPIO_PIN_0, ADC_TRANS_MODE_IT);
	bsp_adc_Start_Init(ADC1, GPIOC, GPIO_PIN_2, ADC_TRANS_MODE_IT);
	//bsp_adc_Start_Two_Init(ADC2, GPIOA, GPIO_PIN_0, GPIOA, GPIO_PIN_1, ADC_TRANS_MODE_DMA);单ADC复用

	/* 引入 ARM 数学库中的 PI 宏 (PI = 3.14159265358979f) */
#define PI  3.14159265358979f


	for (int i = 0; i < CONVERT_NUM; i++)
	{
		/* 1. 计算原始弧度 (0 到 2π) */
		float radians = 2.0f * PI * (float)i / (float)CONVERT_NUM;

		/* 2. 求单精度正弦值 */
		float sin_val = sinf(radians);

		/* 3. 核心步骤：负半轴翻转 (取绝对值) */
		float abs_sin_val = fabsf(sin_val);

		/* 4. 映射到 0~255 的 8位 DAC 范围
		 * 此时 abs_sin_val 的范围已经是 0.0 ~ 1.0，直接乘 255 即可
		 */
		convertarr[i] = (uint8_t)(abs_sin_val * 255.0f);
	}

	/* 第一步：启动普通 CPU 中断模式，底层会自动记录数组长度为 10 */
	GD32_dac_bsp_Start(GPIOA, GPIO_PIN_4, DAC_DMA, convertarr, CONVERT_NUM);

	/* 第二步：优雅地设定频率，比如我们想输出一个 120.5Hz 的波形 */
	GD32_DAC_TIM5_Base(DAC0, 5000);
	while(1) {
		uint16_t adc0_val = bsp_get_adc_value(ADC0, 0);
		uint16_t adc1_val = bsp_get_adc_value(ADC1, 0);

		// 打印电压值 (假设基准电压是 3.3V，12位精度4096)
		printf("ADC0 (PC0) Voltage = %.2f V\r\n", adc0_val * 3.3 / 4096.0);
		printf("ADC1 (PC2) Voltage = %.2f V\r\n", adc1_val * 3.3 / 4096.0);
		LED6.Toggle(); // 心跳灯
		delay_1ms(1000);
	}
}


/****************************End*****************************/
