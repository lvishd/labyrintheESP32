/*
 * hw_input.cpp — joystick cinq contacts, actif à l'état bas.
 *
 * Priorité en cas d'appuis simultanés : Validation, Nord, Sud, Est, Ouest.
 * Aucun anti-rebond élaboré : le rafraîchissement e-paper (15 à 20 s) filtre
 * les rebonds ; waitAllReleased() évite seulement les actions doublées.
 */
#include "hw_input.h"

#include <Arduino.h>

#include "hardware_config.h"

namespace maze {

namespace {

struct PinButton {
    int pin;
    Button button;
};

// Ordre = ordre de priorité.
const PinButton kPins[] = {
    { JOY_PIN_CENTER, Button::Center },
    { JOY_PIN_NORTH,  Button::North  },
    { JOY_PIN_SOUTH,  Button::South  },
    { JOY_PIN_EAST,   Button::East   },
    { JOY_PIN_WEST,   Button::West   }
};
constexpr int kPinCount = (int)(sizeof(kPins) / sizeof(kPins[0]));

bool pressed(int pin)
{
    return digitalRead(pin) == LOW;
}

} // namespace

void initButtonHardware()
{
    for (int i = 0; i < kPinCount; ++i) {
        pinMode(kPins[i].pin, INPUT_PULLUP);
    }
}

Button waitButtonPress()
{
    for (;;) {
        for (int i = 0; i < kPinCount; ++i) {
            if (pressed(kPins[i].pin)) return kPins[i].button;
        }
        delay(5);
    }
}

void waitAllReleased()
{
    for (;;) {
        bool any = false;
        for (int i = 0; i < kPinCount; ++i) {
            if (pressed(kPins[i].pin)) {
                any = true;
                break;
            }
        }
        if (!any) break;
        delay(5);
    }
    delay(30); // marge de relâchement (~30 ms)
}

} // namespace maze
