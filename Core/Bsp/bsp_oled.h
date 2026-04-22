#ifndef __BSP_OLED_H
#define __BSP_OLED_H

#include "gd32f4xx.h"
#include <stdint.h>
#include "font.h"      // <-- 新增：直接包含字体和图片的结构体定义

// OLED器件地址 (8位地址)
#define OLED_ADDRESS 0x78

// OLED参数
#define OLED_PAGE 4            // 0.91寸屏幕只有 4 页 (原为 8)
#define OLED_ROW 8 * OLED_PAGE // OLED行数变成 32 行
#define OLED_COLUMN 128        // 列数不变

// 颜色模式枚举
typedef enum {
    OLED_COLOR_NORMAL = 0,   // 正常显示 (灭为背景，亮为前景)
    OLED_COLOR_REVERSED = 1  // 反色显示 (亮为背景，灭为前景)
} OLED_ColorMode;

/* 初始化与基础控制 */

void OLED_Init(uint32_t i2c_periph, uint32_t scl_port, uint32_t scl_pin, uint32_t sda_port, uint32_t sda_pin);
void OLED_DisPlay_On(void);
void OLED_DisPlay_Off(void);
void OLED_SetColorMode(OLED_ColorMode mode);

/* 显存与刷新操作 */
void OLED_NewFrame(void);
void OLED_ShowFrame(void);

/* 基础绘图函数 */
void OLED_SetPixel(uint8_t x, uint8_t y, OLED_ColorMode color);
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, OLED_ColorMode color);
void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, OLED_ColorMode color);
void OLED_DrawFilledRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, OLED_ColorMode color);
void OLED_DrawTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3, OLED_ColorMode color);
void OLED_DrawFilledTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3, OLED_ColorMode color);
void OLED_DrawCircle(uint8_t x, uint8_t y, uint8_t r, OLED_ColorMode color);
void OLED_DrawFilledCircle(uint8_t x, uint8_t y, uint8_t r, OLED_ColorMode color);
void OLED_DrawEllipse(uint8_t x, uint8_t y, uint8_t a, uint8_t b, OLED_ColorMode color);
void OLED_DrawImage(uint8_t x, uint8_t y, const Image *img, OLED_ColorMode color);

/* 字符与字符串绘制 */
void OLED_PrintASCIIChar(uint8_t x, uint8_t y, char ch, const ASCIIFont *font, OLED_ColorMode color);
void OLED_PrintASCIIString(uint8_t x, uint8_t y, char *str, const ASCIIFont *font, OLED_ColorMode color);
void OLED_PrintString(uint8_t x, uint8_t y, char *str, const Font *font, OLED_ColorMode color);

#endif /* __BSP_OLED_H */