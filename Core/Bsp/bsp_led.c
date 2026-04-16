#include "bsp_led.h"
#include "systick.h"

/* ================== 通用底层驱动 (内部私有) ================== */
static void In_LED_Init(uint32_t rcu, uint32_t port, uint32_t pin) {
    rcu_periph_clock_enable(rcu);
    gpio_mode_set(port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pin);
    gpio_output_options_set(port, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, pin);
    gpio_bit_reset(port, pin);
}

/* ================== LED1 对象方法 ================== */
static void LED1_Init(void)   { In_LED_Init(LED1_RCU, LED1_PORT, LED1_PIN); }
static void LED1_On(void)     { gpio_bit_set(LED1_PORT, LED1_PIN); }
static void LED1_Off(void)    { gpio_bit_reset(LED1_PORT, LED1_PIN); }
static void LED1_Toggle(void) { gpio_bit_toggle(LED1_PORT, LED1_PIN); }

/* ================== LED2 对象方法 ================== */
static void LED2_Init(void)   { In_LED_Init(LED2_RCU, LED2_PORT, LED2_PIN); }
static void LED2_On(void)     { gpio_bit_set(LED2_PORT, LED2_PIN); }
static void LED2_Off(void)    { gpio_bit_reset(LED2_PORT, LED2_PIN); }
static void LED2_Toggle(void) { gpio_bit_toggle(LED2_PORT, LED2_PIN); }

/* ================== LED3 对象方法 ================== */
static void LED3_Init(void)   { In_LED_Init(LED3_RCU, LED3_PORT, LED3_PIN); }
static void LED3_On(void)     { gpio_bit_set(LED3_PORT, LED3_PIN); }
static void LED3_Off(void)    { gpio_bit_reset(LED3_PORT, LED3_PIN); }
static void LED3_Toggle(void) { gpio_bit_toggle(LED3_PORT, LED3_PIN); }

/* ================== LED4 对象方法 ================== */
static void LED4_Init(void)   { In_LED_Init(LED4_RCU, LED4_PORT, LED4_PIN); }
static void LED4_On(void)     { gpio_bit_set(LED4_PORT, LED4_PIN); }
static void LED4_Off(void)    { gpio_bit_reset(LED4_PORT, LED4_PIN); }
static void LED4_Toggle(void) { gpio_bit_toggle(LED4_PORT, LED4_PIN); }

/* ================== LED5 对象方法 ================== */
static void LED5_Init(void)   { In_LED_Init(LED5_RCU, LED5_PORT, LED5_PIN); }
static void LED5_On(void)     { gpio_bit_set(LED5_PORT, LED5_PIN); }
static void LED5_Off(void)    { gpio_bit_reset(LED5_PORT, LED5_PIN); }
static void LED5_Toggle(void) { gpio_bit_toggle(LED5_PORT, LED5_PIN); }

/* ================== LED6 对象方法 ================== */
static void LED6_Init(void)   { In_LED_Init(LED6_RCU, LED6_PORT, LED6_PIN); }
static void LED6_On(void)     { gpio_bit_set(LED6_PORT, LED6_PIN); }
static void LED6_Off(void)    { gpio_bit_reset(LED6_PORT, LED6_PIN); }
static void LED6_Toggle(void) { gpio_bit_toggle(LED6_PORT, LED6_PIN); }


/* ================== 对象实例化  ================== */
LED_Device_t LED1 = { LED1_RCU, LED1_PORT, LED1_PIN, LED1_Init, LED1_On, LED1_Off, LED1_Toggle };
LED_Device_t LED2 = { LED2_RCU, LED2_PORT, LED2_PIN, LED2_Init, LED2_On, LED2_Off, LED2_Toggle };
LED_Device_t LED3 = { LED3_RCU, LED3_PORT, LED3_PIN, LED3_Init, LED3_On, LED3_Off, LED3_Toggle };
LED_Device_t LED4 = { LED4_RCU, LED4_PORT, LED4_PIN, LED4_Init, LED4_On, LED4_Off, LED4_Toggle };
LED_Device_t LED5 = { LED5_RCU, LED5_PORT, LED5_PIN, LED5_Init, LED5_On, LED5_Off, LED5_Toggle };
LED_Device_t LED6 = { LED6_RCU, LED6_PORT, LED6_PIN, LED6_Init, LED6_On, LED6_Off, LED6_Toggle };


/* ================== 高级业务封装 ================== */
void BSP_LED_InitAll(void) {
    LED1.Init(); LED2.Init(); LED3.Init();
    LED4.Init(); LED5.Init(); LED6.Init();
}

void BSP_LED_SystemTest(void) {
    LED_Device_t* leds[] = {&LED1, &LED2, &LED3, &LED4, &LED5, &LED6};
    for(int i = 0; i < 6; i++) {
        leds[i]->On();
        delay_1ms(200);
        leds[i]->Off();
    }
    for(int i = 5; i >= 0; i--) {
        leds[i]->On();
        delay_1ms(200);
        leds[i]->Off();
    }
}
