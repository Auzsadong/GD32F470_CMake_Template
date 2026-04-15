

#include "gd32f4xx_gpio.h"
#include "main.h"

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


	/* 2. 炫酷的 6 灯流水自检 */
	BSP_LED_SystemTest();
	float text=99.8f;
	while(1) {

		LED6.Toggle(); // 心跳灯
		delay_1ms(1000);
	}
}


/****************************End*****************************/
