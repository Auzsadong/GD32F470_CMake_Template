#include "bsp_rtc.h"
#include <stdio.h>

rtc_parameter_struct rtc_initpara;
static __IO uint32_t prescaler_a = 0, prescaler_s = 0;

/*!
    \brief      RTC 时钟预配置 (内部调用)
*/
static void bsp_rtc_pre_config(void)
{
    #if defined (RTC_CLOCK_SOURCE_IRC32K)
        rcu_osci_on(RCU_IRC32K);
        rcu_osci_stab_wait(RCU_IRC32K);
        rcu_rtc_clock_config(RCU_RTCSRC_IRC32K);
        prescaler_s = 0x13F;
        prescaler_a = 0x63;
    #elif defined (RTC_CLOCK_SOURCE_LXTAL)
        rcu_osci_on(RCU_LXTAL);
        rcu_osci_stab_wait(RCU_LXTAL);
        rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);
        prescaler_s = 0xFF;
        prescaler_a = 0x7F;
    #endif

    rcu_periph_clock_enable(RCU_RTC);
    rtc_register_sync_wait();
}

/*!
    \brief      设置默认时间 (替代原厂的串口阻塞输入)
*/
static void bsp_rtc_setup_default(void)
{
    rtc_initpara.factor_asyn = prescaler_a;
    rtc_initpara.factor_syn = prescaler_s;
    rtc_initpara.year = 0x24;            // 2024年 (示例)
    rtc_initpara.month = RTC_JAN;
    rtc_initpara.date = 0x01;
    rtc_initpara.day_of_week = RTC_MONDAY;
    rtc_initpara.display_format = RTC_24HOUR;
    rtc_initpara.am_pm = RTC_AM;
    rtc_initpara.hour = 0x12;
    rtc_initpara.minute = 0x00;
    rtc_initpara.second = 0x00;

    if(ERROR != rtc_init(&rtc_initpara)){
        RTC_BKP0 = BKP_VALUE;
    }
}

/*!
    \brief      BSP RTC 初始化接口
    \note       在系统初始化阶段调用一次即可
*/
void bsp_rtc_init(void)
{
    uint32_t rtcsrc_flag = 0;

    /* 1. 使能 PMU 时钟并开启备份域写权限 */
    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    /* 2. 时钟预配置 */
    bsp_rtc_pre_config();

    /* 3. 检查是否是第一次配置 (掉电或首次烧录) */
    rtcsrc_flag = GET_BITS(RCU_BDCTL, 8, 9);
    if ((BKP_VALUE != RTC_BKP0) || (0x00 == rtcsrc_flag)){
        bsp_rtc_setup_default(); // 写入默认时间
    }

    rcu_all_reset_flag_clear();
}

/*!
    \brief      设定指定秒数后的单次唤醒
    \param[in]  seconds: 多少秒后唤醒
    \note       如果你不需要周期唤醒，需在中断服务函数中将其关闭
*/
void bsp_rtc_set_wakeup(uint32_t seconds)
{
    /* 如果传入0，直接关闭唤醒功能 */
    if (seconds == 0) {
        rtc_wakeup_disable();
        return;
    }

    /* 配置 EXTI 22 线 (RTC 唤醒专用) */
    exti_flag_clear(EXTI_22);
    exti_init(EXTI_22, EXTI_INTERRUPT, EXTI_TRIG_RISING);
    nvic_irq_enable(RTC_WKUP_IRQn, 0, 0);

    /* 配置前先关闭并清除标志位 */
    rtc_wakeup_disable();
    rtc_flag_clear(RTC_FLAG_WT);

    /* 使用 ck_spre 时钟 (默认1Hz) */
    rtc_wakeup_clock_set(WAKEUP_CKSPRE);

    /* 设置重装载值。官方手册规定，实际周期 = value + 1 */
    rtc_wakeup_timer_set(seconds - 1);

    /* 使能中断和定时器 */
    rtc_interrupt_enable(RTC_INT_WAKEUP);
    rtc_wakeup_enable();
}
/*!
    \brief      校正/设置 RTC 当前时间
    \param[in]  year: 年 (仅填后两位，例如 2026年 填 0x26)
    \param[in]  month: 月 (RTC_JAN 到 RTC_DEC)
    \param[in]  date: 日 (0x01 - 0x31)
    \param[in]  day_of_week: 星期 (RTC_MONDAY 到 RTC_SUNDAY)
    \param[in]  hour: 时 (0x00 - 0x23，24小时制)
    \param[in]  minute: 分 (0x00 - 0x59)
    \param[in]  second: 秒 (0x00 - 0x59)
    \retval     SUCCESS / ERROR
    \note       注意：所有数字参数必须使用 BCD 码格式（即 16进制的数字直接对应10进制的视觉表现）
*/

