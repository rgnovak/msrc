#include "display.h"

#include <stdlib.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pfd.h"
#include "pico/stdlib.h"

/* ST7735 panel geometry: both panels are 128x128 but sit at an offset
 * inside the controller's native (larger) addressable area. */
#define PANEL_XSTART 2
#define PANEL_YSTART 1
#define FB_BYTES (W * H * 2)

static uint16_t *fbA;
static uint16_t *fbB;
static uint16_t *frontFB;
uint16_t *backFB;  // declared extern in pfd.h; this is the one definition

static bool backlight_on = false;

static inline void write_cmd(uint8_t cs, uint8_t c) {
    gpio_put(COCKPIT_PIN_DC, 0);
    gpio_put(cs, 0);
    spi_write_blocking(spi0, &c, 1);
    gpio_put(cs, 1);
}

static inline void write_data8(uint8_t cs, uint8_t d) {
    gpio_put(COCKPIT_PIN_DC, 1);
    gpio_put(cs, 0);
    spi_write_blocking(spi0, &d, 1);
    gpio_put(cs, 1);
}

static void set_window(uint8_t cs, int x0, int y0, int w, int h) {
    write_cmd(cs, 0x2A);  // CASET
    write_data8(cs, 0);
    write_data8(cs, (uint8_t)x0);
    write_data8(cs, 0);
    write_data8(cs, (uint8_t)(x0 + w - 1));

    write_cmd(cs, 0x2B);  // RASET
    write_data8(cs, 0);
    write_data8(cs, (uint8_t)y0);
    write_data8(cs, 0);
    write_data8(cs, (uint8_t)(y0 + h - 1));
}

static void init_one_panel(uint8_t cs) {
    spi_set_baudrate(spi0, 1000000);

    write_cmd(cs, 0x01);
    sleep_ms(150);  // SWRESET
    write_cmd(cs, 0x11);
    sleep_ms(500);  // SLPOUT
    write_cmd(cs, 0x3A);
    write_data8(cs, 0x05);  // COLMOD: RGB565
    write_cmd(cs, 0x36);
    write_data8(cs, 0xC0);  // MADCTL: 180 degree rotation
    write_cmd(cs, 0x29);    // DISPON
}

/* Explicitly builds {hi, lo} bytes rather than casting a 16-bit
 * buffer, same as the original Arduino clear loop - mostly moot for
 * pure black (0xFFFF either way round), but kept faithful in case
 * this ever gets reused for a non-black fill. */
static void clear_panel_black(uint8_t cs) {
    uint8_t buf[512];
    uint8_t hi = (uint8_t)(COL_BLACK >> 8);
    uint8_t lo = (uint8_t)(COL_BLACK & 0xFF);
    for (unsigned i = 0; i < sizeof(buf); i += 2) {
        buf[i] = hi;
        buf[i + 1] = lo;
    }

    set_window(cs, PANEL_XSTART, PANEL_YSTART, W, H);
    write_cmd(cs, 0x2C);  // RAMWR
    gpio_put(COCKPIT_PIN_DC, 1);
    gpio_put(cs, 0);

    int total = FB_BYTES;
    int sent = 0;
    while (sent < total) {
        int chunk = (total - sent < (int)sizeof(buf)) ? (total - sent) : (int)sizeof(buf);
        spi_write_blocking(spi0, buf, chunk);
        sent += chunk;
    }
    gpio_put(cs, 1);
}

/* Pushes frontFB to one panel in a single block transfer. The
 * original Arduino code sent the same bytes one at a time with
 * SPI.transfer(); here spi_write_blocking() takes the whole buffer at
 * once, which is just a faster way to put the identical bytes on the
 * wire - the color values in pfd.h are already pre-swapped by
 * panelColor() for exactly this native-endian raw send. */
static void push_front_buffer(uint8_t cs) {
    spi_set_baudrate(spi0, 20000000);

    set_window(cs, PANEL_XSTART, PANEL_YSTART, W, H);
    write_cmd(cs, 0x2C);  // RAMWR
    gpio_put(COCKPIT_PIN_DC, 1);
    gpio_put(cs, 0);
    spi_write_blocking(spi0, (const uint8_t *)frontFB, FB_BYTES);
    gpio_put(cs, 1);
}

void cockpitDisplayBegin(void) {
    gpio_init(COCKPIT_PIN_CS1);
    gpio_set_dir(COCKPIT_PIN_CS1, GPIO_OUT);
    gpio_put(COCKPIT_PIN_CS1, 1);

    gpio_init(COCKPIT_PIN_CS2);
    gpio_set_dir(COCKPIT_PIN_CS2, GPIO_OUT);
    gpio_put(COCKPIT_PIN_CS2, 1);

    gpio_init(COCKPIT_PIN_DC);
    gpio_set_dir(COCKPIT_PIN_DC, GPIO_OUT);

    gpio_init(COCKPIT_PIN_RST);
    gpio_set_dir(COCKPIT_PIN_RST, GPIO_OUT);

    gpio_init(COCKPIT_PIN_BL1);
    gpio_set_dir(COCKPIT_PIN_BL1, GPIO_OUT);
    gpio_put(COCKPIT_PIN_BL1, 0);  // backlights off until the panels are black

    gpio_init(COCKPIT_PIN_BL2);
    gpio_set_dir(COCKPIT_PIN_BL2, GPIO_OUT);
    gpio_put(COCKPIT_PIN_BL2, 0);

    spi_init(spi0, 1000000);
    gpio_set_function(COCKPIT_PIN_SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(COCKPIT_PIN_SPI_MOSI, GPIO_FUNC_SPI);

    gpio_put(COCKPIT_PIN_RST, 0);
    sleep_ms(20);
    gpio_put(COCKPIT_PIN_RST, 1);
    sleep_ms(80);

    init_one_panel(COCKPIT_PIN_CS1);
    init_one_panel(COCKPIT_PIN_CS2);

    spi_set_baudrate(spi0, 20000000);
    clear_panel_black(COCKPIT_PIN_CS1);
    clear_panel_black(COCKPIT_PIN_CS2);

    cockpitDisplaySetBacklight(true);  // panels are black now, safe to show them

    fbA = malloc(FB_BYTES);
    fbB = malloc(FB_BYTES);
    if (!fbA || !fbB) {
        // Matches the original's "can't recover from this" behavior.
        while (1) tight_loop_contents();
    }

    frontFB = fbA;
    backFB = fbB;
    setPFDFramebuffer(backFB);
}

void cockpitDisplaySetBacklight(bool on) {
    gpio_put(COCKPIT_PIN_BL1, on);
    gpio_put(COCKPIT_PIN_BL2, on);
    backlight_on = on;
}

bool cockpitDisplayGetBacklight(void) { return backlight_on; }

void cockpitDisplaySwapAndPush(uint8_t cs) {
    uint16_t *tmp = frontFB;
    frontFB = backFB;
    backFB = tmp;
    setPFDFramebuffer(backFB);

    push_front_buffer(cs);
}

void cockpitDisplayPushBootscreen(void) {
    uint16_t *tmp = frontFB;
    frontFB = backFB;
    backFB = tmp;

    push_front_buffer(COCKPIT_PIN_CS1);
    push_front_buffer(COCKPIT_PIN_CS2);

    tmp = frontFB;
    frontFB = backFB;
    backFB = tmp;
    setPFDFramebuffer(backFB);
}
