#ifndef SRXL2_CHANNELS_H
#define SRXL2_CHANNELS_H

#include <stdbool.h>
#include <stdint.h>

// Decoded SRXL2 control-channel ("stick position") data.
//
// MSRC's own srxl2.c only ever inspected Control (0xCD) packets closely
// enough to know "it's my turn to answer with telemetry" - it never
// decoded the actual channel values carried in those packets, because
// MSRC's job (sending sensor telemetry back to the transmitter) never
// needed them. This module adds that missing decode step, so a second
// consumer (e.g. a future RC-Cockpit task sharing this firmware image)
// can read live channel/switch positions without touching the SRXL2
// UART itself - srxl2.c remains the only code talking to the bus.
//
// Packet layout implemented here (SRXL2 Control packet, packetType
// 0xCD, command 0x00 ChannelData / 0x01 FailsafeChannelData), verified
// against the public SRXL2 spec and cross-checked against Betaflight's
// rx/srxl2.c + rx/srxl2_types.h (an independent, flight-tested
// implementation):
//
//   [0]      id            0xA6 (SRXL2_HEADER)
//   [1]      packetType    0xCD (SRXL2_PACKET_TYPE_CONTROL)
//   [2]      length
//   [3]      command       0x00 ChannelData / 0x01 FailsafeChannelData / 0x02 VTXData
//   [4]      replyId       device ID whose turn it is to answer with telemetry
//                           (this is the byte MSRC's process() already compares
//                           against sensor_id_ - channel data itself is valid
//                           on every Control frame, regardless of replyId)
//   [5]      rssi          int8_t
//   [6..7]   frameLosses   uint16_t, little-endian
//   [8..11]  channelMask   uint32_t, little-endian; bit N set = channel N present
//   [12..]   uint16_t per set bit in channelMask, ascending bit order, little-endian
//   [len-2..len-1] crc (big-endian; already handled by srxl2.c, not touched here)
//
// Channel values are centered on 0x8000 and span 0x0000..0xFFFF for the
// full physical travel, equivalent to roughly 988..2012us of pulse width
// (standard Spektrum convention) - see srxl2_channel_to_us() below.

#define SRXL2_MAX_CHANNELS 32

typedef struct srxl2_channels_t {
    uint16_t raw[SRXL2_MAX_CHANNELS];  // last known raw value per channel (0x0000..0xFFFF)
    uint32_t mask;                      // bitmask of channels ever seen so far
    bool failsafe;                      // true if the most recently parsed frame was Failsafe ChannelData
    uint32_t frame_count;               // increments on every successfully parsed ChannelData frame
} srxl2_channels_t;

// Thread-safe snapshot read, safe to call from any task other than the
// one that owns the SRXL2 UART (i.e. anything other than srxl2_task).
void srxl2_channels_get(srxl2_channels_t *out);

// Converts a raw channel value (0x0000..0xFFFF, center 0x8000) to the
// equivalent microsecond pulse width (~988..2012us, center 1500us).
static inline float srxl2_channel_to_us(uint16_t raw_value) {
    return ((float)(raw_value >> 2) / 16.0f) + 988.0f;
}

// Called from srxl2.c's process() for every received packet. Internally
// verifies id/packetType/command and is a no-op for anything that isn't
// a ChannelData or FailsafeChannelData Control packet - safe to call
// unconditionally with the raw, CRC-checked frame buffer and its length.
void srxl2_channels_parse(const uint8_t *data, uint8_t length);

#endif
