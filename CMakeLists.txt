cmake_minimum_required(VERSION 3.22)

# 1. 设置项目名称
set(CMAKE_PROJECT_NAME F470_Cmake_Template)
project(${CMAKE_PROJECT_NAME} C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_STANDARD 11)

# 2. 【关键修复】在此处定义目标
# 只有先执行了这一行，后面的 target_include_directories 才能找到对象
add_executable(${CMAKE_PROJECT_NAME}
        Core/Bsp/bsp_uart.c
        Core/Bsp/bsp_uart.h
        Core/Bsp/bsp_rtc.c
        Core/Bsp/bsp_rtc.h
        Core/Bsp/bsp_adc.c
        Core/Bsp/bsp_adc.h
        Core/Bsp/bsp_dac.c
        Core/Bsp/bsp_dac.h)

# 3. 宏定义
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        GD32F470
        USE_STDPERIPH_DRIVER
)

# 4. 包含目录 (Include Paths)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        "Core/App/Inc"
        "Core/Bsp"
        "Core/Sys"
        "Drivers/CMSIS"
        "Drivers/CMSIS/Include"             # 👈 必须添加这一行，这里通常存放 core_cm4.h
        "Drivers/CMSIS/GD/GD32F4xx/Include"
        "Drivers/Library/GD32F4xx_standard_peripheral/Include"
        "Drivers/Library/GD32F4xx_usb_library/driver/Include"
        "Drivers/Library/GD32F4xx_usb_library/device/Include"
        "Drivers/Library/Third_Party"
)

# 5. 源文件搜寻
file(GLOB_RECURSE APP_SOURCES "Core/App/*.c")
file(GLOB_RECURSE BSP_SOURCES "Core/Bsp/*.c")
file(GLOB_RECURSE SYS_SOURCES "Core/Sys/*.c")
file(GLOB_RECURSE PERIPH_SOURCES "Drivers/Library/GD32F4xx_standard_peripheral/Source/*.c")

# 系统初始化与你指定的启动文件
set(STARTUP_SYSTEM_SOURCES
        "Core/Sys/system_gd32f4xx.c"
        "startup_gd32f407_427.s"
)

# 将源文件添加到目标
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${APP_SOURCES}
        ${BSP_SOURCES}
        ${SYS_SOURCES}
        ${CORE_SOURCES}
        ${PERIPH_SOURCES}
        ${STARTUP_SYSTEM_SOURCES}
)

# 6. 链接设置
# 确保你已经把之前的 STM32F407XX_FLASH.ld 改名为 GD32F470VET6_FLASH.ld
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/GD32F470VET6_FLASH.ld")

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
        -T${LINKER_SCRIPT}
        -Wl,-Map=${CMAKE_PROJECT_NAME}.map
        -Wl,--gc-sections
        -Wl,-u,_printf_float
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        m
        c
        nosys
)

# 7. 生成 Hex/Bin
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
)
