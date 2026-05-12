#include <stdint.h>

#include "gd30ad3344.h"

/* 内部辅助宏：CS 引脚控制 */
#define GD30_CS_LOW()     gpio_bit_reset(GD30_CS_PORT, GD30_CS_PIN)
#define GD30_CS_HIGH()    gpio_bit_set(GD30_CS_PORT, GD30_CS_PIN)

/* 内部辅助函数：GD32 SPI 读写单个字节 */
static uint8_t GD30_SPI_ReadWriteByte(uint8_t data) {
    while(RESET == spi_i2s_flag_get(GD30_SPI_PERIPH, SPI_FLAG_TBE));
    spi_i2s_data_transmit(GD30_SPI_PERIPH, data);

    while(RESET == spi_i2s_flag_get(GD30_SPI_PERIPH, SPI_FLAG_RBNE));
    return spi_i2s_data_receive(GD30_SPI_PERIPH);
}

/* 内部辅助函数：构建 16 位配置字 */
static uint16_t GD30AD3344_BuildRegister(GD30AD3344_HandleTypeDef *dev, uint8_t start_conv) {
    uint16_t config = 0;

    if (start_conv) {
        config |= (1 << 15);      // OS位: 1 = 启动单次转换
    }

    config |= (dev->mux & 0x07) << 12; // MUX[2:0]
    config |= (dev->pga & 0x07) << 9;  // PGA[2:0]
    config |= (dev->mode & 0x01) << 8; // MODE
    config |= (dev->dr & 0x07) << 5;   // DR[2:0]

    config |= (0 << 4);           // Reserved 0
    config |= (1 << 3);           // PULL_UP_EN = 1 (开启内部上拉，防止浮空漏电)
    config |= (1 << 1);           // NOP[1:0] = 01 (有效数据)
    config |= (1 << 0);           // Reserved 1

    return config;
}

/* --- BSP 接口实现 --- */

void GD30AD3344_Init(GD30AD3344_HandleTypeDef *dev) {
    spi_parameter_struct spi_init_struct;

    /* 1. 使能外设时钟 (现在是 GPIOB 和 SPI1) */
    rcu_periph_clock_enable(GD30_RCU_GPIO);
    rcu_periph_clock_enable(GD30_RCU_SPI);

    /* 2. 配置 GPIO 复用功能 */
    gpio_af_set(GD30_SCK_PORT, GD30_SPI_AF, GD30_SCK_PIN | GD30_MISO_PIN | GD30_MOSI_PIN);

    /* 3. 配置 SPI 引脚 */
    gpio_mode_set(GD30_SCK_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, GD30_SCK_PIN | GD30_MOSI_PIN);
    gpio_output_options_set(GD30_SCK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GD30_SCK_PIN | GD30_MOSI_PIN);

    gpio_mode_set(GD30_MISO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GD30_MISO_PIN);

    /* 4. 配置 CS 软件控制引脚 */
    gpio_mode_set(GD30_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GD30_CS_PIN);
    gpio_output_options_set(GD30_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GD30_CS_PIN);
    GD30_CS_HIGH(); // 默认拉高，失能芯片

    /* 5. 配置 SPI 外设参数 */
    // GD30AD3344 需要 SPI 模式 1: CPOL=0 (空闲低电平), CPHA=1 (第二个边沿采样)
    // 注意：GD32F4 的 SPI1 挂载在 APB1 (时钟频率通常为系统主频的一半，最高 60MHz/100MHz)
    // 这里预分频器选 32，跑个几兆赫兹毫无压力。
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_LOW_PH_2EDGE; // 模式 1
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_32;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;         // MSB 先发
    spi_init(GD30_SPI_PERIPH, &spi_init_struct);

    spi_enable(GD30_SPI_PERIPH);

    /* 6. 初始化设备软件结构体默认值 (满足外部参考AIN3需求) */
    dev->mux  = GD30_MUX_AIN0_AIN3;
    dev->pga  = GD30_PGA_2_048V;
    dev->dr   = GD30_DR_100SPS;
    dev->mode = GD30_MODE_SINGLE_SHOT;
}

void GD30AD3344_SetConfig(GD30AD3344_HandleTypeDef *dev, GD30AD3344_Mux_t mux, GD30AD3344_Pga_t pga, GD30AD3344_Dr_t dr, GD30AD3344_Mode_t mode) {
    dev->mux = mux;
    dev->pga = pga;
    dev->dr = dr;
    dev->mode = mode;
}

int16_t GD30AD3344_ReadData_SingleShot(GD30AD3344_HandleTypeDef *dev) {
    uint16_t config = GD30AD3344_BuildRegister(dev, 1);

    // 1. 发送配置命令并启动转换
    GD30_CS_LOW();
    GD30_SPI_ReadWriteByte((config >> 8) & 0xFF);
    GD30_SPI_ReadWriteByte(config & 0xFF);
    GD30_CS_HIGH();

    // 2. 轮询等待转换完成
    uint32_t timeout = 0x3FFFF;
    uint8_t data_ready = 0;

    while (timeout--) {
        GD30_CS_LOW();

        // 软延时，等待引脚电平稳定
        for(volatile int i = 0; i < 50; i++);

        // 读取 MISO 状态判断数据是否就绪
        if (gpio_input_bit_get(GD30_MISO_PORT, GD30_MISO_PIN) == RESET) {
            data_ready = 1;
            break; // 此时保持 CS 低电平，准备读取数据
        }

        GD30_CS_HIGH();
        // 避免过于频繁操作总线
        for(volatile int i = 0; i < 2000; i++);
    }

    if (!data_ready) {
        GD30_CS_HIGH();
        return 0;
    }

    // 3. 读取 16 位转换结果
    uint8_t msb = GD30_SPI_ReadWriteByte(0x00);
    uint8_t lsb = GD30_SPI_ReadWriteByte(0x00);
    GD30_CS_HIGH();

    // 组合数据，默认二进制补码格式
    int16_t result = (int16_t)((msb << 8) | lsb);

    return result;
}