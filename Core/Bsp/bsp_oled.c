#include "bsp_oled.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

// 显存缓存
static uint8_t OLED_GRAM[OLED_PAGE][OLED_COLUMN];
// 记录当前使用的 I2C 外设
static uint32_t OLED_I2C_PORT = I2C0;

// ========================== 硬件底层配置函数 ==========================

static rcu_periph_enum get_gpio_rcu(uint32_t gpio_port) {
    switch(gpio_port) {
        case GPIOA: return RCU_GPIOA;
        case GPIOB: return RCU_GPIOB;
        case GPIOC: return RCU_GPIOC;
        default: return RCU_GPIOB;
    }
}

static rcu_periph_enum get_i2c_rcu(uint32_t i2c_periph) {
    return (i2c_periph == I2C1) ? RCU_I2C1 : RCU_I2C0;
}

/**
 * @brief 初始化 GD32 的 I2C 和对应的 GPIO 引脚
 */
static void OLED_I2C_Init(uint32_t i2c_periph, uint32_t scl_port, uint32_t scl_pin, uint32_t sda_port, uint32_t sda_pin) {
    OLED_I2C_PORT = i2c_periph;

    rcu_periph_clock_enable(get_gpio_rcu(scl_port));
    rcu_periph_clock_enable(get_gpio_rcu(sda_port));
    rcu_periph_clock_enable(get_i2c_rcu(i2c_periph));

    gpio_af_set(scl_port, GPIO_AF_4, scl_pin);
    gpio_af_set(sda_port, GPIO_AF_4, sda_pin);

    gpio_mode_set(scl_port, GPIO_MODE_AF, GPIO_PUPD_PULLUP, scl_pin);
    gpio_output_options_set(scl_port, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, scl_pin);
    gpio_mode_set(sda_port, GPIO_MODE_AF, GPIO_PUPD_PULLUP, sda_pin);
    gpio_output_options_set(sda_port, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, sda_pin);

    // 降速保平安：400K降到100K，防止面包板和杜邦线信号干扰导致丢包
    i2c_clock_config(OLED_I2C_PORT, 100000, I2C_DTCY_2);
    i2c_mode_addr_config(OLED_I2C_PORT, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0x72);
    i2c_enable(OLED_I2C_PORT);
    i2c_ack_config(OLED_I2C_PORT, I2C_ACK_ENABLE);
}

/**
 * @brief 向OLED发送数据的底层函数 (修复硬件时序截断BUG)
 */
static void OLED_Send(uint8_t *data, uint8_t len) {
    uint32_t timeout = 0xFFFF;
    while(i2c_flag_get(OLED_I2C_PORT, I2C_FLAG_I2CBSY)) {
        if(--timeout == 0) return;
    }

    i2c_start_on_bus(OLED_I2C_PORT);
    timeout = 0xFFFF;
    while(!i2c_flag_get(OLED_I2C_PORT, I2C_FLAG_SBSEND)) {
        if(--timeout == 0) return;
    }

    i2c_master_addressing(OLED_I2C_PORT, OLED_ADDRESS, I2C_TRANSMITTER);
    timeout = 0xFFFF;
    while(!i2c_flag_get(OLED_I2C_PORT, I2C_FLAG_ADDSEND)) {
        if(--timeout == 0) return;
    }
    i2c_flag_clear(OLED_I2C_PORT, I2C_FLAG_ADDSEND);

    for(uint8_t i = 0; i < len; i++) {
        timeout = 0xFFFF;
        // 等待数据寄存器空 (TBE) 才能塞入下一个数据
        while(!i2c_flag_get(OLED_I2C_PORT, I2C_FLAG_TBE)) {
            if(--timeout == 0) return;
        }
        i2c_data_transmit(OLED_I2C_PORT, data[i]);
    }

    // 👇👇👇 致命BUG修复处：必须等待底层移位寄存器也彻底发送完成 (BTC)！
    timeout = 0xFFFF;
    while(!i2c_flag_get(OLED_I2C_PORT, I2C_FLAG_BTC)) {
        if(--timeout == 0) return;
    }
    // 👆👆👆 如果没有上面这步，STOP信号会把最后发出的指令拦腰斩断！

    i2c_stop_on_bus(OLED_I2C_PORT);
}

static void OLED_SendCmd(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    OLED_Send(buf, 2);
}

// ========================== OLED核心驱动 ==========================

