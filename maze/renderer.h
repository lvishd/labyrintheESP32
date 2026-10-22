/*
 * renderer.h — dessin complet du menu, du labyrinthe et de l'écran de fin.
 */
#ifndef RENDERER_H
#define RENDERER_H

#include "display_iface.h"
#include "game.h"

namespace maze {
namespace renderer {

void drawCurrent(Display &d, const Game &game);
void drawMenu(Display &d, const Game &game);
void drawPlaying(Display &d, const Game &game);
void drawEnd(Display &d, const Game &game);

} // namespace renderer
} // namespace maze

#endif // RENDERER_H
