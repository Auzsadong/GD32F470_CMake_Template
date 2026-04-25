#include "bsp_key.h"
#include "systick.h"

/* 简单实现：基于定时扫描计数器来做去抖与按键时长统计
 * 要求：BSP_KEY_Scan() 以接近 KEY_SCAN_INTERVAL_MS 的周期被调用
 */

typedef struct {
    uint32_t rcu;
    uint32_t port;
    uint16_t pin;

    uint8_t raw;        // 原始输入电平
    uint8_t stable;     // 去抖后稳定的电平
    uint8_t last_stable;
    uint8_t db_cnt;     // 去抖计数

    uint16_t press_ms;  // 按下已持续的毫秒数 (基于 scan 周期)
    uint8_t long_sent;  // 是否已发送过长按事件
} key_obj_t;

static KeyEventCallback g_cb = 0;

static key_obj_t keys[BSP_KEY_COUNT];

/* 硬件定义表（和 bsp_key.h 中的宏一一对应） */
static const struct { uint32_t rcu; uint32_t port; uint16_t pin; } key_hw[BSP_KEY_COUNT] = {
    { KEY1_RCU, KEY1_PORT, KEY1_PIN },
    { KEY2_RCU, KEY2_PORT, KEY2_PIN },
    { KEY3_RCU, KEY3_PORT, KEY3_PIN },
    { KEY4_RCU, KEY4_PORT, KEY4_PIN },
    { KEY5_RCU, KEY5_PORT, KEY5_PIN },
    { KEY6_RCU, KEY6_PORT, KEY6_PIN },
};

/* 内部：读取硬件电平并映射为 0/1 */
static uint8_t ReadPinLevel(uint32_t port, uint16_t pin) {
    return (uint8_t)gpio_input_bit_get(port, pin);
}

void BSP_KEY_InitAll(void) {
    for (int i = 0; i < BSP_KEY_COUNT; i++) {
        rcu_periph_clock_enable(key_hw[i].rcu);
        gpio_mode_set(key_hw[i].port, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, key_hw[i].pin);

        keys[i].rcu = key_hw[i].rcu;
        keys[i].port = key_hw[i].port;
        keys[i].pin = key_hw[i].pin;
        keys[i].raw = ReadPinLevel(keys[i].port, keys[i].pin);
        keys[i].stable = keys[i].raw;
        keys[i].last_stable = keys[i].raw;
        keys[i].db_cnt = 0;
        keys[i].press_ms = 0;
        keys[i].long_sent = 0;
    }
}

uint8_t BSP_KEY_GetRaw(KEY_ID_t id) {
    if (id >= BSP_KEY_COUNT) return 0;
    return keys[id].raw;
}

uint8_t BSP_KEY_GetStable(KEY_ID_t id) {
    if (id >= BSP_KEY_COUNT) return 0;
    return keys[id].stable;
}

void BSP_KEY_RegisterCallback(KeyEventCallback cb) {
    g_cb = cb;
}

/* 主扫描函数：去抖、检测按下/释放、短按/长按 */
void BSP_KEY_Scan(void) {
    /* 以 KEY_SCAN_INTERVAL_MS 为单位更新状态 */
    for (int i = 0; i < BSP_KEY_COUNT; i++) {
        uint8_t level = ReadPinLevel(keys[i].port, keys[i].pin);
        keys[i].raw = level;

        if (level == keys[i].stable) {
            /* 稳定态未变化，清除去抖计数 */
            keys[i].db_cnt = 0;
        } else {
            /* 存在变化，累计去抖时间 */
            keys[i].db_cnt += KEY_SCAN_INTERVAL_MS;
            if (keys[i].db_cnt >= KEY_DEBOUNCE_MS) {
                /* 确认状态切换 */
                keys[i].last_stable = keys[i].stable;
                keys[i].stable = level;
                keys[i].db_cnt = 0;

                /* 事件：按下或释放（按下为与 KEY_PRESSED_LEVEL 相等） */
                if (keys[i].stable == KEY_PRESSED_LEVEL) {
                    /* 按下确认 */
                    keys[i].press_ms = 0;
                    keys[i].long_sent = 0;
                    if (g_cb) g_cb((KEY_ID_t)i, KEY_EVENT_PRESS);
                } else {
                    /* 释放确认 */
                    if (g_cb) g_cb((KEY_ID_t)i, KEY_EVENT_RELEASE);

                    /* 根据持续时间判定短按或长按（release 时判定短按） */
                    if (keys[i].press_ms >= KEY_LONG_PRESS_MS) {
                        /* 已经在按下期间发送过长按事件，release 只发 RELEASE */
                    } else if (keys[i].press_ms >= KEY_SHORT_PRESS_MS) {
                        if (g_cb) g_cb((KEY_ID_t)i, KEY_EVENT_SHORT);
                    }
                    keys[i].press_ms = 0;
                    keys[i].long_sent = 0;
                }
            }
        }

        /* 如果当前处于按下稳定态，累计按下时间并判断长按 */
        if (keys[i].stable == KEY_PRESSED_LEVEL) {
            keys[i].press_ms += KEY_SCAN_INTERVAL_MS;
            if (!keys[i].long_sent && keys[i].press_ms >= KEY_LONG_PRESS_MS) {
                keys[i].long_sent = 1;
                if (g_cb) g_cb((KEY_ID_t)i, KEY_EVENT_LONG);
            }
        }
    }
}

