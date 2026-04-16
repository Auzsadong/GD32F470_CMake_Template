

#include "gd32f4xx_gpio.h"
#include "main.h"

#include "bsp_rtc.h"

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

	bsp_adc_Start_Init(ADC0, GPIOC, GPIO_PIN_0, ADC_TRANS_MODE_DMA);
	bsp_adc_Start_Init(ADC1, GPIOC, GPIO_PIN_2, ADC_TRANS_MODE_DMA);
	//bsp_adc_Start_Two_Init(ADC2, GPIOA, GPIO_PIN_0, GPIOA, GPIO_PIN_1, ADC_TRANS_MODE_DMA);单ADC复用
	while(1) {
		uint16_t adc_val = bsp_get_adc_value(ADC0, 0);
		uint16_t pc2_val = bsp_get_adc_value(ADC1, 0);
		printf("ADC Value on PC0 = %.2f\n", adc_val*3.3/4096.0);
		printf("ADC Value on PC2 = %.2f\n", pc2_val*3.3/4096.0);
		LED6.Toggle(); // 心跳灯
		delay_1ms(1000);
	}
}


/****************************End*****************************/