uint8_t bsp_rtc_set_time(uint8_t year, uint8_t month, uint8_t date, uint8_t day_of_week, uint8_t hour, uint8_t minute, uint8_t second)
{
    /* 保持原有的分频器设置 */
    rtc_initpara.factor_asyn = 0x7F; // 如果使用 LXTAL 默认 0x7F
    rtc_initpara.factor_syn  = 0xFF; // 如果使用 LXTAL 默认 0xFF

    /* 设置日期和时间参数 */
    rtc_initpara.year = year;
    rtc_initpara.month = month;
    rtc_initpara.date = date;
    rtc_initpara.day_of_week = day_of_week;

    rtc_initpara.display_format = RTC_24HOUR;
    rtc_initpara.am_pm = RTC_AM;

    rtc_initpara.hour = hour;
    rtc_initpara.minute = minute;
    rtc_initpara.second = second;

    /* 调用库函数重新初始化 RTC 时间 */
    if(ERROR == rtc_init(&rtc_initpara)){
        return ERROR;
    }else{
        /* 写入备份寄存器，标记时间已被成功配置 */
        RTC_BKP0 = BKP_VALUE;
        return SUCCESS;
    }
}
/* 十进制转 BCD 码宏定义 */
#define DEC2BCD(val)  ((((val) / 10) << 4) | ((val) % 10))

/*!
    \brief      根据日期计算星期 (基姆拉尔森公式)
    \param[in]  year: 完整年份 (如 2026)
    \param[in]  month: 月份 (1-12)
    \param[in]  day: 日期 (1-31)
    \retval     星期 (1代表星期一 ... 7代表星期日)
*/
 uint8_t calculate_week_day(uint16_t year, uint8_t month, uint8_t day)
{
    int week = 0;

    /* 在这个公式中，把一月和二月看成是上一年的十三月和十四月 */
    if (month == 1 || month == 2) {
        month += 12;
        year--;
    }

    week = (day + 2 * month + 3 * (month + 1) / 5 + year + year / 4 - year / 100 + year / 400 + 1) % 7;

    /* 转换成 GD32 RTC 的标准: 1-7 分别对应 周一到周日 */
    if (week == 0) {
        week = 7;
    }
    return week;
}
/*!
    \brief      解析串口时间字符串并校准 RTC
    \param[in]  rx_buffer: 接收到的字符串指针，格式应为 "RTC 2026 04 15 21 15"
    \retval     none
*/
void uart_parse_and_set_rtc(const char *rx_buffer)
{
    int year, month, day, hour, minute;

    /* 1. 使用 sscanf 按照指定格式提取十进制数据 */
    /* 返回值为成功匹配并赋值的变量个数，必须等于 5 才算格式正确 */
    if (sscanf(rx_buffer, "RTC %d %d %d %d %d", &year, &month, &day, &hour, &minute) == 5) {

        /* 2. 数据合法性简单校验 (防止输入乱码导致崩溃) */
        if(year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 ||
           hour > 23 || minute > 59) {
            printf("Error: Date or Time out of range!\r\n");
            return;
           }

        /* 3. 获取两位数年份 (2026 -> 26) 和 星期 */
        uint8_t short_year = year % 100;
        uint8_t week = calculate_week_day(year, month, day);

        /* 4. 将所有十进制数据转换为 BCD 码 */
        uint8_t bcd_year   = DEC2BCD(short_year);
        uint8_t bcd_month  = DEC2BCD(month);
        uint8_t bcd_day    = DEC2BCD(day);
        uint8_t bcd_hour   = DEC2BCD(hour);
        uint8_t bcd_minute = DEC2BCD(minute);
        uint8_t bcd_second = 0x00;  // 协议未包含秒，默认从 00 秒开始

        /* 5. 调用之前写好的 bsp_rtc_set_time 写入底层寄存器 */
        if (SUCCESS == bsp_rtc_set_time(bcd_year, bcd_month, bcd_day, week, bcd_hour, bcd_minute, bcd_second)) {
            /* 打印校准成功的信息，验证 BCD 数据 */
            printf("RTC Calibrated: 20%02x-%02x-%02x %02x:%02x:%02x, Week: %d\r\n",
                   bcd_year, bcd_month, bcd_day, bcd_hour, bcd_minute, bcd_second, week);
        } else {
            printf("Error: RTC Hardware update failed!\r\n");
        }

    } else {
        /* 如果字符串前缀不是 RTC 或者数字不够 */
        printf("Error: Invalid Format! Please use: RTC YYYY MM DD HH MM\r\n");
    }
}
/*!
    \brief      获取并打印当前时间 (调试用)
*/
void bsp_rtc_get_time(void)
{
    rtc_current_time_get(&rtc_initpara);
    printf("Time: %0.2x:%0.2x:%0.2x \n\r", rtc_initpara.hour, rtc_initpara.minute, rtc_initpara.second);
}