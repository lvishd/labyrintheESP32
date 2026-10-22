/*
 * display_iface.cpp — police 5 x 7 et rendu du texte via fillRect().
 *
 * La police couvre les majuscules, les chiffres et les symboles utilisés par
 * le menu et l'écran de fin. Les minuscules sont converties en majuscules.
 */
#include "display_iface.h"

#include <string.h>

namespace maze {

namespace {

const uint8_t GLYPH_SPACE[7] = {0, 0, 0, 0, 0, 0, 0};
const uint8_t GLYPH_A[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
const uint8_t GLYPH_B[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
const uint8_t GLYPH_C[7] = {0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0F};
const uint8_t GLYPH_D[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
const uint8_t GLYPH_E[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
const uint8_t GLYPH_F[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
const uint8_t GLYPH_G[7] = {0x0F, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0F};
const uint8_t GLYPH_H[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
const uint8_t GLYPH_I[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
const uint8_t GLYPH_J[7] = {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C};
const uint8_t GLYPH_K[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
const uint8_t GLYPH_L[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
const uint8_t GLYPH_M[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
const uint8_t GLYPH_N[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
const uint8_t GLYPH_O[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
const uint8_t GLYPH_P[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
const uint8_t GLYPH_Q[7] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
const uint8_t GLYPH_R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
const uint8_t GLYPH_S[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
const uint8_t GLYPH_T[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
const uint8_t GLYPH_U[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
const uint8_t GLYPH_V[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
const uint8_t GLYPH_W[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11};
const uint8_t GLYPH_X[7] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
const uint8_t GLYPH_Y[7] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
const uint8_t GLYPH_Z[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
const uint8_t GLYPH_0[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
const uint8_t GLYPH_1[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
const uint8_t GLYPH_2[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
const uint8_t GLYPH_3[7] = {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E};
const uint8_t GLYPH_4[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
const uint8_t GLYPH_5[7] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
const uint8_t GLYPH_6[7] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
const uint8_t GLYPH_7[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
const uint8_t GLYPH_8[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
const uint8_t GLYPH_9[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};
const uint8_t GLYPH_EQ[7] = {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00};
const uint8_t GLYPH_US[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F};
const uint8_t GLYPH_EXCL[7] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04};
const uint8_t GLYPH_COLON[7] = {0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00};
const uint8_t GLYPH_DOT[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04};
const uint8_t GLYPH_COMMA[7] = {0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x08};
const uint8_t GLYPH_MINUS[7] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
const uint8_t GLYPH_PLUS[7] = {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00};
const uint8_t GLYPH_SLASH[7] = {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
const uint8_t GLYPH_LPAREN[7] = {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02};
const uint8_t GLYPH_RPAREN[7] = {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08};
const uint8_t GLYPH_QUEST[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};

char upperChar(char c)
{
    if (c >= 'a' && c <= 'z') return (char)(c - 'a' + 'A');
    return c;
}

const uint8_t *glyph(char c)
{
    c = upperChar(c);
    switch (c) {
    case 'A': return GLYPH_A;
    case 'B': return GLYPH_B;
    case 'C': return GLYPH_C;
    case 'D': return GLYPH_D;
    case 'E': return GLYPH_E;
    case 'F': return GLYPH_F;
    case 'G': return GLYPH_G;
    case 'H': return GLYPH_H;
    case 'I': return GLYPH_I;
    case 'J': return GLYPH_J;
    case 'K': return GLYPH_K;
    case 'L': return GLYPH_L;
    case 'M': return GLYPH_M;
    case 'N': return GLYPH_N;
    case 'O': return GLYPH_O;
    case 'P': return GLYPH_P;
    case 'Q': return GLYPH_Q;
    case 'R': return GLYPH_R;
    case 'S': return GLYPH_S;
    case 'T': return GLYPH_T;
    case 'U': return GLYPH_U;
    case 'V': return GLYPH_V;
    case 'W': return GLYPH_W;
    case 'X': return GLYPH_X;
    case 'Y': return GLYPH_Y;
    case 'Z': return GLYPH_Z;
    case '0': return GLYPH_0;
    case '1': return GLYPH_1;
    case '2': return GLYPH_2;
    case '3': return GLYPH_3;
    case '4': return GLYPH_4;
    case '5': return GLYPH_5;
    case '6': return GLYPH_6;
    case '7': return GLYPH_7;
    case '8': return GLYPH_8;
    case '9': return GLYPH_9;
    case '=': return GLYPH_EQ;
    case '_': return GLYPH_US;
    case '!': return GLYPH_EXCL;
    case ':': return GLYPH_COLON;
    case '.': return GLYPH_DOT;
    case ',': return GLYPH_COMMA;
    case '-': return GLYPH_MINUS;
    case '+': return GLYPH_PLUS;
    case '/': return GLYPH_SLASH;
    case '(': return GLYPH_LPAREN;
    case ')': return GLYPH_RPAREN;
    case '?': return GLYPH_QUEST;
    case ' ':
    default:  return GLYPH_SPACE;
    }
}

} // namespace

void Display::drawLine(int x0, int y0, int x1, int y1,
                       int thickness, Color c)
{
    if (thickness < 1) thickness = 1;
    if (x0 == x1) {
        const int y = (y0 < y1) ? y0 : y1;
        const int h = (y0 < y1 ? y1 - y0 : y0 - y1) + 1;
        fillRect(x0 - thickness / 2, y, thickness, h, c);
        return;
    }
    if (y0 == y1) {
        const int x = (x0 < x1) ? x0 : x1;
        const int w = (x0 < x1 ? x1 - x0 : x0 - x1) + 1;
        fillRect(x, y0 - thickness / 2, w, thickness, c);
        return;
    }

    // Cas général (non utilisé par le jeu, qui trace des segments
    // horizontaux ou verticaux) : petits carrés le long de la droite.
    int dx = x1 - x0;
    int dy = y1 - y0;
    const int adx = (dx < 0) ? -dx : dx;
    const int ady = (dy < 0) ? -dy : dy;
    const int sx = (dx < 0) ? -1 : 1;
    const int sy = (dy < 0) ? -1 : 1;
    int err = adx - ady;
    for (;;) {
        fillRect(x0 - thickness / 2, y0 - thickness / 2,
                 thickness, thickness, c);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 > -ady) {
            err -= ady;
            x0 += sx;
        }
        if (e2 < adx) {
            err += adx;
            y0 += sy;
        }
    }
}

void Display::drawCircle(int cx, int cy, int diameter, int thickness,
                         Color outline, Color fill)
{
    if (diameter < 1) diameter = 1;
    if (thickness < 1) thickness = 1;
    const int r = diameter / 2;
    int innerDiameter = diameter - 2 * thickness;
    if (innerDiameter < 0) innerDiameter = 0;

    // Pour un diamètre pair, le centre du cercle est entre deux pixels :
    // cela donne exactement `diameter` pixels de large au lieu de d+1.
    const int offset2 = (diameter % 2 == 0) ? 1 : 0;

    const int xMin = (cx - r - 1 < 0) ? 0 : cx - r - 1;
    const int xMax = (cx + r + 1 > 199) ? 199 : cx + r + 1;
    const int yMin = (cy - r - 1 < 0) ? 0 : cy - r - 1;
    const int yMax = (cy + r + 1 > 199) ? 199 : cy + r + 1;

    const int outer2 = diameter * diameter;
    const int inner2 = innerDiameter * innerDiameter;

    for (int y = yMin; y <= yMax; ++y) {
        for (int x = xMin; x <= xMax; ++x) {
            const int dx2 = 2 * (x - cx) - offset2;
            const int dy2 = 2 * (y - cy) - offset2;
            const int d2 = dx2 * dx2 + dy2 * dy2;
            if (d2 > outer2) continue;
            const Color c = (d2 > inner2) ? outline : fill;
            fillRect(x, y, 1, 1, c);
        }
    }
}

int Display::textHeight(int scale)
{
    if (scale < 1) scale = 1;
    return 7 * scale;
}

int Display::textWidth(const char *text, int scale)
{
    if (!text || !*text) return 0;
    if (scale < 1) scale = 1;
    const int n = (int)strlen(text);
    return (n * 6 - 1) * scale;
}

void Display::drawText(int x, int y, const char *text,
                       Color fg, Color bg, int scale)
{
    if (!text) return;
    if (scale < 1) scale = 1;
    const int width = textWidth(text, scale);
    if (width > 0) {
        fillRect(x, y, width, textHeight(scale), bg);
    }
    int cursor = x;
    for (const char *p = text; *p; ++p) {
        const uint8_t *rows = glyph(*p);
        for (int row = 0; row < 7; ++row) {
            const uint8_t bits = rows[row];
            for (int col = 0; col < 5; ++col) {
                if (bits & (uint8_t)(1u << (4 - col))) {
                    fillRect(cursor + col * scale, y + row * scale,
                             scale, scale, fg);
                }
            }
        }
        cursor += 6 * scale;
    }
}

} // namespace maze
