include_directories(. pio protocol sensor cockpit)

# Optional subsystems on top of the MSRC core, one switch each, so a
# single codebase can serve every model: e.g. the Rockstar wants
# MSRC_ENABLE_COCKPIT but not MSRC_ENABLE_LIGHT_CONTROLLER, the J65
# the other way round. Each one compiles to nothing when OFF - no
# dead code, no pin usage - and is also exposed to C as a 0/1 macro
# of the same name (see the target_compile_definitions below), so a
# shared file like main.c can #if around a feature it doesn't need.
option(MSRC_ENABLE_COCKPIT "Build the cockpit extension: PFD/map displays, pilot-head gaze servo, IMU (see cockpit/README.md)" ON)
option(MSRC_ENABLE_AUDIO "Build the Cockpit-Audio extension: ESP32->RP2350 port, not written yet (see audio/README.md)" OFF)
option(MSRC_ENABLE_LIGHT_CONTROLLER "Build the light-controller extension: beacon/landing light/strobe/position via external LED driver, not written yet (see light_controller/README.md)" OFF)

add_executable(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    MSRC_ENABLE_COCKPIT=$<BOOL:${MSRC_ENABLE_COCKPIT}>
    MSRC_ENABLE_AUDIO=$<BOOL:${MSRC_ENABLE_AUDIO}>
    MSRC_ENABLE_LIGHT_CONTROLLER=$<BOOL:${MSRC_ENABLE_LIGHT_CONTROLLER}>
)

add_subdirectory(protocol)
add_subdirectory(sensor)
add_subdirectory(pio)
if(MSRC_ENABLE_COCKPIT)
    add_subdirectory(cockpit)
endif()
if(MSRC_ENABLE_AUDIO)
    add_subdirectory(audio)
endif()
if(MSRC_ENABLE_LIGHT_CONTROLLER)
    add_subdirectory(light_controller)
endif()

target_sources(${PROJECT_NAME} PRIVATE
    main.c
    uart.c
    common.c
    led.c
    config.c
    sim_rx.c
    uart_pio.c
    usb.c
    serial_monitor.c
    dbg_rb.c
    dbg_task.c
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    freertos
    hardware_uart
    hardware_irq
    hardware_pwm
    hardware_clocks
    hardware_adc
    hardware_pio
    hardware_i2c
    hardware_flash
    hardware_spi
    pico_i2c_slave
)

pico_add_extra_outputs(${PROJECT_NAME})

pico_enable_stdio_usb(${PROJECT_NAME} 1) # 1 normal, 0 probe debug 
pico_enable_stdio_uart(${PROJECT_NAME} 0) # 0 normal, 1 probe debug
