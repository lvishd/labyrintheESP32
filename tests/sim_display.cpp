/*
 * sim_display.cpp — écran simulé et export PNG agrandi.
 *
 * Palette interne : 0 = blanc, 1 = noir, 2 = rouge, 3 = jaune.
 * Toute commande qui sort de 0..199 incrémente un compteur de débordements.
 */
#include "sim_display.h"

#include <stdio.h>
#include <string.h>
#include <vector>

namespace sim {

namespace {

uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t len)
{
    crc ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = (uint32_t)(-(int32_t)(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

uint32_t adler32(const std::vector<uint8_t> &data)
{
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

void putBE32(std::vector<uint8_t> &out, uint32_t v)
{
    out.push_back((uint8_t)(v >> 24));
    out.push_back((uint8_t)(v >> 16));
    out.push_back((uint8_t)(v >> 8));
    out.push_back((uint8_t)v);
}

void writeChunk(FILE *f, const char type[4], const std::vector<uint8_t> &data)
{
    uint8_t len[4] = {
        (uint8_t)(data.size() >> 24), (uint8_t)(data.size() >> 16),
        (uint8_t)(data.size() >> 8), (uint8_t)data.size()
    };
    fwrite(len, 1, 4, f);
    fwrite(type, 1, 4, f);
    if (!data.empty()) fwrite(data.data(), 1, data.size(), f);

    std::vector<uint8_t> crcData;
    crcData.reserve(4 + data.size());
    crcData.insert(crcData.end(), type, type + 4);
    crcData.insert(crcData.end(), data.begin(), data.end());
    const uint32_t crc = crc32Update(0, crcData.data(), crcData.size());
    uint8_t crcBytes[4] = {
        (uint8_t)(crc >> 24), (uint8_t)(crc >> 16),
        (uint8_t)(crc >> 8), (uint8_t)crc
    };
    fwrite(crcBytes, 1, 4, f);
}

void rgbFor(maze::Color c, uint8_t &r, uint8_t &g, uint8_t &b)
{
    switch (c) {
    case maze::Color::White:  r = 255; g = 255; b = 255; break;
    case maze::Color::Black:  r = 0;   g = 0;   b = 0;   break;
    case maze::Color::Red:    r = 210; g = 0;   b = 0;   break;
    case maze::Color::Yellow: r = 255; g = 210; b = 0;   break;
    default:                  r = 255; g = 255; b = 255; break;
    }
}

} // namespace

SimDisplay::SimDisplay() : overflow_(0), flush_(0)
{
    clear(maze::Color::White);
}

void SimDisplay::clear(maze::Color c)
{
    memset(pixels_, (int)c, sizeof(pixels_));
}

void SimDisplay::fillRect(int x, int y, int w, int h, maze::Color c)
{
    if (w <= 0 || h <= 0) return;
    if (x < 0 || y < 0 || x + w > WIDTH || y + h > HEIGHT) {
        ++overflow_;
    }
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > WIDTH ? WIDTH : x + w;
    int y1 = y + h > HEIGHT ? HEIGHT : y + h;
    for (int py = y0; py < y1; ++py) {
        memset(pixels_ + py * WIDTH + x0, (int)c, (size_t)(x1 - x0));
    }
}

void SimDisplay::flush()
{
    ++flush_;
}

maze::Color SimDisplay::pixel(int x, int y) const
{
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return maze::Color::White;
    }
    return (maze::Color)pixels_[y * WIDTH + x];
}

int SimDisplay::countColor(maze::Color c) const
{
    int n = 0;
    for (int i = 0; i < WIDTH * HEIGHT; ++i) {
        if (pixels_[i] == (uint8_t)c) ++n;
    }
    return n;
}

bool SimDisplay::savePng(const char *path, int scale) const
{
    if (!path) return false;
    if (scale < 1) scale = 1;

    const int outW = WIDTH * scale;
    const int outH = HEIGHT * scale;

    std::vector<uint8_t> raw;
    raw.reserve((size_t)outH * (1 + (size_t)outW * 3));
    for (int py = 0; py < outH; ++py) {
        raw.push_back(0); // filtre PNG "None"
        const int sy = py / scale;
        for (int px = 0; px < outW; ++px) {
            const int sx = px / scale;
            uint8_t r, g, b;
            rgbFor((maze::Color)pixels_[sy * WIDTH + sx], r, g, b);
            raw.push_back(r);
            raw.push_back(g);
            raw.push_back(b);
        }
    }

    // Flux zlib avec blocs DEFLATE non compressés (portable, sans zlib).
    std::vector<uint8_t> zdata;
    zdata.push_back(0x78);
    zdata.push_back(0x01);
    size_t offset = 0;
    while (offset < raw.size()) {
        const size_t chunk = (raw.size() - offset > 65535u)
                                 ? 65535u : raw.size() - offset;
        const bool last = (offset + chunk >= raw.size());
        zdata.push_back(last ? 0x01 : 0x00);
        zdata.push_back((uint8_t)(chunk & 0xFF));
        zdata.push_back((uint8_t)((chunk >> 8) & 0xFF));
        zdata.push_back((uint8_t)((~chunk) & 0xFF));
        zdata.push_back((uint8_t)(((~chunk) >> 8) & 0xFF));
        zdata.insert(zdata.end(), raw.begin() + offset,
                     raw.begin() + offset + chunk);
        offset += chunk;
    }
    const uint32_t ad = adler32(raw);
    zdata.push_back((uint8_t)(ad >> 24));
    zdata.push_back((uint8_t)(ad >> 16));
    zdata.push_back((uint8_t)(ad >> 8));
    zdata.push_back((uint8_t)ad);

    std::vector<uint8_t> ihdr;
    putBE32(ihdr, (uint32_t)outW);
    putBE32(ihdr, (uint32_t)outH);
    ihdr.push_back(8);  // profondeur
    ihdr.push_back(2);  // RVB
    ihdr.push_back(0);  // compression
    ihdr.push_back(0);  // filtre
    ihdr.push_back(0);  // entrelacement

    FILE *f = fopen(path, "wb");
    if (!f) return false;

    const uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(signature, 1, 8, f);
    writeChunk(f, "IHDR", ihdr);
    writeChunk(f, "IDAT", zdata);
    writeChunk(f, "IEND", std::vector<uint8_t>());
    const bool ok = (ferror(f) == 0);
    fclose(f);
    return ok;
}

} // namespace sim
