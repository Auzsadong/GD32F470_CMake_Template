

#include "gd32f4xx_gpio.h"
#include "main.h"



/************************ ִ�к��� ************************/

int main(void)
{
	systick_config();

	/* 1. 一键初始化所有 LED */
	BSP_LED_InitAll();

	/* 2. 炫酷的 6 灯流水自检 */
	BSP_LED_SystemTest();

	while(1) {
		LED1.Toggle();
		LED6.Toggle(); // 比如让首尾两个灯交替闪烁
		delay_1ms(500);
	}
}


/****************************End*****************************/