void OLED_Init(uint32_t i2c_periph, uint32_t scl_port, uint32_t scl_pin, uint32_t sda_port, uint32_t sda_pin) {
    OLED_I2C_Init(i2c_periph, scl_port, scl_pin, sda_port, sda_pin);

    OLED_SendCmd(0xAE); // 关闭显示
    OLED_SendCmd(0xD5); // 设置时钟分频
    OLED_SendCmd(0x80);

    OLED_SendCmd(0xA8); // 设置多路复用率 (分辨率高度)
    OLED_SendCmd(0x1F); // 👇 【关键修改】128x32 屏幕必须为 0x1F (原为 0x3F)

    OLED_SendCmd(0xD3); // 设置显示偏移
    OLED_SendCmd(0x00);
    OLED_SendCmd(0x40); // 设置起始行

    // --- 屏幕旋转 180 度配置 ---
    OLED_SendCmd(0xA1); // 0xA1: 正常(左右反) / 0xA0: 旋转180度
    OLED_SendCmd(0xC8); // 0xC8: 正常(上下反) / 0xC0: 旋转180度

    OLED_SendCmd(0xDA); // COM引脚配置
    OLED_SendCmd(0x02); // 👇 【关键修改】128x32 屏幕必须为 0x02 (原为 0x12)

    OLED_SendCmd(0x81); // 对比度设置
    OLED_SendCmd(0xCF);
    OLED_SendCmd(0xD9); // 设置预充电周期
    OLED_SendCmd(0xF1);
    OLED_SendCmd(0xDB); // 设置VCOMH保持电压
    OLED_SendCmd(0x40);
    OLED_SendCmd(0xA4); // 全屏点亮恢复
    OLED_SendCmd(0xA6); // 正常显示模式

    OLED_SendCmd(0x8D); // 开启电荷泵 (SSD1306 核心)
    OLED_SendCmd(0x14);

    OLED_NewFrame();
    OLED_ShowFrame();
    OLED_SendCmd(0xAF); // 开启显示
}

void OLED_NewFrame(void) {
    memset(OLED_GRAM, 0, sizeof(OLED_GRAM));
}

void OLED_ShowFrame(void) {
    uint8_t buf[OLED_COLUMN + 1];
    buf[0] = 0x40;
    for (uint8_t i = 0; i < OLED_PAGE; i++) {
        OLED_SendCmd(0xB0 + i);
        OLED_SendCmd(0x00); // 这里的列地址设置改为标准 SSD1306 格式
        OLED_SendCmd(0x10);
        memcpy(buf + 1, OLED_GRAM[i], OLED_COLUMN);
        OLED_Send(buf, OLED_COLUMN + 1);
    }
}

void OLED_SetPixel(uint8_t x, uint8_t y, OLED_ColorMode color) {
    if (x >= OLED_COLUMN || y >= OLED_ROW) return;
    if (color == OLED_COLOR_NORMAL) {
        OLED_GRAM[y / 8][x] |= 1 << (y % 8);
    } else {
        OLED_GRAM[y / 8][x] &= ~(1 << (y % 8));
    }
}

// 这里的渲染逻辑已经过优化，兼容 32 位系统且不再依赖中间函数
static void OLED_SetBlock(uint8_t x, uint8_t y, const uint8_t *data, uint8_t w, uint8_t h, OLED_ColorMode color) {
    uint8_t pages = (h + 7) / 8;
    for (uint8_t i = 0; i < w; i++) {
        for (uint8_t j = 0; j < pages; j++) {
            uint8_t byte_data = data[i + j * w];
            for (uint8_t k = 0; k < 8; k++) {
                if (j * 8 + k >= h) break;
                // 如果字模位为 1，则按照用户要求的 color 绘制像素
                if ((byte_data >> k) & 0x01) {
                    OLED_SetPixel(x + i, y + j * 8 + k, color);
                } else {
                    // 如果位为 0，绘制反色（底色）
                    OLED_SetPixel(x + i, y + j * 8 + k, (OLED_ColorMode)!color);
                }
            }
        }
    }
}

// ========================== 绘图函数 ==========================

void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, OLED_ColorMode color) {
    int16_t dx = abs(x2 - x1), dy = abs(y2 - y1);
    int16_t sx = (x1 < x2) ? 1 : -1, sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;
    while (1) {
        OLED_SetPixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, OLED_ColorMode color) {
    OLED_DrawLine(x, y, x + w - 1, y, color);
    OLED_DrawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
    OLED_DrawLine(x, y, x, y + h - 1, color);
    OLED_DrawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
}

void OLED_DrawFilledRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, OLED_ColorMode color) {
    for (uint8_t i = 0; i < h; i++) {
        OLED_DrawLine(x, y + i, x + w - 1, y + i, color);
    }
}

void OLED_PrintASCIIChar(uint8_t x, uint8_t y, char ch, const ASCIIFont *font, OLED_ColorMode color) {
    uint32_t offset = (ch - ' ') * (((font->h + 7) / 8) * font->w);
    OLED_SetBlock(x, y, font->chars + offset, font->w, font->h, color);
}

void OLED_PrintASCIIString(uint8_t x, uint8_t y, char *str, const ASCIIFont *font, OLED_ColorMode color) {
    while (*str) {
        OLED_PrintASCIIChar(x, y, *str, font, color);
        x += font->w;
        str++;
    }
}

void OLED_DisPlay_On(void) { OLED_SendCmd(0xAF); }
void OLED_DisPlay_Off(void) { OLED_SendCmd(0xAE); }

void OLED_SetColorMode(OLED_ColorMode mode) {
    OLED_SendCmd((mode == OLED_COLOR_NORMAL) ? 0xA6 : 0xA7);
}