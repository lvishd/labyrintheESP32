/*
 * renderer.cpp — ordre de dessin : fond, cadre, structure, trajet rouge,
 * segment jaune, marqueurs, joueur. Tout est redessiné depuis l'état en
 * mémoire, sans rafraîchissement partiel.
 */
#include "renderer.h"

#include <stdio.h>
#include <string.h>

namespace maze {
namespace renderer {

namespace {

constexpr int THICK_PATH = 5;
constexpr int THICK_WALL = 3;
constexpr int FRAME = 2;

void drawFrameAndStructure(Display &d, const Maze &m, const Params &p)
{
    d.clear(Color::White);

    // Cadre noir de 2 px à l'intérieur des 200 x 200.
    d.fillRect(0, 0, SCREEN_W, FRAME, Color::Black);
    d.fillRect(0, SCREEN_H - FRAME, SCREEN_W, FRAME, Color::Black);
    d.fillRect(0, 0, FRAME, SCREEN_H, Color::Black);
    d.fillRect(SCREEN_W - FRAME, 0, FRAME, SCREEN_H, Color::Black);

    int X[MAX_N + 1], Y[MAX_N + 1];
    computeCellCoordinates(m.nX, m.nY, X, Y);

    // Interruption blanche du cadre sur le ou les côtés extérieurs de la
    // cellule de sortie.
    const uint8_t exitEdges = cellEdgeSet(m.exitX, m.exitY, m.nX, m.nY);
    const int ex0 = X[m.exitX];
    const int ex1 = X[m.exitX + 1];
    const int ey0 = Y[m.exitY];
    const int ey1 = Y[m.exitY + 1];
    if (exitEdges & EDGE_LEFT) {
        d.fillRect(0, ey0, FRAME, ey1 - ey0, Color::White);
    }
    if (exitEdges & EDGE_RIGHT) {
        d.fillRect(SCREEN_W - FRAME, ey0, FRAME, ey1 - ey0, Color::White);
    }
    if (exitEdges & EDGE_TOP) {
        d.fillRect(ex0, 0, ex1 - ex0, FRAME, Color::White);
    }
    if (exitEdges & EDGE_BOTTOM) {
        d.fillRect(ex0, SCREEN_H - FRAME, ex1 - ex0, FRAME, Color::White);
    }

    if (p.typeAff == 0) {
        // Chaque connexion ouverte est un trait noir de 5 px.
        for (int y = 0; y < m.nY; ++y) {
            for (int x = 0; x < m.nX; ++x) {
                int cx, cy, nx, ny;
                cellCenter(x, y, X, Y, cx, cy);
                if ((m.open[y][x] & (1u << EAST)) && x + 1 < m.nX) {
                    cellCenter(x + 1, y, X, Y, nx, ny);
                    d.drawLine(cx, cy, nx, ny, THICK_PATH, Color::Black);
                }
                if ((m.open[y][x] & (1u << SOUTH)) && y + 1 < m.nY) {
                    cellCenter(x, y + 1, X, Y, nx, ny);
                    d.drawLine(cx, cy, nx, ny, THICK_PATH, Color::Black);
                }
            }
        }

        // Voie(s) de sortie jusqu'au bord de l'écran.
        int cx, cy;
        cellCenter(m.exitX, m.exitY, X, Y, cx, cy);
        if (exitEdges & EDGE_LEFT) {
            d.drawLine(cx, cy, 0, cy, THICK_PATH, Color::Black);
        }
        if (exitEdges & EDGE_RIGHT) {
            d.drawLine(cx, cy, SCREEN_W - 1, cy, THICK_PATH, Color::Black);
        }
        if (exitEdges & EDGE_TOP) {
            d.drawLine(cx, cy, cx, 0, THICK_PATH, Color::Black);
        }
        if (exitEdges & EDGE_BOTTOM) {
            d.drawLine(cx, cy, cx, SCREEN_H - 1, THICK_PATH, Color::Black);
        }
    } else {
        // Chaque frontière sans connexion est un mur noir de 3 px.
        for (int y = 0; y < m.nY; ++y) {
            for (int x = 0; x < m.nX; ++x) {
                if (x + 1 < m.nX && !(m.open[y][x] & (1u << EAST))) {
                    d.drawLine(X[x + 1], Y[y], X[x + 1], Y[y + 1] - 1,
                               THICK_WALL, Color::Black);
                }
                if (y + 1 < m.nY && !(m.open[y][x] & (1u << SOUTH))) {
                    d.drawLine(X[x], Y[y + 1], X[x + 1] - 1, Y[y + 1],
                               THICK_WALL, Color::Black);
                }
            }
        }
    }
}

void drawPath(Display &d, const Maze &m, int prevX, int prevY,
              int curX, int curY, bool pending)
{
    int X[MAX_N + 1], Y[MAX_N + 1];
    computeCellCoordinates(m.nX, m.nY, X, Y);

    // Trajet validé (rouge), une seule fois par connexion.
    for (int y = 0; y < m.nY; ++y) {
        for (int x = 0; x < m.nX; ++x) {
            int cx, cy, nx, ny;
            cellCenter(x, y, X, Y, cx, cy);
            if ((m.red[y][x] & (1u << EAST)) && x + 1 < m.nX) {
                cellCenter(x + 1, y, X, Y, nx, ny);
                d.drawLine(cx, cy, nx, ny, THICK_PATH, Color::Red);
            }
            if ((m.red[y][x] & (1u << SOUTH)) && y + 1 < m.nY) {
                cellCenter(x, y + 1, X, Y, nx, ny);
                d.drawLine(cx, cy, nx, ny, THICK_PATH, Color::Red);
            }
        }
    }

    // Un seul segment provisoire jaune, toujours au-dessus du rouge.
    if (pending) {
        int cx0, cy0, cx1, cy1;
        cellCenter(prevX, prevY, X, Y, cx0, cy0);
        cellCenter(curX, curY, X, Y, cx1, cy1);
        d.drawLine(cx0, cy0, cx1, cy1, THICK_PATH, Color::Yellow);
    }
}

void drawMarkerRect(Display &d, int cx, int cy, int cw, int ch, Color c)
{
    int w = cw / 2;
    int h = ch / 2;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    d.fillRect(cx - w / 2, cy - h / 2, w, h, c);
}

void drawMarkers(Display &d, const Maze &m, bool regenFailed)
{
    int X[MAX_N + 1], Y[MAX_N + 1];
    computeCellCoordinates(m.nX, m.nY, X, Y);

    int scx, scy, ecx, ecy;
    cellCenter(m.entryX, m.entryY, X, Y, scx, scy);
    cellCenter(m.exitX, m.exitY, X, Y, ecx, ecy);
    const int scw = X[m.entryX + 1] - X[m.entryX];
    const int sch = Y[m.entryY + 1] - Y[m.entryY];
    const int ecw = X[m.exitX + 1] - X[m.exitX];
    const int ech = Y[m.exitY + 1] - Y[m.exitY];

    // Départ : noir plein, ou noir avec intérieur rouge si la dernière
    // régénération a échoué.
    drawMarkerRect(d, scx, scy, scw, sch, Color::Black);
    if (regenFailed) {
        int w = scw / 2;
        int h = sch / 2;
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        int iw = w / 2;
        int ih = h / 2;
        if (iw < 1) iw = 1;
        if (ih < 1) ih = 1;
        d.fillRect(scx - iw / 2, scy - ih / 2, iw, ih, Color::Red);
    }

    // Arrivée : rouge plein.
    drawMarkerRect(d, ecx, ecy, ecw, ech, Color::Red);
}

void drawPlayer(Display &d, const Maze &m, int x, int y, bool pending)
{
    int X[MAX_N + 1], Y[MAX_N + 1];
    computeCellCoordinates(m.nX, m.nY, X, Y);

    int cx, cy;
    cellCenter(x, y, X, Y, cx, cy);
    const int cw = X[x + 1] - X[x];
    const int ch = Y[y + 1] - Y[y];
    const int minSide = (cw < ch) ? cw : ch;
    int diameter = (minSide * 8 + 5) / 10;
    if (diameter < 1) diameter = 1;
    int thickness = (diameter + 2) / 4;
    if (thickness < 1) thickness = 1;

    // Sur les petites cellules des bords, le cercle de 80 % atteindrait le
    // cadre de 2 px. On le décale d'au plus 1 px vers l'intérieur pour que le
    // cadre reste complet, sans changer son diamètre.
    const int r = diameter / 2;
    const int minCenter = 2 + r - ((diameter % 2 == 0) ? 1 : 0);
    const int maxCenter = (SCREEN_W - 1 - 2) - r;
    if (cx < minCenter) cx = minCenter;
    if (cx > maxCenter) cx = maxCenter;
    if (cy < minCenter) cy = minCenter;
    if (cy > maxCenter) cy = maxCenter;

    const Color outline = pending ? Color::Yellow : Color::Red;
    const Color fill = pending ? Color::Red : Color::Yellow;
    d.drawCircle(cx, cy, diameter, thickness, outline, fill);
}

} // namespace

void drawMenu(Display &d, const Game &game)
{
    d.clear(Color::White);

    const Params &p = game.parameters();
    for (int i = 0; i < MENU_FIELD_COUNT; ++i) {
        const MenuField f = (MenuField)i;
        const int row = i % 6;
        const int col = i / 6;
        const int x = col * 100 + 2;
        const int y = row * 33 + 1;
        const int w = 96;
        const int h = 31;
        const bool selected = (game.menuRow() == i);

        if (selected) d.fillRect(x, y, w, h, Color::Black);

        char text[32];
        if (f == MenuField::START) {
            snprintf(text, sizeof(text), "START");
        } else {
            snprintf(text, sizeof(text), "%s=%d", fieldLabel(f),
                     fieldValue(p, f));
        }

        const Color fg = selected ? Color::White : Color::Black;
        const Color bg = selected ? Color::Black : Color::White;
        int tx = x + 4;
        if (f == MenuField::START) {
            tx = x + (w - Display::textWidth(text, 1)) / 2;
        }
        d.drawText(tx, y + 12, text, fg, bg, 1);
    }

    if (game.menuError()) {
        const char *msg = "ERREUR PARAM";
        const int tw = Display::textWidth(msg, 1);
        const int bx = (SCREEN_W - tw - 12) / 2;
        d.fillRect(bx, 88, tw + 12, 18, Color::Red);
        d.drawText((SCREEN_W - tw) / 2, 93, msg, Color::White,
                   Color::Red, 1);
    }
}

void drawPlaying(Display &d, const Game &game)
{
    const Maze &m = game.maze();
    const Params &p = game.parameters();

    drawFrameAndStructure(d, m, p);
    drawPath(d, m, game.previousX(), game.previousY(),
             game.playerX(), game.playerY(), game.hasPending());
    drawMarkers(d, m, game.regenerationFailed());
    drawPlayer(d, m, game.playerX(), game.playerY(), game.hasPending());
}

void drawEnd(Display &d, const Game &game)
{
    d.clear(Color::White);
    d.fillRect(0, 0, SCREEN_W, FRAME, Color::Black);
    d.fillRect(0, SCREEN_H - FRAME, SCREEN_W, FRAME, Color::Black);
    d.fillRect(0, 0, FRAME, SCREEN_H, Color::Black);
    d.fillRect(SCREEN_W - FRAME, 0, FRAME, SCREEN_H, Color::Black);

    const char *title = "SORTIE !";
    const int titleScale = 3;
    const int titleW = Display::textWidth(title, titleScale);
    d.drawText((SCREEN_W - titleW) / 2, 38, title, Color::Red,
               Color::White, titleScale);

    const uint32_t totalSeconds = game.elapsedMs() / 1000u;
    const unsigned long minutes = totalSeconds / 60u;
    const unsigned long seconds = totalSeconds % 60u;
    char timeText[24];
    snprintf(timeText, sizeof(timeText), "%02lu:%02lu", minutes, seconds);
    const int timeScale = 3;
    const int timeW = Display::textWidth(timeText, timeScale);
    d.drawText((SCREEN_W - timeW) / 2, 92, timeText, Color::Black,
               Color::White, timeScale);

    const char *hint = "VALIDATION = MENU";
    const int hintW = Display::textWidth(hint, 1);
    d.fillRect((SCREEN_W - hintW) / 2 - 5, 145, hintW + 10, 13,
               Color::Yellow);
    d.drawText((SCREEN_W - hintW) / 2, 148, hint, Color::Black,
               Color::Yellow, 1);
}

void drawCurrent(Display &d, const Game &game)
{
    switch (game.state()) {
    case UiState::Menu:
        drawMenu(d, game);
        break;
    case UiState::Playing:
        drawPlaying(d, game);
        break;
    case UiState::End:
        drawEnd(d, game);
        break;
    default:
        d.clear(Color::White);
        break;
    }
}

} // namespace renderer
} // namespace maze
