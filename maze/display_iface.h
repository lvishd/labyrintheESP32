/*
 * display_iface.h — interface de dessin abstraite, commune à l'écran réel
 * et à l'écran simulé sur PC. Le code de dessin du jeu n'existe qu'une fois.
 */
#ifndef DISPLAY_IFACE_H
#define DISPLAY_IFACE_H

#include <stdint.h>

namespace maze {

enum class Color : uint8_t {
    White = 0,
    Black = 1,
    Red = 2,
    Yellow = 3
};

class Display {
public:
    virtual ~Display() {}

    virtual void clear(Color c) = 0;
    virtual void fillRect(int x, int y, int w, int h, Color c) = 0;
    virtual void flush() = 0;

    // Ligne et cercle sont construits uniquement avec fillRect() : le rendu
    // est ainsi rigoureusement le même sur la carte et dans la simulation.
    void drawLine(int x0, int y0, int x1, int y1, int thickness, Color c);
    void drawCircle(int cx, int cy, int diameter, int thickness,
                    Color outline, Color fill);

    // Texte 5 x 7 agrandi par un facteur entier. Le fond du texte est rempli.
    void drawText(int x, int y, const char *text,
                  Color fg, Color bg, int scale = 1);

    static int textWidth(const char *text, int scale = 1);
    static int textHeight(int scale = 1);
};

} // namespace maze

#endif // DISPLAY_IFACE_H
