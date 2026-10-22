/*
 * hw_epd.h — afficheur e-paper réel, implémentation de Display.
 */
#ifndef HW_EPD_H
#define HW_EPD_H

#include "display_iface.h"

namespace maze {

class EpdDisplay : public Display {
public:
    EpdDisplay();

    void clear(Color c) override;
    void fillRect(int x, int y, int w, int h, Color c) override;
    void flush() override;

private:
    void setPixel(int x, int y, Color c);
    static uint8_t epdColor(Color c);

    // Tampon 200 x 200 en 2 bits par pixel = 10 000 octets.
    uint8_t image_[10000];
};

// Initialise le SPI logiciel et le panneau, puis efface l'écran en blanc.
// Bloquant pendant le premier rafraîchissement (environ 15 à 20 s).
void initDisplayHardware();

} // namespace maze

#endif // HW_EPD_H
