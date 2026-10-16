#include "bootscreen.h"

#include <stdint.h>
#include <string.h>

#include "pfd.h"

/* Ported near-verbatim from RC-Cockpit's bootscreen.h (it was
 * header-only in the original Arduino sketch; split into its own
 * translation unit here, see bootscreen.h). */

/* 5x7 font, capital letters only - just enough for "GARMIN". */
static const uint8_t font5x7[26][7] = {
    // A
    {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001},
    // B
    {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110},
    // C
    {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110},
    // D
    {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110},
    // E
    {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111},
    // F
    {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000},
    // G
    {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110},
    // H
    {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001},
    // I
    {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b11111},
    // J
    {0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100},
    // K
    {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001},
    // L
    {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111},
    // M
    {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001},
    // N
    {0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001},
    // O
    {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110},
    // P
    {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000},
    // Q
    {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101},
    // R
    {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001},
    // S
    {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110},
    // T
    {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100},
    // U
    {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110},
    // V
    {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100},
    // W
    {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001},
    // X
    {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001},
    // Y
    {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100},
    // Z
    {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111},
};

static void drawBigChar(int x, int y, char c, uint16_t color) {
    if (c < 'A' || c > 'Z') return;

    int idx = c - 'A';

    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (font5x7[idx][row] & (1 << (4 - col))) {
                px(x + col, y + row, color);
            }
        }
    }
}

static void drawBigText(int x, int y, const char *text, uint16_t color) {
    int xPos = x;
    while (*text) {
        drawBigChar(xPos, y, *text, color);
        xPos += 6;  // 5px glyph width + 1px spacing
        text++;
    }
}

void showGarminBootscreen(void) {
    /* Garmin-blue background. */
    uint16_t garminBlue = panelColor(RGB565(0, 82, 156));
    clearFB(garminBlue);

    /* "GARMIN" centered (6 letters x 6px). */
    int textWidth = 6 * 6;
    int startX = (W - textWidth) / 2;
    int startY = 50;

    drawBigText(startX, startY, "GARMIN", COL_WHITE);

    /* Underline, G1000-style. */
    int lineY = startY + 10;
    for (int x = startX; x < startX + textWidth; x++) {
        px(x, lineY, COL_WHITE);
    }

    /* Small "(C) GARMIN" copyright line, hand-drawn glyph-by-glyph
     * with a few manual approximations (this is what the original
     * Arduino sketch did too - a dedicated small font was never
     * written for it). */
    const char *copyright = "(C) GARMIN";
    int cpWidth = (int)strlen(copyright) * 4;
    int cpX = (W - cpWidth) / 2;

    int xPos = cpX;
    for (const char *p = copyright; *p; p++) {
        if (*p == '(' || *p == ')') {
            if (*p == '(') {
                px(xPos, lineY + 6, COL_WHITE);
                px(xPos, lineY + 7, COL_WHITE);
                px(xPos, lineY + 8, COL_WHITE);
                px(xPos + 1, lineY + 5, COL_WHITE);
                px(xPos + 1, lineY + 9, COL_WHITE);
            } else {
                px(xPos + 1, lineY + 6, COL_WHITE);
                px(xPos + 1, lineY + 7, COL_WHITE);
                px(xPos + 1, lineY + 8, COL_WHITE);
                px(xPos, lineY + 5, COL_WHITE);
                px(xPos, lineY + 9, COL_WHITE);
            }
            xPos += 3;
        } else if (*p == 'C') {
            px(xPos + 1, lineY + 5, COL_WHITE);
            px(xPos, lineY + 6, COL_WHITE);
            px(xPos, lineY + 7, COL_WHITE);
            px(xPos, lineY + 8, COL_WHITE);
            px(xPos + 1, lineY + 9, COL_WHITE);
            xPos += 3;
        } else if (*p >= 'A' && *p <= 'Z') {
            int charIdx = *p - 'A';
            for (int r = 0; r < 5; r++) {
                for (int c = 0; c < 3; c++) {
                    if (charIdx == 6) {  // G
                        if ((r == 0 && c > 0) || (r == 4 && c > 0) || (c == 0 && r > 0 && r < 4) ||
                            (r == 2 && c > 0) || (c == 2 && r > 1)) {
                            px(xPos + c, lineY + 5 + r, COL_WHITE);
                        }
                    } else if (charIdx == 0) {  // A
                        if ((r == 0 && c == 1) || (c == 0 && r > 0) || (c == 2 && r > 0) || (r == 2)) {
                            px(xPos + c, lineY + 5 + r, COL_WHITE);
                        }
                    } else if (charIdx == 17) {  // R
                        if ((c == 0) || (r == 0 && c > 0) || (r == 2 && c > 0) || (c == 2 && r == 1) ||
                            (r == 3 && c == 1) || (r == 4 && c == 2)) {
                            px(xPos + c, lineY + 5 + r, COL_WHITE);
                        }
                    } else if (charIdx == 12) {  // M
                        if ((c == 0) || (c == 2) || (r == 1 && c == 1)) {
                            px(xPos + c, lineY + 5 + r, COL_WHITE);
                        }
                    } else if (charIdx == 8) {  // I
                        if (r == 0 || r == 4 || c == 1) {
                            px(xPos + c, lineY + 5 + r, COL_WHITE);
                        }
                    } else if (charIdx == 13) {  // N
                        if ((c == 0) || (c == 2) || (r == c + 1)) {
                            px(xPos + c, lineY + 5 + r, COL_WHITE);
                        }
                    }
                }
            }
            xPos += 4;
        } else if (*p == ' ') {
            xPos += 3;
        }
    }
}
