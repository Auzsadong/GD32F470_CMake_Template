#ifndef BSP_GD30AD3344_H
#define BSP_GD30AD3344_H

#include "gd32f4xx.h"
#include <stdint.h>

/* --- 硬件引脚配置宏定义 (已切换至 GPIOB 和 SPI1) --- */
#define GD30_RCU_SPI          RCU_SPI1
#define GD30_RCU_GPIO         RCU_GPIOB
#define GD30_SPI_PERIPH       SPI1

#define GD30_CS_PORT          GPIOB
#define GD30_CS_PIN           GPIO_PIN_12

#define GD30_SCK_PORT         GPIOB
#define GD30_SCK_PIN          GPIO_PIN_13
#define GD30_MISO_PORT        GPIOB
#define GD30_MISO_PIN         GPIO_PIN_14
#define GD30_MOSI_PORT        GPIOB
#define GD30_MOSI_PIN         GPIO_PIN_15

/* GD32F4 系列中，PB13/14/15 的 SPI1 复用功能为 AF5 */
#define GD30_SPI_AF           GPIO_AF_5

/* --- GD30AD3344 配置枚举 --- */
typedef enum {
    GD30_MUX_AIN0_AIN1 = 0x00,
    GD30_MUX_AIN0_AIN3 = 0x01, // 关键：AIN0相对于AIN3测量 (支持外部参考)
    GD30_MUX_AIN1_AIN3 = 0x02, // 关键：AIN1相对于AIN3测量
    GD30_MUX_AIN2_AIN3 = 0x03, // 关键：AIN2相对于AIN3测量
    GD30_MUX_AIN0_GND  = 0x04,
    GD30_MUX_AIN1_GND  = 0x05,
    GD30_MUX_AIN2_GND  = 0x06,
    GD30_MUX_AIN3_GND  = 0x07
} GD30AD3344_Mux_t;

typedef enum {
    GD30_PGA_6_144V = 0x00,
    GD30_PGA_4_096V = 0x01,
    GD30_PGA_2_048V = 0x02,
    GD30_PGA_1_024V = 0x03,
    GD30_PGA_0_512V = 0x04,
    GD30_PGA_0_256V = 0x05,
    GD30_PGA_0_064V = 0x06
} GD30AD3344_Pga_t;

typedef enum {
    GD30_DR_6_25SPS = 0x00,
    GD30_DR_12_5SPS = 0x01,
    GD30_DR_25SPS   = 0x02,
    GD30_DR_50SPS   = 0x03,
    GD30_DR_100SPS  = 0x04,
    GD30_DR_250SPS  = 0x05,
    GD30_DR_500SPS  = 0x06,
    GD30_DR_1000SPS = 0x07
} GD30AD3344_Dr_t;

typedef enum {
    GD30_MODE_CONTINUOUS  = 0x00,
    GD30_MODE_SINGLE_SHOT = 0x01
} GD30AD3344_Mode_t;

/* 设备控制结构体 */
typedef struct {
    GD30AD3344_Mux_t  mux;
    GD30AD3344_Pga_t  pga;
    GD30AD3344_Dr_t   dr;
    GD30AD3344_Mode_t mode;
} GD30AD3344_HandleTypeDef;

/* --- API 声明 --- */
void GD30AD3344_Init(GD30AD3344_HandleTypeDef *dev);
void GD30AD3344_SetConfig(GD30AD3344_HandleTypeDef *dev, GD30AD3344_Mux_t mux, GD30AD3344_Pga_t pga, GD30AD3344_Dr_t dr, GD30AD3344_Mode_t mode);
int16_t GD30AD3344_ReadData_SingleShot(GD30AD3344_HandleTypeDef *dev);

#endif // BSP_GD30AD3344_H