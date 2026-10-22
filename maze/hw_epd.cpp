/*
 * hw_epd.cpp — pilote réel : tampon 2 bits/pixel et envoi au panneau.
 *
 * Le pilote bas niveau EPD_1in54g.cpp et DEV_Config.cpp est la copie du code
 * de démonstration officiel Waveshare de la carte (voir l'en-tête de ces
 * fichiers pour l'URL et le commit). Les couleurs du panneau sont :
 * 0 = noir, 1 = blanc, 2 = jaune, 3 = rouge.
 */
#include "hw_epd.h"

#include "DEV_Config.h"
#include "EPD_1in54g.h"

#include <string.h>

namespace maze {

namespace {
constexpr int EPD_WIDTH = 200;
constexpr int EPD_HEIGHT = 200;
constexpr int BYTES_PER_ROW = EPD_WIDTH / 4; // 50 octets par ligne
} // namespace

EpdDisplay::EpdDisplay()
{
    clear(Color::White);
}

uint8_t EpdDisplay::epdColor(Color c)
{
    switch (c) {
    case Color::White:  return EPD_1IN54G_WHITE;  // 0x1
    case Color::Black:  return EPD_1IN54G_BLACK;  // 0x0
    case Color::Red:    return EPD_1IN54G_RED;    // 0x3
    case Color::Yellow: return EPD_1IN54G_YELLOW; // 0x2
    default:            return EPD_1IN54G_WHITE;
    }
}

void EpdDisplay::setPixel(int x, int y, Color c)
{
    if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) return;
    const uint8_t value = epdColor(c);
    const int index = y * BYTES_PER_ROW + x / 4;
    const int shift = 6 - 2 * (x % 4);
    const uint8_t mask = (uint8_t)(0xC0u >> (6 - shift));
    image_[index] = (uint8_t)((image_[index] & (uint8_t)~mask) |
                              (uint8_t)(value << shift));
}

void EpdDisplay::clear(Color c)
{
    const uint8_t value = epdColor(c);
    memset(image_, (value << 6) | (value << 4) | (value << 2) | value,
           sizeof(image_));
}

void EpdDisplay::fillRect(int x, int y, int w, int h, Color c)
{
    if (w <= 0 || h <= 0) return;
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > EPD_WIDTH) x1 = EPD_WIDTH;
    if (y1 > EPD_HEIGHT) y1 = EPD_HEIGHT;
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            setPixel(px, py, c);
        }
    }
}

void EpdDisplay::flush()
{
    EPD_1IN54G_Display(image_);
}

void initDisplayHardware()
{
    if (DEV_Module_Init() != 0) {
        return;
    }
    EPD_1IN54G_Init_Fast();
    EPD_1IN54G_Clear(EPD_1IN54G_WHITE);
}

} // namespace maze
