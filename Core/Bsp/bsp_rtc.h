#ifndef __BSP_RTC_H
#define __BSP_RTC_H

#include "gd32f4xx.h"

/* 宏定义：选择 RTC 时钟源，默认使用 LXTAL (外部低速晶振) */
#define RTC_CLOCK_SOURCE_LXTAL
#define BKP_VALUE    0x32F0

/* 外部声明，方便其他地方获取时间 */
extern rtc_parameter_struct rtc_initpara;

/* 核心 API */
void bsp_rtc_init(void);
void bsp_rtc_set_wakeup(uint32_t seconds);
void bsp_rtc_get_time(void);
uint8_t bsp_rtc_set_time(uint8_t year, uint8_t month, uint8_t date, uint8_t day_of_week, uint8_t hour, uint8_t minute, uint8_t second);
void uart_parse_and_set_rtc(const char *rx_buffer);

#endif /* __BSP_RTC_H */