#ifndef BSP_KEY_H
#define BSP_KEY_H

#include "gd32f4xx.h"

/*
 * Board-level key driver
 *
 * - Supports up to 6 keys
 * - Debounce and short/long press detection
 * - Callbacks for key events
 *
 * Configuration:
 * Edit the KEYx_RCU / KEYx_PORT / KEYx_PIN macros below to match your board wiring.
 */

/* ================== 用户可配置的按键硬件映射 ================== */
// 按键映射更新：Key1..Key5 -> PB3..PB7, Key6 -> PD6
// 请确认板上引脚与下列宏一致
#define KEY1_RCU   RCU_GPIOB
#define KEY1_PORT  GPIOB
#define KEY1_PIN   GPIO_PIN_3

#define KEY2_RCU   RCU_GPIOB
#define KEY2_PORT  GPIOB
#define KEY2_PIN   GPIO_PIN_4

#define KEY3_RCU   RCU_GPIOB
#define KEY3_PORT  GPIOB
#define KEY3_PIN   GPIO_PIN_5

#define KEY4_RCU   RCU_GPIOB
#define KEY4_PORT  GPIOB
#define KEY4_PIN   GPIO_PIN_6

#define KEY5_RCU   RCU_GPIOB
#define KEY5_PORT  GPIOB
#define KEY5_PIN   GPIO_PIN_7

#define KEY6_RCU   RCU_GPIOD
#define KEY6_PORT  GPIOD
#define KEY6_PIN   GPIO_PIN_7

/* 按键电平定义：按下时为 0 (低电平) 或 1 (高电平)。
 * 默认为低有效（短接至 GND 时触发），如果你的电路是高有效请改为 1。
 */
#ifndef KEY_PRESSED_LEVEL
#define KEY_PRESSED_LEVEL   0
#endif

/* 扫描周期与阈值（单位 ms）
 * BSP_KEY_Scan 应该以约 KEY_SCAN_INTERVAL_MS 周期被调用（例如在 systick 或定时器中）
 */
#define KEY_SCAN_INTERVAL_MS    10
#define KEY_DEBOUNCE_MS         30
#define KEY_SHORT_PRESS_MS      50
#define KEY_LONG_PRESS_MS       1000

/* 按键数量 */
#define BSP_KEY_COUNT 6

typedef enum {
    KEY_ID_1 = 0,
    KEY_ID_2,
    KEY_ID_3,
    KEY_ID_4,
    KEY_ID_5,
    KEY_ID_6,
} KEY_ID_t;

typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PRESS,      // 按下（去抖后第一次确认）
    KEY_EVENT_RELEASE,    // 释放
    KEY_EVENT_SHORT,      // 短按
    KEY_EVENT_LONG,       // 长按
} KEY_Event_t;

typedef void (*KeyEventCallback)(KEY_ID_t id, KEY_Event_t evt);

/* 初始化所有按键（配置 GPIO） */
void BSP_KEY_InitAll(void);

/* 必须定期调用以完成去抖与长按检测。建议在 systick 或 10ms 定时器中调用 */
void BSP_KEY_Scan(void);

/* 立即读取硬件电平（原始） 0/1 */
uint8_t BSP_KEY_GetRaw(KEY_ID_t id);

/* 读取去抖后的稳定电平（0/1），0 表示松开还是按下取决于 KEY_PRESSED_LEVEL */
uint8_t BSP_KEY_GetStable(KEY_ID_t id);

/* 注册按键事件回调（覆盖式） */
void BSP_KEY_RegisterCallback(KeyEventCallback cb);

#endif

