

#include "gd32f4xx_gpio.h"
#include "main.h"

void My_Uart_Rx_Handler(uint8_t data) {
	// 收到什么就发回什么 (回显)
	DebugUART.SendByte(data);

	// 如果收到字符 'A'，就点亮 LED1
	if(data == 'A') {
		LED1.On();
		printf("\r\n[SYSTEM] LED1 is turned ON by UART Command!\r\n");
	}
}

int main(void)
{
	systick_config();

	/* 1. 一键初始化所有 LED */
	BSP_LED_InitAll();

	DebugUART.Init(115200);
	// 将我们写的处理函数挂载到串口接收上！彻底告别在 it.c 里写业务代码！
	DebugUART.RegisterRxCallback(My_Uart_Rx_Handler);

	/* 2. 炫酷开机自检与打印 */
	printf("\r\n========================================\r\n");
	printf("=   GD32F470 System Boot Successful!   =\r\n");
	printf("=   System Clock: %ld Hz         =\r\n", rcu_clock_freq_get(CK_SYS));
	printf("========================================\r\n");


	/* 2. 炫酷的 6 灯流水自检 */
	BSP_LED_SystemTest();

	while(1) {
		LED6.Toggle(); // 心跳灯
		delay_1ms(1000);
	}
}


/****************************End*****************************/
