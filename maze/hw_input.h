/*
 * hw_input.h — lecture bloquante du joystick (contacts à la masse).
 */
#ifndef HW_INPUT_H
#define HW_INPUT_H

#include "game.h"

namespace maze {

void initButtonHardware();

// Attend un appui et renvoie le bouton selon la priorité
// Validation > Nord > Sud > Est > Ouest.
Button waitButtonPress();

// Attend que tous les contacts soient relâchés puis laisse passer 30 ms.
void waitAllReleased();

} // namespace maze

#endif // HW_INPUT_H
