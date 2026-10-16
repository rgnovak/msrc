#include "srxl2_channels.h"

#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "srxl2.h"  // SRXL2_HEADER, SRXL2_PACKET_TYPE_CONTROL

#define SRXL2_CMD_CHANNEL_DATA 0x00
#define SRXL2_CMD_FAILSAFE_CHANNEL_DATA 0x01
// 0x02 VTXData exists too, not relevant here.

#define SRXL2_CTRL_OFF_COMMAND 3
#define SRXL2_CTRL_OFF_CHANMASK 8
#define SRXL2_CTRL_OFF_CHANDATA 12

static srxl2_channels_t state;
static SemaphoreHandle_t lock;

static SemaphoreHandle_t get_lock(void) {
    // Created lazily on first use so no separate explicit init call is
    // needed from main.c; srxl2_task and any reader task both go
    // through this before touching `state`.
    if (lock == NULL) {
        lock = xSemaphoreCreateMutex();
    }
    return lock;
}

void srxl2_channels_get(srxl2_channels_t *out) {
    SemaphoreHandle_t m = get_lock();
    xSemaphoreTake(m, portMAX_DELAY);
    memcpy(out, &state, sizeof(state));
    xSemaphoreGive(m);
}

void srxl2_channels_parse(const uint8_t *data, uint8_t length) {
    if (length < SRXL2_CTRL_OFF_CHANDATA + 2) return;
    if (data[0] != SRXL2_HEADER || data[1] != SRXL2_PACKET_TYPE_CONTROL) return;

    uint8_t command = data[SRXL2_CTRL_OFF_COMMAND];
    if (command != SRXL2_CMD_CHANNEL_DATA && command != SRXL2_CMD_FAILSAFE_CHANNEL_DATA) return;

    uint32_t mask;
    memcpy(&mask, &data[SRXL2_CTRL_OFF_CHANMASK], sizeof(mask));
    if (mask == 0) return;  // dropped frame, per SRXL2 convention

    SemaphoreHandle_t m = get_lock();
    xSemaphoreTake(m, portMAX_DELAY);

    uint8_t offset = SRXL2_CTRL_OFF_CHANDATA;
    for (uint8_t ch = 0; ch < SRXL2_MAX_CHANNELS; ch++) {
        if (!(mask & (1u << ch))) continue;
        if (offset + 2 > length - 2) break;  // stay inside the frame, before the CRC
        uint16_t value;
        memcpy(&value, &data[offset], sizeof(value));
        state.raw[ch] = value;
        offset += 2;
    }
    state.mask |= mask;
    state.failsafe = (command == SRXL2_CMD_FAILSAFE_CHANNEL_DATA);
    state.frame_count++;

    xSemaphoreGive(m);
}
