set(PICO_SDK_FREERTOS_SOURCE FreeRTOS-Kernel)

add_library(freertos
    ${PICO_SDK_FREERTOS_SOURCE}/event_groups.c
    ${PICO_SDK_FREERTOS_SOURCE}/list.c
    ${PICO_SDK_FREERTOS_SOURCE}/queue.c
    ${PICO_SDK_FREERTOS_SOURCE}/stream_buffer.c
    ${PICO_SDK_FREERTOS_SOURCE}/tasks.c
    ${PICO_SDK_FREERTOS_SOURCE}/timers.c
    ${PICO_SDK_FREERTOS_SOURCE}/portable/MemMang/heap_3.c
)

if (PICO_PLATFORM STREQUAL "rp2040")
    target_sources(freertos PRIVATE
        ${PICO_SDK_FREERTOS_SOURCE}/portable/GCC/ARM_CM0/port.c
    )
    target_include_directories(freertos PUBLIC
        .
        ${PICO_SDK_FREERTOS_SOURCE}/include
        ${PICO_SDK_FREERTOS_SOURCE}/portable/GCC/ARM_CM0
    )
else()
    # RP2350, Arm-Kern (Cortex-M33, non-secure). Der RISC-V-Kern des
    # RP2350 wird hier nicht unterstuetzt.
    set(FREERTOS_RP2350_PORT_DIR
        ${PICO_SDK_FREERTOS_SOURCE}/portable/ThirdParty/GCC/RP2350_ARM_NTZ/non_secure)
    target_sources(freertos PRIVATE
        ${FREERTOS_RP2350_PORT_DIR}/port.c
        ${FREERTOS_RP2350_PORT_DIR}/portasm.c
    )
    target_include_directories(freertos PUBLIC
        .
        ${PICO_SDK_FREERTOS_SOURCE}/include
        ${FREERTOS_RP2350_PORT_DIR}
    )
    target_compile_definitions(freertos PUBLIC
        LIB_FREERTOS_KERNEL=1
        FREE_RTOS_KERNEL_SMP=1
        PICO_CONFIG_RTOS_ADAPTER_HEADER=${CMAKE_CURRENT_LIST_DIR}/${FREERTOS_RP2350_PORT_DIR}/freertos_sdk_config.h
    )
    target_link_libraries(freertos PUBLIC
        hardware_clocks
        hardware_exception
        pico_multicore
    )
endif()
