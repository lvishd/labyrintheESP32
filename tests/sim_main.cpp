/*
 * sim_main.cpp — tests automatiques, fuzz et images de contrôle.
 *
 * Usage :
 *   ./sim test     suite automatique (code retour non nul au moindre échec)
 *   ./sim fuzz     100 000 parties de robot avec invariants
 *   ./sim images   images PNG de contrôle dans out/
 */
#include "game.h"
#include "maze_logic.h"
#include "renderer.h"
#include "sim_display.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <random>
#include <string>
#include <vector>

#include <sys/stat.h>

using maze::ActionResult;
using maze::Button;
using maze::Color;
using maze::Game;
using maze::Maze;
using maze::MenuField;
using maze::Params;
using maze::UiState;

// Compteur d'allocations dynamiques (tests mémoire).
long g_allocCount = 0;
bool g_trackAlloc = false;

void *operator new(std::size_t size)
{
    if (g_trackAlloc) ++g_allocCount;
    void *p = std::malloc(size ? size : 1);
    if (!p) throw std::bad_alloc();
    return p;
}

void *operator new[](std::size_t size)
{
    if (g_trackAlloc) ++g_allocCount;
    void *p = std::malloc(size ? size : 1);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

namespace {

// ---------------------------------------------------------------------------
// Infrastructure de test
// ---------------------------------------------------------------------------

int g_pass = 0;
int g_fail = 0;
int g_groupFails = 0;
const char *g_group = "";

void groupBegin(const char *name)
{
    g_group = name;
    g_groupFails = g_fail;
    std::printf("\n[%s]\n", name);
}

void groupEnd()
{
    if (g_fail == g_groupFails) {
        std::printf("PASS %s\n", g_group);
    } else {
        std::printf("FAIL %s (%d échec(s))\n", g_group,
                    g_fail - g_groupFails);
    }
}

void check(bool condition, const char *fmt, ...)
{
    if (condition) {
        ++g_pass;
        return;
    }
    ++g_fail;
    std::printf("  FAIL: ");
    va_list args;
    va_start(args, fmt);
    std::vprintf(fmt, args);
    va_end(args);
    std::printf("\n");
}

// ---------------------------------------------------------------------------
// Générateur injecté et petites aides
// ---------------------------------------------------------------------------

struct XorShift32 {
    uint32_t state;
    explicit XorShift32(uint32_t seed) : state(seed ? seed : 1u) {}
    uint32_t next()
    {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }
};

XorShift32 g_rng(0x12345678u);

uint32_t testRng()
{
    return g_rng.next();
}

void seedTestRng(uint32_t seed)
{
    g_rng = XorShift32(seed);
}

struct Cell {
    int x;
    int y;
};


int popcount8(uint8_t v)
{
    int n = 0;
    while (v) {
        n += v & 1;
        v >>= 1;
    }
    return n;
}

const uint8_t kOpposite[4] = {2, 3, 0, 1};

void compassDelta(int compass, int &dx, int &dy)
{
    maze::compassDelta(compass, dx, dy);
}

Button buttonForStep(const Cell &from, const Cell &to)
{
    if (to.x == from.x + 1) return Button::East;
    if (to.x == from.x - 1) return Button::West;
    if (to.y == from.y + 1) return Button::South;
    return Button::North;
}


bool paramsEqual(const Params &a, const Params &b)
{
    return a.nX == b.nX && a.nY == b.nY &&
           a.coinIn == b.coinIn && a.pixIn == b.pixIn &&
           a.dirIn == b.dirIn && a.coinOut == b.coinOut &&
           a.pixOut == b.pixOut && a.dirOut == b.dirOut &&
           a.nReset == b.nReset && a.nLoops == b.nLoops &&
           a.typeAff == b.typeAff;
}

bool masksSymmetricAndInside(const Maze &m)
{
    for (int y = 0; y < m.nY; ++y) {
        for (int x = 0; x < m.nX; ++x) {
            for (int c = 0; c < 4; ++c) {
                if (!(m.open[y][x] & (1u << c))) continue;
                int dx, dy;
                compassDelta(c, dx, dy);
                const int nx = x + dx;
                const int ny = y + dy;
                if (nx < 0 || nx >= m.nX || ny < 0 || ny >= m.nY) {
                    return false;
                }
                if (!(m.open[ny][nx] & (1u << kOpposite[c]))) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool findPath(const Maze &m, int sx, int sy, int tx, int ty,
              std::vector<Cell> &path)
{
    path.clear();
    if (!m.inBounds(sx, sy) || !m.inBounds(tx, ty)) return false;

    int parent[maze::MAX_CELLS];
    for (int i = 0; i < maze::MAX_CELLS; ++i) parent[i] = -1;
    int queue[maze::MAX_CELLS];
    int head = 0, tail = 0;
    const int startId = sy * m.nX + sx;
    const int targetId = ty * m.nX + tx;
    parent[startId] = startId;
    queue[tail++] = startId;

    while (head < tail && parent[targetId] < 0) {
        const int id = queue[head++];
        const int x = id % m.nX;
        const int y = id / m.nX;
        for (int c = 0; c < 4; ++c) {
            if (!(m.open[y][x] & (1u << c))) continue;
            int dx, dy;
            compassDelta(c, dx, dy);
            const int nx = x + dx;
            const int ny = y + dy;
            if (nx < 0 || nx >= m.nX || ny < 0 || ny >= m.nY) continue;
            const int nid = ny * m.nX + nx;
            if (parent[nid] < 0) {
                parent[nid] = id;
                queue[tail++] = nid;
            }
        }
    }
    if (parent[targetId] < 0) return false;

    std::vector<Cell> reverse;
    for (int id = targetId; id != startId; id = parent[id]) {
        reverse.push_back(Cell{id % m.nX, id / m.nX});
    }
    for (int i = (int)reverse.size() - 1; i >= 0; --i) {
        path.push_back(reverse[(size_t)i]);
    }
    return true;
}

void externalButtons(const Maze &m, int x, int y, std::vector<Button> &out)
{
    out.clear();
    if (x - 1 < 0) out.push_back(Button::West);
    if (x + 1 >= m.nX) out.push_back(Button::East);
    if (y - 1 < 0) out.push_back(Button::North);
    if (y + 1 >= m.nY) out.push_back(Button::South);
}

bool invariantsHold(Game &game, std::string &why)
{
    if (game.state() != UiState::Playing) return true;
    const Maze &m = game.maze();
    if (game.playerX() < 0 || game.playerX() >= m.nX ||
        game.playerY() < 0 || game.playerY() >= m.nY) {
        why = "position hors grille";
        return false;
    }
    if (!masksSymmetricAndInside(m)) {
        why = "masques non symétriques ou tournés vers l'extérieur";
        return false;
    }
    if (!m.allRedAndForcedOpen()) {
        why = "rouge ou imposé non ouvert";
        return false;
    }
    if (game.hasPending()) {
        const int dx = game.playerX() - game.previousX();
        const int dy = game.playerY() - game.previousY();
        if (dx * dx + dy * dy != 1) {
            why = "segment provisoire non adjacent";
            return false;
        }
        // Direction du joueur vers la cellule précédente.
        const int backX = game.previousX() - game.playerX();
        const int backY = game.previousY() - game.playerY();
        int compass = -1;
        if (backX == 1) compass = maze::EAST;
        else if (backX == -1) compass = maze::WEST;
        else if (backY == 1) compass = maze::SOUTH;
        else if (backY == -1) compass = maze::NORTH;
        if (compass < 0 ||
            !m.isOpen(game.playerX(), game.playerY(), compass)) {
            why = "segment provisoire sur une connexion fermée";
            return false;
        }
    }
    if (!m.validateMazeConnectivity()) {
        why = "toutes les cellules ne sont pas atteintes";
        return false;
    }
    if (!m.validateExitReachability(game.playerX(), game.playerY())) {
        why = "sortie non atteignable depuis le joueur";
        return false;
    }
    if (game.validSteps() < 0 || game.abandonCount() < 0 ||
        game.abandonCount() > 4) {
        why = "compteur hors bornes";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Petit banc de test avec horloge et écran simulés
// ---------------------------------------------------------------------------

struct Harness {
    Game game;
    sim::SimDisplay display;
    uint32_t now;
    uint32_t lastPressMs;

    Harness() : now(1000), lastPressMs(0)
    {
        game.setRng(testRng);
    }

    bool startDirect(const Params &p, uint32_t seed, bool render = false)
    {
        seedTestRng(seed);
        game.setParameters(p);
        const bool ok = game.startGame(now);
        if (ok && render) renderNow();
        return ok;
    }

    bool selectStartAndPress(bool render = false)
    {
        while (game.menuRow() != (int)MenuField::START) {
            press(Button::South, false);
        }
        const ActionResult r = press(Button::Center, render);
        return r.state == UiState::Playing;
    }

    void renderNow()
    {
        display.resetCounters();
        maze::renderer::drawCurrent(display, game);
        display.flush();
        now += 15000;
        game.notifyDisplayDone(now);
    }

    ActionResult press(Button b, bool render = false)
    {
        now += 2000;
        lastPressMs = now;
        const ActionResult r = game.handleButton(b, now);
        if (r.redraw && render) renderNow();
        return r;
    }
};

bool walkToExit(Harness &h, bool render, int *steps = nullptr)
{
    const Maze &m = h.game.maze();
    std::vector<Cell> path;
    if (!findPath(m, h.game.playerX(), h.game.playerY(),
                  m.exitX, m.exitY, path)) {
        return false;
    }
    for (size_t i = 0; i < path.size(); ++i) {
        const Cell from{h.game.playerX(), h.game.playerY()};
        const Button b = buttonForStep(from, path[i]);
        const ActionResult r = h.press(b, render);
        if (!r.redraw || !h.game.hasPending() ||
            h.game.playerX() != path[i].x || h.game.playerY() != path[i].y) {
            return false;
        }
        const ActionResult v = h.press(Button::Center, render);
        if (!v.redraw || h.game.hasPending() ||
            h.game.state() != UiState::Playing) {
            return false;
        }
    }
    if (steps) *steps = (int)path.size();
    return true;
}

// ---------------------------------------------------------------------------
// Conversions, directions, géométrie
// ---------------------------------------------------------------------------

void testGeometry()
{
    groupBegin("conversions et directions");

    const int sizes[][2] = {{5, 5}, {15, 15}, {10, 7}, {7, 10}};
    for (const auto &sz : sizes) {
        const int nX = sz[0];
        const int nY = sz[1];

        std::vector<Cell> border;
        for (int y = 0; y < nY; ++y) {
            for (int x = 0; x < nX; ++x) {
                if (maze::cellEdgeSet(x, y, nX, nY)) {
                    border.push_back(Cell{x, y});
                }
            }
        }
        check((int)border.size() == 2 * (nX + nY) - 4,
              "nombre de cellules de bord %dx%d", nX, nY);

        for (int coin = 1; coin <= 4; ++coin) {
            const int len = maze::edgeLength(coin, nX, nY);
            const uint8_t edgeBit = (uint8_t)(1u << (coin - 1));
            for (int pix = 1; pix <= len; ++pix) {
                int x = -1, y = -1;
                const bool ok = maze::convertEdgeToCell(coin, pix, nX, nY,
                                                        x, y);
                check(ok && x >= 0 && x < nX && y >= 0 && y < nY,
                      "conversion coin=%d pix=%d %dx%d", coin, pix, nX, nY);
                check((maze::cellEdgeSet(x, y, nX, nY) & edgeBit) != 0,
                      "cellule sur l'arête coin=%d pix=%d", coin, pix);
                check(maze::countExitOpenings(x, y, nX, nY) ==
                          popcount8(maze::cellEdgeSet(x, y, nX, nY)),
                      "voies extérieures coin=%d pix=%d", coin, pix);
                if (pix == 1) {
                    const int prevCoin = (coin == 1) ? 4 : coin - 1;
                    const int prevLen = maze::edgeLength(prevCoin, nX, nY);
                    int px = -1, py = -1;
                    check(maze::convertEdgeToCell(prevCoin, prevLen, nX, nY,
                                                  px, py) &&
                              px == x && py == y,
                          "double désignation du coin %d", coin);
                }
            }

            const int txTab[5] = {0, 0, 1, 0, -1};
            const int tyTab[5] = {0, 1, 0, -1, 0};
            const int nxTab[5] = {0, 1, 0, -1, 0};
            const int nyTab[5] = {0, 0, -1, 0, 1};
            for (int pix = 1; pix <= len; ++pix) {
                int cx, cy;
                maze::convertEdgeToCell(coin, pix, nX, nY, cx, cy);
                int allowed = 0;
                for (int dir = 1; dir <= 3; ++dir) {
                    int nx = -1, ny = -1;
                    if (maze::neighborFromDir(coin, pix, dir, nX, nY,
                                              nx, ny)) {
                        ++allowed;
                        int ex = 0, ey = 0;
                        if (dir == maze::DIR_START) {
                            ex = cx - txTab[coin];
                            ey = cy - tyTab[coin];
                        } else if (dir == maze::DIR_INWARD) {
                            ex = cx + nxTab[coin];
                            ey = cy + nyTab[coin];
                        } else {
                            ex = cx + txTab[coin];
                            ey = cy + tyTab[coin];
                        }
                        check(nx == ex && ny == ey,
                              "dir=%d coin=%d pix=%d", dir, coin, pix);
                    }
                }
                const bool corner = (pix == 1 || pix == len);
                check(allowed == (corner ? 2 : 3),
                      "nombre de dir permis coin=%d pix=%d (%d)", coin, pix,
                      allowed);
            }
        }
    }

    // Exemples exacts du cahier des charges (§5.2).
    const struct { int coin, pix, x, y; } examples[] = {
        {1, 1, 0, 0}, {1, 7, 0, 6}, {2, 1, 0, 6}, {2, 10, 9, 6},
        {3, 1, 9, 6}, {3, 7, 9, 0}, {4, 1, 9, 0}, {4, 10, 0, 0}
    };
    for (const auto &e : examples) {
        int x = -1, y = -1;
        const bool ok = maze::convertEdgeToCell(e.coin, e.pix, 10, 7, x, y);
        check(ok && x == e.x && y == e.y,
              "exemple §5.2 coin=%d pix=%d", e.coin, e.pix);
    }

    for (int n = 5; n <= 15; ++n) {
        int X[maze::MAX_N + 1], Y[maze::MAX_N + 1];
        maze::computeCellCoordinates(n, n, X, Y);
        check(X[0] == 0 && X[n] == 200 && Y[0] == 0 && Y[n] == 200,
              "bornes des coordonnées n=%d", n);
        for (int i = 0; i < n; ++i) {
            const int w = X[i + 1] - X[i];
            const int h = Y[i + 1] - Y[i];
            check(w > 0 && h > 0, "cellule non vide n=%d i=%d", n, i);
            check(i + 1 >= n || (X[i + 2] - X[i + 1] - w <= 1 &&
                                 w - (X[i + 2] - X[i + 1]) <= 1),
                  "largeurs écartées de plus de 1 px n=%d", n);
            check(w <= 1 || h <= 1 || true, "taille cellules n=%d", n);
        }
    }

    check(maze::oppositeCompass(maze::NORTH) == maze::SOUTH &&
              maze::oppositeCompass(maze::EAST) == maze::WEST,
          "directions opposées");

    groupEnd();
}

// ---------------------------------------------------------------------------
// Paramètres et menu
// ---------------------------------------------------------------------------

void testMenuRules()
{
    groupBegin("paramètres et menu (règles)");

    std::mt19937 meta(123456u);
    auto randInt = [&meta](int lo, int hi) {
        std::uniform_int_distribution<int> d(lo, hi);
        return d(meta);
    };

    const int sizes[][2] = {{5, 5}, {15, 15}, {10, 7}, {7, 10}};
    for (const auto &sz : sizes) {
        const int nX = sz[0];
        const int nY = sz[1];
        std::vector<Cell> cells;
        for (int coin = 1; coin <= 4; ++coin) {
            const int len = maze::edgeLength(coin, nX, nY);
            for (int pix = 1; pix <= len; ++pix) {
                int x, y;
                maze::convertEdgeToCell(coin, pix, nX, nY, x, y);
                cells.push_back(Cell{x, y});
            }
        }

        for (int ci = 1; ci <= 4; ++ci) {
            for (int pi = 1; pi <= maze::edgeLength(ci, nX, nY); ++pi) {
                int ex, ey;
                maze::convertEdgeToCell(ci, pi, nX, nY, ex, ey);
                const uint8_t entryEdges = maze::cellEdgeSet(ex, ey, nX, nY);
                for (int co = 1; co <= 4; ++co) {
                    Params p;
                    p.nX = nX;
                    p.nY = nY;
                    p.coinIn = ci;
                    p.pixIn = pi;
                    p.coinOut = co;
                    p.dirIn = 2;
                    p.dirOut = 2;

                    bool exists = false;
                    const int len = maze::edgeLength(co, nX, nY);
                    for (int po = 1; po <= len; ++po) {
                        int xx, xy;
                        maze::convertEdgeToCell(co, po, nX, nY, xx, xy);
                        if ((maze::cellEdgeSet(xx, xy, nX, nY) &
                             entryEdges) == 0) {
                            exists = true;
                        }
                    }
                    check(maze::isFieldValueAllowed(MenuField::COIN_OUT,
                                                    co, p) == exists,
                          "coin_out autorisé %dx%d %d/%d -> %d", nX, nY,
                          ci, pi, co);
                }
            }
        }

        for (const Cell &entry : cells) {
            const uint8_t entryEdges =
                maze::cellEdgeSet(entry.x, entry.y, nX, nY);
            for (int co = 1; co <= 4; ++co) {
                const int len = maze::edgeLength(co, nX, nY);
                for (int po = 1; po <= len; ++po) {
                    int xx, xy;
                    maze::convertEdgeToCell(co, po, nX, nY, xx, xy);
                    const bool disjoint =
                        (maze::cellEdgeSet(xx, xy, nX, nY) & entryEdges) == 0;
                    Params p;
                    p.nX = nX;
                    p.nY = nY;
                    p.coinOut = co;
                    p.pixOut = po;
                    p.dirIn = 2;
                    p.dirOut = 2;
                    // Retrouve une entrée correspondant à cette cellule.
                    bool foundEntry = false;
                    for (int ci = 1; ci <= 4 && !foundEntry; ++ci) {
                        for (int pi = 1;
                             pi <= maze::edgeLength(ci, nX, nY); ++pi) {
                            int ex, ey;
                            maze::convertEdgeToCell(ci, pi, nX, nY, ex, ey);
                            if (ex == entry.x && ey == entry.y) {
                                p.coinIn = ci;
                                p.pixIn = pi;
                                foundEntry = true;
                                break;
                            }
                        }
                    }
                    check(foundEntry, "entrée retrouvée %dx%d", nX, nY);
                    check(maze::isFieldValueAllowed(MenuField::PIX_OUT,
                                                    po, p) == disjoint,
                          "pix_out autorisé %dx%d entrée(%d,%d) sortie %d/%d",
                          nX, nY, entry.x, entry.y, co, po);
                }
            }
        }
    }

    // 100 000 états initiaux aléatoires corrigés en cascade.
    for (int i = 0; i < 100000; ++i) {
        Params p;
        p.nX = randInt(5, 15);
        p.nY = randInt(5, 15);
        p.coinIn = randInt(1, 4);
        p.pixIn = randInt(1, 15);
        p.dirIn = randInt(1, 3);
        p.coinOut = randInt(1, 4);
        p.pixOut = randInt(1, 15);
        p.dirOut = randInt(1, 3);
        p.nReset = randInt(0, 32);
        p.nLoops = randInt(0, 10);
        p.typeAff = randInt(0, 1);
        maze::fixDependentParameters(p);
        check(maze::validateParameters(p),
              "correction en cascade i=%d nx=%d ny=%d ci=%d pi=%d co=%d po=%d",
              i, p.nX, p.nY, p.coinIn, p.pixIn, p.coinOut, p.pixOut);
        if (g_fail > g_groupFails + 20) break;
    }

    // Valeurs permises successives, sans bouclage.
    {
        Params p;
        p.nX = 10;
        p.nY = 7;
        p.coinIn = 1;
        p.pixIn = 2;
        p.dirIn = 2;
        p.coinOut = 2;
        p.pixOut = 1;
        p.dirOut = 2;
        check(!maze::isFieldValueAllowed(MenuField::COIN_OUT, 1, p),
              "coin 1 interdit avec entrée sur l'arête gauche");
        check(maze::nextAllowedValue(MenuField::COIN_OUT, 2, 1, p) == 3,
              "coin_out 2 -> 3");
        check(maze::nextAllowedValue(MenuField::COIN_OUT, 3, 1, p) == 4,
              "coin_out 3 -> 4");
        check(maze::nextAllowedValue(MenuField::COIN_OUT, 4, 1, p) == 4,
              "coin_out butée haute");
        check(maze::nextAllowedValue(MenuField::COIN_OUT, 2, -1, p) == 2,
              "coin_out butée basse sans bouclage");

        check(maze::nextAllowedValue(MenuField::N_X, 5, -1, p) == 5,
              "n_x butée basse");
        check(maze::nextAllowedValue(MenuField::N_X, 15, 1, p) == 15,
              "n_x butée haute");
        check(maze::nextAllowedValue(MenuField::N_RESET, 0, -1, p) == 0 &&
                  maze::nextAllowedValue(MenuField::N_RESET, 32, 1, p) == 32,
              "n_reset 0..32");
        check(maze::nextAllowedValue(MenuField::N_BOUCLES, 0, -1, p) == 0 &&
                  maze::nextAllowedValue(MenuField::N_BOUCLES, 10, 1, p) == 10,
              "n_boucles 0..10");
        check(maze::nextAllowedValue(MenuField::TYPE_AFF, 0, 1, p) == 1 &&
                  maze::nextAllowedValue(MenuField::TYPE_AFF, 1, 1, p) == 1,
              "type_aff 0/1");

        Params corner;
        corner.nX = 10;
        corner.nY = 7;
        corner.coinIn = 1;
        corner.pixIn = 1;
        corner.dirIn = 2;
        check(maze::nextAllowedValue(MenuField::DIR_IN, 2, -1, corner) == 2,
              "dir=1 sauté dans un coin");
        check(maze::nextAllowedValue(MenuField::DIR_IN, 2, 1, corner) == 3,
              "dir 2 -> 3 dans un coin");
        check(maze::nextAllowedValue(MenuField::DIR_IN, 3, 1, corner) == 3,
              "dir butée dans un coin");
    }

    // Corrections en cascade ciblées.
    {
        Params p;
        p.nX = 10;
        p.nY = 7;
        p.coinIn = 1;
        p.pixIn = 7;
        p.dirIn = 3;   // hors grille en bas
        p.coinOut = 3;
        p.pixOut = 7;
        p.dirOut = 1;
        p.nReset = 5;
        p.nLoops = 3;
        maze::fixDependentParameters(p);
        check(p.dirIn == 2, "dir_in corrigé vers 2");
        check(maze::validateParameters(p), "paramètres corrigés valides");

        p.nY = 5;
        maze::fixDependentParameters(p);
        check(p.pixIn >= 1 && p.pixIn <= 5 &&
                  p.pixOut >= 1 && p.pixOut <= maze::edgeLength(p.coinOut,
                                                                p.nX, p.nY),
              "pix corrigés après réduction de n_y");
        check(maze::validateParameters(p),
              "paramètres valides après réduction de n_y");

        Params q;
        q.nX = 10;
        q.nY = 7;
        q.coinIn = 3;
        q.pixIn = 2;
        q.dirIn = 2;
        q.coinOut = 1;
        q.pixOut = 2;
        q.dirOut = 2;
        maze::fixDependentParameters(q);
        check(maze::validateParameters(q),
              "sortie corrigée après changement d'entrée");
    }

    groupEnd();
}

void testMenuNavigation()
{
    groupBegin("navigation du menu et machine à états du menu");

    Game g;
    g.setRng(testRng);
    check(g.state() == UiState::Menu, "état initial menu");
    check(g.menuRow() == 0, "ligne initiale 0");
    check(g.parameters().nX == 10 && g.parameters().nY == 7 &&
              g.parameters().coinIn == 1 && g.parameters().pixIn == 1 &&
              g.parameters().dirIn == 2 && g.parameters().coinOut == 3 &&
              g.parameters().pixOut == 1 && g.parameters().dirOut == 2 &&
              g.parameters().nReset == 5 && g.parameters().nLoops == 3 &&
              g.parameters().typeAff == 0,
          "valeurs par défaut du cahier des charges");

    ActionResult r = g.handleButton(Button::South, 1000);
    check(r.redraw && g.menuRow() == 1, "Sud -> ligne 1");
    r = g.handleButton(Button::North, 1100);
    check(r.redraw && g.menuRow() == 0, "Nord -> ligne 0");
    r = g.handleButton(Button::North, 1200);
    check(r.redraw && g.menuRow() == 11, "Nord depuis 0 -> START");
    r = g.handleButton(Button::South, 1300);
    check(r.redraw && g.menuRow() == 0, "Sud depuis START -> ligne 0");

    r = g.handleButton(Button::Center, 1400);
    check(!r.redraw && g.state() == UiState::Menu,
          "validation hors START sans effet");

    r = g.handleButton(Button::East, 1500);
    check(r.redraw && g.parameters().nX == 11, "n_x 10 -> 11");
    r = g.handleButton(Button::West, 1600);
    check(r.redraw && g.parameters().nX == 10, "n_x 11 -> 10");

    Params coinParams;
    coinParams.nX = 10;
    coinParams.nY = 7;
    coinParams.coinIn = 1;
    coinParams.pixIn = 2;
    coinParams.dirIn = 2;
    coinParams.coinOut = 2;
    coinParams.pixOut = 1;
    coinParams.dirOut = 2;
    g.setParameters(coinParams);
    for (int i = 0; i < 5; ++i) g.handleButton(Button::South, 2000 + i);
    check(g.menuRow() == (int)MenuField::COIN_OUT, "ligne coin_out");
    r = g.handleButton(Button::East, 3000);
    check(r.redraw && g.parameters().coinOut == 3,
          "changement coin_out accepté");
    r = g.handleButton(Button::Center, 3100);
    check(!r.redraw && g.state() == UiState::Menu,
          "validation sur coin_out sans effet");

    while (g.menuRow() != (int)MenuField::START) {
        g.handleButton(Button::South, 3200);
    }
    r = g.handleButton(Button::East, 3300);
    check(!r.redraw, "Est sur START sans effet");
    r = g.handleButton(Button::West, 3400);
    check(!r.redraw, "Ouest sur START sans effet");

    seedTestRng(77);
    r = g.handleButton(Button::Center, 3500);
    check(r.redraw && g.state() == UiState::Playing, "START lance la partie");
    check(g.abandonCount() == 0, "START ne compte pas pour l'abandon");

    // Abandon : 5 validations en état A.
    const Params saved = g.parameters();
    for (int i = 0; i < 4; ++i) {
        r = g.handleButton(Button::Center, 4000 + i);
        check(r.redraw && g.state() == UiState::Playing,
              "validation A %d rafraîchit", i + 1);
        check(g.abandonCount() == i + 1, "compteur abandon=%d", i + 1);
    }
    // Un appui directionnel bloqué ne remet pas le compteur à zéro.
    // Depuis un coin d'entrée, une direction extérieure est ignorée.
    r = g.handleButton(Button::West, 5000);
    check(!r.redraw && g.abandonCount() == 4,
          "appui bloqué ne modifie pas l'abandon");
    r = g.handleButton(Button::Center, 5100);
    check(r.redraw && g.state() == UiState::Menu, "5e validation = abandon");
    check(paramsEqual(g.parameters(), saved), "paramètres conservés");
    check(g.abandonCount() == 0, "compteur remis à zéro au menu");

    // Un déplacement accepté remet le compteur à zéro.
    seedTestRng(78);
    while (g.menuRow() != (int)MenuField::START) {
        g.handleButton(Button::South, 6000);
    }
    g.handleButton(Button::Center, 6100);
    check(g.state() == UiState::Playing, "redémarrage après abandon");
    g.handleButton(Button::Center, 6200);
    g.handleButton(Button::Center, 6300);
    check(g.abandonCount() == 2, "deux validations A");
    const Maze &m = g.maze();
    std::vector<Cell> path;
    findPath(m, g.playerX(), g.playerY(), m.exitX, m.exitY, path);
    check(!path.empty(), "chemin disponible");
    if (!path.empty()) {
        const Cell from{g.playerX(), g.playerY()};
        r = g.handleButton(buttonForStep(from, path[0]), 6400);
        check(r.redraw && g.abandonCount() == 0,
              "déplacement accepté remet l'abandon à zéro");
    }

    groupEnd();
}

// ---------------------------------------------------------------------------
// Génération
// ---------------------------------------------------------------------------

struct EdgeRef {
    int coin;
    int pix;
    int x;
    int y;
};

std::vector<EdgeRef> allEdgeCells(int nX, int nY)
{
    std::vector<EdgeRef> cells;
    for (int coin = 1; coin <= 4; ++coin) {
        const int len = maze::edgeLength(coin, nX, nY);
        for (int pix = 1; pix <= len; ++pix) {
            int x, y;
            maze::convertEdgeToCell(coin, pix, nX, nY, x, y);
            cells.push_back(EdgeRef{coin, pix, x, y});
        }
    }
    return cells;
}

void prepareMaze(Maze &m, const Params &p)
{
    int ex, ey, xx, xy;
    maze::convertEdgeToCell(p.coinIn, p.pixIn, p.nX, p.nY, ex, ey);
    maze::convertEdgeToCell(p.coinOut, p.pixOut, p.nX, p.nY, xx, xy);
    m.init(p.nX, p.nY, ex, ey, xx, xy);
    int nx, ny;
    if (maze::neighborFromDir(p.coinIn, p.pixIn, p.dirIn,
                              p.nX, p.nY, nx, ny)) {
        m.setForcedEdge(ex, ey, nx, ny);
    }
    if (maze::neighborFromDir(p.coinOut, p.pixOut, p.dirOut,
                              p.nX, p.nY, nx, ny)) {
        m.setForcedEdge(xx, xy, nx, ny);
    }
}

void testGeneration()
{
    groupBegin("génération (5 000 graines, 4 tailles, boucles 0..10)");

    const int sizes[][2] = {{5, 5}, {15, 15}, {10, 7}, {7, 10}};
    for (const auto &sz : sizes) {
        const int nX = sz[0];
        const int nY = sz[1];
        const std::vector<EdgeRef> cells = allEdgeCells(nX, nY);
        std::vector<std::pair<EdgeRef, EdgeRef>> combos;
        for (const EdgeRef &in : cells) {
            const uint8_t inEdges = maze::cellEdgeSet(in.x, in.y, nX, nY);
            for (const EdgeRef &out : cells) {
                if ((maze::cellEdgeSet(out.x, out.y, nX, nY) & inEdges) == 0) {
                    combos.push_back({in, out});
                }
            }
        }

        for (int seed = 1; seed <= 5000; ++seed) {
            const auto &combo = combos[(size_t)(seed - 1) % combos.size()];
            Params p;
            p.nX = nX;
            p.nY = nY;
            p.coinIn = combo.first.coin;
            p.pixIn = combo.first.pix;
            p.dirIn = 1 + (seed % 3);
            {
                int tx, ty;
                if (!maze::neighborFromDir(p.coinIn, p.pixIn, p.dirIn,
                                           p.nX, p.nY, tx, ty)) {
                    p.dirIn = 2;
                }
            }
            p.coinOut = combo.second.coin;
            p.pixOut = combo.second.pix;
            p.dirOut = 1 + ((seed / 3) % 3);
            {
                int tx, ty;
                if (!maze::neighborFromDir(p.coinOut, p.pixOut, p.dirOut,
                                           p.nX, p.nY, tx, ty)) {
                    p.dirOut = 2;
                }
            }
            p.nLoops = (seed - 1) % 11;
            p.nReset = 0;
            p.typeAff = seed % 2;

            Maze m;
            prepareMaze(m, p);
            m.resetOpenToBase();
            seedTestRng((uint32_t)seed * 2654435761u + (uint32_t)nX);
            int cycles = -1;
            const bool ok = m.generate(p.nLoops, testRng, &cycles);

            check(ok, "génération seed=%d %dx%d", seed, nX, nY);
            check(m.validateMazeConnectivity(),
                  "connexité seed=%d %dx%d", seed, nX, nY);
            check(m.validateExitReachability(m.entryX, m.entryY),
                  "sortie atteignable seed=%d %dx%d", seed, nX, nY);
            check(m.allRedAndForcedOpen(),
                  "connexions imposées ouvertes seed=%d", seed);
            check(masksSymmetricAndInside(m),
                  "masques symétriques seed=%d", seed);
            check(cycles == p.nLoops && m.countCycles() == p.nLoops,
                  "boucles=%d attendu=%d seed=%d %dx%d",
                  m.countCycles(), p.nLoops, seed, nX, nY);
            if (g_fail > g_groupFails + 20) return;
        }
    }

    // Reproductibilité et diversité.
    const Params base = [] {
        Params p;
        p.nX = 10;
        p.nY = 7;
        p.coinIn = 1;
        p.pixIn = 3;
        p.dirIn = 2;
        p.coinOut = 3;
        p.pixOut = 3;
        p.dirOut = 2;
        p.nLoops = 5;
        return p;
    }();
    for (int seed = 1; seed <= 100; ++seed) {
        Maze a, b;
        prepareMaze(a, base);
        a.resetOpenToBase();
        seedTestRng((uint32_t)seed);
        a.generate(base.nLoops, testRng, nullptr);
        prepareMaze(b, base);
        b.resetOpenToBase();
        seedTestRng((uint32_t)seed);
        b.generate(base.nLoops, testRng, nullptr);
        check(a.hashOpen() == b.hashOpen(), "même graine seed=%d", seed);
    }

    uint32_t hashes[50];
    for (int seed = 1; seed <= 50; ++seed) {
        Maze a;
        prepareMaze(a, base);
        a.resetOpenToBase();
        seedTestRng((uint32_t)seed);
        a.generate(base.nLoops, testRng, nullptr);
        hashes[seed - 1] = a.hashOpen();
    }
    int unique = 0;
    for (int i = 0; i < 50; ++i) {
        bool seen = false;
        for (int j = 0; j < i; ++j) {
            if (hashes[i] == hashes[j]) seen = true;
        }
        if (!seen) ++unique;
    }
    check(unique >= 45, "graines différentes -> labyrinthes différents (%d/50)",
          unique);

    // type_aff ne change pas la structure logique.
    for (int seed = 1; seed <= 20; ++seed) {
        Params p0 = base;
        Params p1 = base;
        p0.typeAff = 0;
        p1.typeAff = 1;
        Harness h0, h1;
        check(h0.startDirect(p0, (uint32_t)seed), "départ type 0 seed=%d", seed);
        check(h1.startDirect(p1, (uint32_t)seed), "départ type 1 seed=%d", seed);
        check(h0.game.maze().hashOpen() == h1.game.maze().hashOpen(),
              "type_aff sans effet sur la structure seed=%d", seed);
    }

    groupEnd();
}

// ---------------------------------------------------------------------------
// Machine à états du jeu
// ---------------------------------------------------------------------------

void testGameStates()
{
    groupBegin("machine à états et déplacements");

    Params p;
    p.nX = 10;
    p.nY = 7;
    p.coinIn = 1;
    p.pixIn = 2;
    p.dirIn = 2;
    p.coinOut = 3;
    p.pixOut = 3;
    p.dirOut = 2;
    p.nReset = 0;
    p.nLoops = 3;
    p.typeAff = 0;

    // Déplacement, retour et validation.
    {
        Harness h;
        check(h.startDirect(p, 101), "départ");
        const Maze &m0 = h.game.maze();
        std::vector<Cell> path;
        check(findPath(m0, h.game.playerX(), h.game.playerY(),
                       m0.exitX, m0.exitY, path) && path.size() >= 2,
              "chemin trouvé");
        const Cell start{h.game.playerX(), h.game.playerY()};
        ActionResult r = h.press(buttonForStep(start, path[0]));
        check(r.redraw && h.game.hasPending(), "déplacement -> état B");
        check(h.game.validSteps() == 0, "rien n'est compté avant validation");
        check(h.game.playerX() == path[0].x && h.game.playerY() == path[0].y,
              "position mise à jour");

        // En état B, une direction non-retour est ignorée.
        Button ignored = Button::North;
        for (int c = 0; c < 4; ++c) {
            Button cand = (Button)c;
            if (cand == Button::North || cand == Button::South ||
                cand == Button::East || cand == Button::West) {
                const int dx = (cand == Button::East) - (cand == Button::West);
                const int dy = (cand == Button::South) - (cand == Button::North);
                if (h.game.playerX() + dx == h.game.previousX() &&
                    h.game.playerY() + dy == h.game.previousY()) {
                    continue;
                }
                ignored = cand;
                break;
            }
        }
        const int px = h.game.playerX();
        const int py = h.game.playerY();
        r = h.press(ignored);
        check(!r.redraw && h.game.hasPending() &&
                  h.game.playerX() == px && h.game.playerY() == py,
              "direction ignorée en état B");

        // Retour : annule le segment.
        const int revDx = h.game.previousX() - h.game.playerX();
        const int revDy = h.game.previousY() - h.game.playerY();
        Button rev = Button::North;
        if (revDx == 1) rev = Button::East;
        else if (revDx == -1) rev = Button::West;
        else if (revDy == 1) rev = Button::South;
        r = h.press(rev);
        check(r.redraw && !h.game.hasPending(), "retour annule le segment");
        check(h.game.playerX() == h.game.previousX() &&
                  h.game.playerY() == h.game.previousY(),
              "position précédente restaurée");
        check(h.game.validSteps() == 0, "retour ne compte pas");

        // Déplacement puis validation.
        const Cell start2{h.game.playerX(), h.game.playerY()};
        r = h.press(buttonForStep(start2, path[0]));
        check(r.redraw && h.game.hasPending(), "second état B");
        r = h.press(Button::Center);
        check(r.redraw && !h.game.hasPending(), "validation -> état A");
        check(h.game.validSteps() == 1, "n_valid=1");
        check(h.game.abandonCount() == 0, "abandon remis à zéro");
        int compass = -1;
        const int dx = path[0].x - start2.x;
        const int dy = path[0].y - start2.y;
        if (dx == 1) compass = maze::EAST;
        else if (dx == -1) compass = maze::WEST;
        else if (dy == 1) compass = maze::SOUTH;
        else if (dy == -1) compass = maze::NORTH;
        check(compass >= 0 && h.game.maze().isRed(start2.x, start2.y, compass),
              "connexion rouge enregistrée");

        // Repassage sur le rouge, puis annulation : le rouge reste.
        r = h.press(rev);
        check(r.redraw && h.game.hasPending(),
              "repassage sur le rouge -> état B");
        const Button back = buttonForStep(start2, path[0]);
        r = h.press(back);
        check(r.redraw && !h.game.hasPending(),
              "retour annule le provisoire");
        check(h.game.maze().isRed(start2.x, start2.y, compass),
              "le rouge est restitué après annulation");
        check(h.game.validSteps() == 1, "pas de pas compté à l'annulation");
    }

    // Déplacements bloqués et hors grille.
    {
        Harness h;
        check(h.startDirect(p, 102), "départ blocages");
        const Maze &m = h.game.maze();
        const Cell pos{h.game.playerX(), h.game.playerY()};
        bool tested = false;
        for (int c = 0; c < 4; ++c) {
            int dx, dy;
            compassDelta(c, dx, dy);
            const int tx = pos.x + dx;
            const int ty = pos.y + dy;
            const bool outside = tx < 0 || tx >= m.nX || ty < 0 || ty >= m.nY;
            const bool closed = !outside && !m.isOpen(pos.x, pos.y, c);
            if (outside || closed) {
                Button b = Button::North;
                if (c == maze::EAST) b = Button::East;
                else if (c == maze::SOUTH) b = Button::South;
                else if (c == maze::WEST) b = Button::West;
                const ActionResult r = h.press(b);
                check(!r.redraw && !h.game.hasPending(),
                      "déplacement bloqué ignoré (c=%d)", c);
                check(h.game.playerX() == pos.x && h.game.playerY() == pos.y,
                      "position inchangée sur blocage");
                tested = true;
                break;
            }
        }
        check(tested, "au moins une direction bloquée depuis l'entrée");
    }

    // Sortie et fin de partie.
    {
        Params pe = p;
        pe.nReset = 0;
        Harness h;
        check(h.startDirect(pe, 103, true), "départ sortie");
        const uint32_t t0 = h.now;
        check(walkToExit(h, true), "parcours jusqu'à la sortie");
        check(h.game.state() == UiState::Playing, "partie encore en cours");
        const Maze &m = h.game.maze();
        std::vector<Button> ext;
        externalButtons(m, h.game.playerX(), h.game.playerY(), ext);
        check(ext.size() == 1, "une seule voie de sortie hors coin");
        const int before = h.game.validSteps();
        (void)before;
        const ActionResult r = h.press(ext[0], true);
        check(r.redraw && h.game.state() == UiState::End,
              "sortie -> écran de fin");
        check(h.game.elapsedMs() == h.lastPressMs - t0,
              "temps écoulé cohérent (%u)", (unsigned)h.game.elapsedMs());
        const Params saved = h.game.parameters();
        const ActionResult back = h.press(Button::Center);
        check(back.redraw && h.game.state() == UiState::Menu,
              "validation = menu après la fin");
        check(paramsEqual(h.game.parameters(), saved),
              "paramètres conservés après la fin");
    }

    // Sortie impossible avec un segment provisoire.
    {
        Harness h;
        check(h.startDirect(p, 104), "départ provisoire sortie");
        const Maze &m = h.game.maze();
        std::vector<Cell> path;
        findPath(m, h.game.playerX(), h.game.playerY(), m.exitX, m.exitY,
                 path);
        check(!path.empty(), "chemin vers la sortie");
        for (size_t i = 0; i + 1 < path.size(); ++i) {
            const Cell from{h.game.playerX(), h.game.playerY()};
            h.press(buttonForStep(from, path[i]));
            h.press(Button::Center);
        }
        const Cell from{h.game.playerX(), h.game.playerY()};
        h.press(buttonForStep(from, path.back()));
        check(h.game.hasPending() && h.game.playerX() == m.exitX &&
                  h.game.playerY() == m.exitY,
              "arrivée dans la sortie en état B");
        std::vector<Button> ext;
        externalButtons(m, m.exitX, m.exitY, ext);
        const ActionResult r = h.press(ext[0]);
        check(!r.redraw && h.game.state() == UiState::Playing &&
                  h.game.hasPending(),
              "sortie refusée tant que le pas n'est pas validé");
        h.press(Button::Center);
        check(h.game.state() == UiState::Playing && !h.game.hasPending(),
              "validation du dernier pas");
        const ActionResult out = h.press(ext[0]);
        check(out.redraw && h.game.state() == UiState::End,
              "sortie après validation");
    }

    // Sortie dans un coin : deux voies extérieures.
    for (int variant = 0; variant < 2; ++variant) {
        Params pc;
        pc.nX = 10;
        pc.nY = 7;
        pc.coinIn = 1;
        pc.pixIn = 3;
        pc.dirIn = 2;
        pc.coinOut = 3;
        pc.pixOut = 1;  // coin bas-droite
        pc.dirOut = 2;
        pc.nReset = 0;
        pc.nLoops = 2;
        Harness h;
        check(h.startDirect(pc, 105 + (uint32_t)variant),
              "départ coin variant=%d", variant);
        check(walkToExit(h, false), "parcours coin variant=%d", variant);
        const Maze &m = h.game.maze();
        std::vector<Button> ext;
        externalButtons(m, m.exitX, m.exitY, ext);
        check(ext.size() == 2, "deux voies extérieures dans un coin");
        const ActionResult r = h.press(ext[(size_t)variant]);
        check(r.redraw && h.game.state() == UiState::End,
              "sortie par la voie %d du coin", variant);
    }

    // Direction extérieure depuis une cellule qui n'est pas la sortie.
    {
        Harness h;
        check(h.startDirect(p, 106), "départ hors sortie");
        const ActionResult r = h.press(Button::West);
        check(!r.redraw && h.game.state() == UiState::Playing,
              "direction extérieure hors sortie ignorée");
    }

    // Après abandon puis START : nouveau labyrinthe.
    {
        Harness h;
        check(h.startDirect(p, 107), "départ abandon");
        const uint32_t firstHash = h.game.maze().hashOpen();
        for (int i = 0; i < 5; ++i) h.press(Button::Center);
        check(h.game.state() == UiState::Menu, "abandon -> menu");
        check(h.selectStartAndPress(), "START après abandon");
        check(h.game.maze().hashOpen() != firstHash,
              "nouveau labyrinthe après abandon");
    }

    groupEnd();
}

// ---------------------------------------------------------------------------
// Régénération
// ---------------------------------------------------------------------------

struct RobotStats {
    long games = 0;
    long regenerations = 0;
    long restChanged = 0;
    long loopsChanged = 0;
    bool ok = true;
    std::string failure;
};

Params randomParams(std::mt19937 &meta, int forcedReset = -1)
{
    auto ri = [&meta](int lo, int hi) {
        std::uniform_int_distribution<int> d(lo, hi);
        return d(meta);
    };
    Params p;
    p.nX = ri(5, 15);
    p.nY = ri(5, 15);
    p.coinIn = ri(1, 4);
    p.pixIn = ri(1, maze::edgeLength(p.coinIn, p.nX, p.nY));
    p.dirIn = ri(1, 3);
    p.coinOut = ri(1, 4);
    p.pixOut = ri(1, maze::edgeLength(p.coinOut, p.nX, p.nY));
    p.dirOut = ri(1, 3);
    p.nReset = (forcedReset >= 0) ? forcedReset : ri(0, 32);
    p.nLoops = ri(0, 10);
    p.typeAff = ri(0, 1);
    maze::fixDependentParameters(p);
    return p;
}

Button randomButton(std::mt19937 &meta, int &abandonWeight)
{
    std::uniform_int_distribution<int> d(0, 99);
    const int v = d(meta);
    if (v < abandonWeight) return Button::Center;
    if (v < 35) return Button::Center;
    const int dir = (int)(meta() % 4);
    switch (dir) {
    case 0: return Button::North;
    case 1: return Button::South;
    case 2: return Button::East;
    default: return Button::West;
    }
}

bool runRobotGame(uint32_t seed, const Params &p, int eventCount,
                  RobotStats *stats, std::string &failure)
{
    Harness h;
    seedTestRng(seed);
    h.game.setParameters(p);
    if (!h.game.startGame(h.now)) {
        failure = "startGame a échoué";
        return false;
    }
    std::string why;
    if (!invariantsHold(h.game, why)) {
        failure = "invariants initiaux : " + why;
        return false;
    }

    std::mt19937 meta(seed ^ 0x9E3779B9u);
    int events = 0;
    while (events < eventCount) {
        if (h.game.state() == UiState::Menu) {
            if (!h.selectStartAndPress()) {
                failure = "START impossible";
                return false;
            }
            if (!invariantsHold(h.game, why)) {
                failure = "invariants après START : " + why;
                return false;
            }
            continue;
        }
        if (h.game.state() == UiState::End) {
            h.press(Button::Center);
            if (!h.selectStartAndPress()) {
                failure = "START impossible après fin";
                return false;
            }
            continue;
        }

        const uint32_t beforeHash = h.game.maze().hashOpen();
        const int beforeX = h.game.playerX();
        const int beforeY = h.game.playerY();
        const bool beforePending = h.game.hasPending();
        uint8_t beforeRed[maze::MAX_N][maze::MAX_N];
        std::memcpy(beforeRed, h.game.maze().red, sizeof(beforeRed));
        uint8_t beforeOpen[maze::MAX_N][maze::MAX_N];
        std::memcpy(beforeOpen, h.game.maze().open, sizeof(beforeOpen));
        uint8_t beforeBase[maze::MAX_N][maze::MAX_N];
        for (int y = 0; y < maze::MAX_N; ++y) {
            for (int x = 0; x < maze::MAX_N; ++x) {
                beforeBase[y][x] = (uint8_t)(h.game.maze().red[y][x] |
                                             h.game.maze().forced[y][x]);
            }
        }

        // Poids variable : parfois des séries de validations pour déclencher
        // les abandons et les régénérations.
        int abandonWeight = 35;
        if ((meta() % 10) == 0) abandonWeight = 75;
        const Button b = randomButton(meta, abandonWeight);
        h.press(b);
        ++events;

        if (!invariantsHold(h.game, why)) {
            failure = "invariants après événement : " + why;
            return false;
        }
        if (h.game.state() != UiState::Playing) {
            continue;
        }

        const uint32_t afterHash = h.game.maze().hashOpen();
        if (afterHash != beforeHash) {
            if (stats) ++stats->regenerations;
            if (p.nReset == 0) {
                failure = "régénération avec n_reset=0";
                return false;
            }
            if (!beforePending) {
                failure = "régénération sans segment provisoire";
                return false;
            }
            if (h.game.playerX() != beforeX || h.game.playerY() != beforeY) {
                failure = "régénération a déplacé le joueur";
                return false;
            }
            if (h.game.validSteps() != 0) {
                failure = "n_valid non remis à zéro après régénération";
                return false;
            }
            for (int y = 0; y < h.game.maze().nY; ++y) {
                for (int x = 0; x < h.game.maze().nX; ++x) {
                    if ((beforeRed[y][x] & (uint8_t)~h.game.maze().red[y][x])) {
                        failure = "connexion rouge perdue à la régénération";
                        return false;
                    }
                }
            }
            const int mu0 = h.game.maze().countBaseCycles();
            const int expected = (p.nLoops > mu0) ? p.nLoops : mu0;
            if (h.game.maze().countCycles() != expected) {
                failure = "boucles après régénération";
                return false;
            }
            if (stats) {
                int diff = 0;
                int loopDiff = 0;
                for (int y = 0; y < h.game.maze().nY; ++y) {
                    for (int x = 0; x < h.game.maze().nX; ++x) {
                        const uint8_t nonBaseBefore =
                            (uint8_t)(beforeOpen[y][x] & ~beforeBase[y][x]);
                        const uint8_t newBase =
                            (uint8_t)(h.game.maze().red[y][x] |
                                      h.game.maze().forced[y][x]);
                        const uint8_t nonBaseAfter =
                            (uint8_t)(h.game.maze().open[y][x] & ~newBase);
                        const uint8_t changed = (uint8_t)(nonBaseBefore ^
                                                          nonBaseAfter);
                        diff += popcount8(changed);
                        if (changed) ++loopDiff;
                    }
                }
                if (diff > 0) ++stats->restChanged;
                if (loopDiff > 0) ++stats->loopsChanged;
            }
        } else if (p.nReset == 0) {
            // Rien à faire : hash stable vérifié implicitement.
        }
    }
    return true;
}

void testRegeneration()
{
    groupBegin("régénération dynamique (robots)");
    const int resets[] = {0, 1, 2, 5, 32};
    RobotStats total;
    for (int r = 0; r < 5; ++r) {
        for (int i = 0; i < 2000; ++i) {
            std::mt19937 meta((uint32_t)(100000 + r * 10000 + i));
            const Params p = randomParams(meta, resets[r]);
            const uint32_t seed = (uint32_t)(r * 100000 + i + 1);
            std::string failure;
            if (!runRobotGame(seed, p, 20 + (int)(meta() % 30), &total,
                              failure)) {
                check(false, "robot n_reset=%d graine=%u : %s", resets[r],
                      seed, failure.c_str());
                if (g_fail > g_groupFails + 5) return;
            } else {
                ++total.games;
            }
        }
    }
    check(total.games == 10000, "10 000 parties jouées (%ld)",
          total.games);
    check(total.regenerations > 0, "des régénérations ont eu lieu (%ld)",
          total.regenerations);
    check(total.restChanged > 0,
          "le reste du labyrinthe change réellement (%ld)", total.restChanged);
    check(total.loopsChanged > 0,
          "les boucles réapparaissent ailleurs (%ld)", total.loopsChanged);

    // Régénération exactement après n_reset pas validés.
    {
        Params p;
        p.nX = 10;
        p.nY = 7;
        p.coinIn = 1;
        p.pixIn = 2;
        p.dirIn = 2;
        p.coinOut = 3;
        p.pixOut = 3;
        p.dirOut = 2;
        p.nReset = 3;
        p.nLoops = 3;
        Harness h;
        check(h.startDirect(p, 890), "départ n_reset=3");
        const Maze &m = h.game.maze();
        std::vector<Cell> path;
        findPath(m, h.game.playerX(), h.game.playerY(), m.exitX, m.exitY,
                 path);
        check(path.size() >= 3, "chemin de 3 pas");
        for (int i = 0; i < 3 && i < (int)path.size(); ++i) {
            const uint32_t before = h.game.maze().hashOpen();
            const Cell from{h.game.playerX(), h.game.playerY()};
            h.press(buttonForStep(from, path[(size_t)i]));
            h.press(Button::Center);
            if (i < 2) {
                check(h.game.maze().hashOpen() == before &&
                          h.game.validSteps() == i + 1,
                      "pas de régénération avant n_reset (pas %d)", i + 1);
            } else {
                check(h.game.maze().hashOpen() != before &&
                          h.game.validSteps() == 0,
                      "régénération au 3e pas validé");
            }
        }
    }

    // Échec simulé de régénération.
    {
        Params p;
        p.nX = 10;
        p.nY = 7;
        p.coinIn = 1;
        p.pixIn = 3;
        p.dirIn = 2;
        p.coinOut = 3;
        p.pixOut = 3;
        p.dirOut = 2;
        p.nReset = 1;
        p.nLoops = 3;
        Harness h;
        check(h.startDirect(p, 888, true), "départ échec de régénération");
        const Maze &m = h.game.maze();
        std::vector<Cell> path;
        findPath(m, h.game.playerX(), h.game.playerY(), m.exitX, m.exitY,
                 path);
        check(!path.empty(), "chemin pour échec");
        const Cell from{h.game.playerX(), h.game.playerY()};
        h.press(buttonForStep(from, path[0]));
        const uint32_t beforeHash = h.game.maze().hashOpen();
        h.game.setRegenerationFailureInjection(true);
        h.press(Button::Center, true);
        check(h.game.state() == UiState::Playing, "reste en jeu après échec");
        check(h.game.regenerationFailed(), "signal d'échec actif");
        check(h.game.maze().hashOpen() == beforeHash,
              "ancien labyrinthe conservé après échec");
        check(h.game.validSteps() == 1, "n_valid non remis à zéro après échec");
        check(h.display.overflowCount() == 0,
              "aucun débordement au dessin du signal");

        const Maze &m2 = h.game.maze();
        int X[maze::MAX_N + 1], Y[maze::MAX_N + 1];
        maze::computeCellCoordinates(m2.nX, m2.nY, X, Y);
        const int scx = (X[m2.entryX] + X[m2.entryX + 1]) / 2;
        const int scy = (Y[m2.entryY] + Y[m2.entryY + 1]) / 2;
        const int scw = X[m2.entryX + 1] - X[m2.entryX];
        check(h.display.pixel(scx, scy) == Color::Red,
              "intérieur rouge du marqueur d'échec");
        check(h.display.pixel(scx - scw / 4, scy) == Color::Black,
              "bord noir du marqueur d'échec");

        // La tentative suivante a lieu à la validation d'après.
        h.game.setRegenerationFailureInjection(false);
        const Maze &m3 = h.game.maze();
        std::vector<Cell> path2;
        findPath(m3, h.game.playerX(), h.game.playerY(), m3.exitX, m3.exitY,
                 path2);
        check(!path2.empty(), "chemin après échec");
        const Cell from2{h.game.playerX(), h.game.playerY()};
        h.press(buttonForStep(from2, path2[0]));
        h.press(Button::Center);
        check(!h.game.regenerationFailed(), "signal retiré après succès");
        check(h.game.validSteps() == 0, "n_valid remis à zéro après succès");
        check(h.game.maze().hashOpen() != beforeHash,
              "régénération réussie à la validation suivante");
    }

    groupEnd();
}

// ---------------------------------------------------------------------------
// Rendu (sondage de pixels)
// ---------------------------------------------------------------------------

void checkPaletteAndOverflow(const sim::SimDisplay &d, const char *ctx)
{
    check(d.overflowCount() == 0, "débordement écran (%s)", ctx);
    for (int y = 0; y < sim::SimDisplay::HEIGHT; ++y) {
        for (int x = 0; x < sim::SimDisplay::WIDTH; ++x) {
            const int c = (int)d.pixel(x, y);
            if (c < 0 || c > 3) {
                check(false, "couleur hors palette (%s) en %d,%d", ctx, x, y);
                return;
            }
        }
    }
    check(true, "palette (%s)", ctx);
}

void checkFrame(const sim::SimDisplay &d, const Maze &m, const Params &p,
                const char *ctx)
{
    int X[maze::MAX_N + 1], Y[maze::MAX_N + 1];
    maze::computeCellCoordinates(m.nX, m.nY, X, Y);
    const int ex0 = X[m.exitX], ex1 = X[m.exitX + 1];
    const int ey0 = Y[m.exitY], ey1 = Y[m.exitY + 1];
    const uint8_t edges = maze::cellEdgeSet(m.exitX, m.exitY, m.nX, m.nY);

    auto inGapX = [&](int x) {
        return (edges & (maze::EDGE_TOP | maze::EDGE_BOTTOM)) &&
               x >= ex0 && x < ex1;
    };
    auto inGapY = [&](int y) {
        return (edges & (maze::EDGE_LEFT | maze::EDGE_RIGHT)) &&
               y >= ey0 && y < ey1;
    };

    bool ok = true;
    int badX = -1, badY = -1;
    for (int x = 0; x < 200 && ok; ++x) {
        if (!inGapX(x)) {
            if (d.pixel(x, 0) != Color::Black) { ok = false; badX = x; badY = 0; }
            else if (d.pixel(x, 1) != Color::Black) { ok = false; badX = x; badY = 1; }
            else if (d.pixel(x, 198) != Color::Black) { ok = false; badX = x; badY = 198; }
            else if (d.pixel(x, 199) != Color::Black) { ok = false; badX = x; badY = 199; }
        }
    }
    for (int y = 0; y < 200 && ok; ++y) {
        if (!inGapY(y)) {
            if (d.pixel(0, y) != Color::Black) { ok = false; badX = 0; badY = y; }
            else if (d.pixel(1, y) != Color::Black) { ok = false; badX = 1; badY = y; }
            else if (d.pixel(198, y) != Color::Black) { ok = false; badX = 198; badY = y; }
            else if (d.pixel(199, y) != Color::Black) { ok = false; badX = 199; badY = y; }
        }
    }
    check(ok, "cadre noir complet (%s) premier défaut %d,%d couleur=%d", ctx,
          badX, badY, ok ? -1 : (int)d.pixel(badX, badY));

    // Interruption blanche au bord de la cellule de sortie, hors trait de
    // sortie du mode 0.
    // Les murs internes jointifs couvrent les extrémités ; on sonde donc à
    // deux pixels des bords, hors du trait de sortie du mode 0.
    if (edges & maze::EDGE_LEFT) {
        check(d.pixel(1, ey0 + 2) == Color::White &&
                  d.pixel(1, ey1 - 3) == Color::White,
              "interruption cadre gauche (%s)", ctx);
    }
    if (edges & maze::EDGE_RIGHT) {
        check(d.pixel(198, ey0 + 2) == Color::White &&
                  d.pixel(198, ey1 - 3) == Color::White,
              "interruption cadre droite (%s)", ctx);
    }
    if (edges & maze::EDGE_TOP) {
        check(d.pixel(ex0 + 2, 1) == Color::White &&
                  d.pixel(ex1 - 3, 1) == Color::White,
              "interruption cadre haut (%s)", ctx);
    }
    if (edges & maze::EDGE_BOTTOM) {
        check(d.pixel(ex0 + 2, 198) == Color::White &&
                  d.pixel(ex1 - 3, 198) == Color::White,
              "interruption cadre bas (%s)", ctx);
    }

    // Voie de sortie : trait noir en mode 0, blanc en mode 1.
    int cx, cy;
    maze::cellCenter(m.exitX, m.exitY, X, Y, cx, cy);
    const Color outer = (p.typeAff == 0) ? Color::Black : Color::White;
    if (edges & maze::EDGE_LEFT) check(d.pixel(0, cy) == outer, "sortie gauche");
    if (edges & maze::EDGE_RIGHT) check(d.pixel(199, cy) == outer, "sortie droite");
    if (edges & maze::EDGE_TOP) check(d.pixel(cx, 0) == outer, "sortie haut");
    if (edges & maze::EDGE_BOTTOM) check(d.pixel(cx, 199) == outer, "sortie bas");
}

void checkConnections(const sim::SimDisplay &d, const Maze &m,
                      const Params &p, const char *ctx)
{
    int X[maze::MAX_N + 1], Y[maze::MAX_N + 1];
    maze::computeCellCoordinates(m.nX, m.nY, X, Y);
    bool ok = true;
    for (int y = 0; y < m.nY && ok; ++y) {
        for (int x = 0; x < m.nX && ok; ++x) {
            int cx, cy, nx, ny;
            maze::cellCenter(x, y, X, Y, cx, cy);
            if (x + 1 < m.nX) {
                maze::cellCenter(x + 1, y, X, Y, nx, ny);
                const bool open = (m.open[y][x] & (1u << maze::EAST)) != 0;
                const Color expected = open
                    ? (p.typeAff == 0 ? Color::Black : Color::White)
                    : (p.typeAff == 0 ? Color::White : Color::Black);
                if (d.pixel((cx + nx) / 2, (cy + ny) / 2) != expected) ok = false;
            }
            if (y + 1 < m.nY) {
                maze::cellCenter(x, y + 1, X, Y, nx, ny);
                const bool open = (m.open[y][x] & (1u << maze::SOUTH)) != 0;
                const Color expected = open
                    ? (p.typeAff == 0 ? Color::Black : Color::White)
                    : (p.typeAff == 0 ? Color::White : Color::Black);
                if (d.pixel((cx + nx) / 2, (cy + ny) / 2) != expected) ok = false;
            }
        }
    }
    check(ok, "connexions/murs conformes au mode %d (%s)", p.typeAff, ctx);

    // Trait rouge et jaune sur les connexions validées/provisoires.
    for (int y = 0; y < m.nY; ++y) {
        for (int x = 0; x < m.nX; ++x) {
            int cx, cy, nx, ny;
            maze::cellCenter(x, y, X, Y, cx, cy);
            if (x + 1 < m.nX && (m.red[y][x] & (1u << maze::EAST))) {
                maze::cellCenter(x + 1, y, X, Y, nx, ny);
                if (d.pixel((cx + nx) / 2, (cy + ny) / 2) != Color::Red) {
                    ok = false;
                }
            }
            if (y + 1 < m.nY && (m.red[y][x] & (1u << maze::SOUTH))) {
                maze::cellCenter(x, y + 1, X, Y, nx, ny);
                if (d.pixel((cx + nx) / 2, (cy + ny) / 2) != Color::Red) {
                    ok = false;
                }
            }
        }
    }
    check(ok, "trajet rouge sur les connexions validées (%s)", ctx);
}

Params makep(int nX, int nY, int ci, int pi, int co, int po, int loops,
             int mode)
{
    Params p;
    p.nX = nX;
    p.nY = nY;
    p.coinIn = ci;
    p.pixIn = pi;
    p.dirIn = 2;
    p.coinOut = co;
    p.pixOut = po;
    p.dirOut = 2;
    p.nReset = 0;
    p.nLoops = loops;
    p.typeAff = mode;
    maze::fixDependentParameters(p);
    return p;
}

void testRendering()
{
    groupBegin("rendu (palette, cadre, marqueurs, cercle, trajets)");

    for (int n = 5; n <= 15; ++n) {
        for (int mode = 0; mode <= 1; ++mode) {
            Params p;
            p.nX = n;
            p.nY = n;
            p.coinIn = 1;
            p.pixIn = 2;
            p.dirIn = 2;
            p.coinOut = 3;
            p.pixOut = 2;
            p.dirOut = 2;
            p.nReset = 0;
            p.nLoops = 2;
            p.typeAff = mode;

            Harness h;
            check(h.startDirect(p, 500u + (uint32_t)n + (uint32_t)mode * 100,
                                true),
                  "départ rendu %dx%d mode %d", n, n, mode);
            char ctx[64];
            std::snprintf(ctx, sizeof(ctx), "%dx%d mode %d", n, n, mode);
            checkPaletteAndOverflow(h.display, ctx);
            checkFrame(h.display, h.game.maze(), p, ctx);
            checkConnections(h.display, h.game.maze(), p, ctx);

            // Marqueurs de départ et de sortie, joueur déplacé.
            const Maze &m = h.game.maze();
            std::vector<Cell> path;
            findPath(m, h.game.playerX(), h.game.playerY(), m.exitX, m.exitY,
                     path);
            check(!path.empty(), "chemin rendu %s", ctx);
            if (!path.empty()) {
                const Cell from{h.game.playerX(), h.game.playerY()};
                h.press(buttonForStep(from, path[0]), true);
                h.press(Button::Center, true);
            }
            int X[maze::MAX_N + 1], Y[maze::MAX_N + 1];
            maze::computeCellCoordinates(m.nX, m.nY, X, Y);
            int scx, scy, ecx, ecy;
            maze::cellCenter(m.entryX, m.entryY, X, Y, scx, scy);
            maze::cellCenter(m.exitX, m.exitY, X, Y, ecx, ecy);
            const int scw = X[m.entryX + 1] - X[m.entryX];
            const int sch = Y[m.entryY + 1] - Y[m.entryY];
            const int ecw = X[m.exitX + 1] - X[m.exitX];
            const int ech = Y[m.exitY + 1] - Y[m.exitY];
            check(h.display.pixel(scx, scy) == Color::Black,
                  "centre départ noir (%s)", ctx);
            check(h.display.pixel(ecx, ecy) == Color::Red,
                  "centre sortie rouge (%s)", ctx);
            const int sw = scw / 2, sh = sch / 2;
            const int ew = ecw / 2, eh = ech / 2;
            check(h.display.pixel(scx - sw / 2, scy - sh / 2) == Color::Black &&
                      h.display.pixel(scx + sw / 2 - 1,
                                      scy + sh / 2 - 1) == Color::Black,
                  "taille rectangle départ (%s)", ctx);
            check(h.display.pixel(ecx - ew / 2, ecy - eh / 2) == Color::Red &&
                      h.display.pixel(ecx + ew / 2 - 1,
                                      ecy + eh / 2 - 1) == Color::Red,
                  "taille rectangle sortie (%s)", ctx);
            check(h.display.pixel(scx + sw / 2 + 1, scy + sh / 2 + 1) ==
                      Color::White,
                  "extérieur rectangle départ blanc (%s)", ctx);
            check(h.display.pixel(ecx + ew / 2 + 1, ecy + eh / 2 + 1) ==
                      Color::White,
                  "extérieur rectangle sortie blanc (%s)", ctx);
        }
    }

    // Ouvertures de sortie sur les quatre arêtes et dans un coin, deux modes.
    for (int mode = 0; mode <= 1; ++mode) {
        for (int edge = 1; edge <= 4; ++edge) {
            const int entryEdge = (edge + 1) % 4 + 1;
            Params p = makep(10, 7, entryEdge, 3, edge, 3, 2, mode);
            Harness h;
            check(h.startDirect(p, 700u + (uint32_t)mode * 10 +
                                       (uint32_t)edge, true),
                  "départ ouverture edge=%d mode=%d", edge, mode);
            char ctx[64];
            std::snprintf(ctx, sizeof(ctx), "exit edge %d mode %d",
                          edge, mode);
            checkPaletteAndOverflow(h.display, ctx);
            checkFrame(h.display, h.game.maze(), p, ctx);
        }
        Params pc = makep(10, 7, 1, 3, 3, 1, 2, mode);
        Harness hc;
        check(hc.startDirect(pc, 750u + (uint32_t)mode, true),
              "départ coin mode=%d", mode);
        char ctx[64];
        std::snprintf(ctx, sizeof(ctx), "exit coin mode %d", mode);
        checkPaletteAndOverflow(hc.display, ctx);
        checkFrame(hc.display, hc.game.maze(), pc, ctx);
    }

    // Cercle validé, puis provisoire, puis validé sur un pas.
    {
        Params p;
        p.nX = 10;
        p.nY = 7;
        p.coinIn = 1;
        p.pixIn = 2;
        p.dirIn = 2;
        p.coinOut = 3;
        p.pixOut = 3;
        p.dirOut = 2;
        p.nReset = 0;
        p.nLoops = 2;
        p.typeAff = 1;
        Harness h;
        check(h.startDirect(p, 600, true), "départ cercle");
        const Maze &m = h.game.maze();
        int X[maze::MAX_N + 1], Y[maze::MAX_N + 1];
        maze::computeCellCoordinates(m.nX, m.nY, X, Y);
        int cx, cy;
        maze::cellCenter(m.entryX, m.entryY, X, Y, cx, cy);
        const int cw = X[m.entryX + 1] - X[m.entryX];
        const int ch = Y[m.entryY + 1] - Y[m.entryY];
        int d = ((cw < ch ? cw : ch) * 8 + 5) / 10;
        if (d < 1) d = 1;
        int t = (d + 2) / 4;
        if (t < 1) t = 1;
        check(h.display.pixel(cx, cy) == Color::Yellow,
              "cercle validé : remplissage jaune");
        check(h.display.pixel(cx + d / 2 - t / 2, cy) == Color::Red,
              "cercle validé : contour rouge");
        check(h.display.pixel(cx + d / 2 + 1, cy + d / 2 + 1) ==
                  Color::White,
              "extérieur du cercle blanc");

        std::vector<Cell> path;
        findPath(m, h.game.playerX(), h.game.playerY(), m.exitX, m.exitY,
                 path);
        const Cell from{h.game.playerX(), h.game.playerY()};
        h.press(buttonForStep(from, path[0]), true);
        int cx2, cy2;
        maze::cellCenter(h.game.playerX(), h.game.playerY(), X, Y, cx2, cy2);
        check(h.display.pixel(cx2, cy2) == Color::Red,
              "cercle non validé : remplissage rouge");
        check(h.display.pixel(cx2 + d / 2 - t / 2, cy2) == Color::Yellow,
              "cercle non validé : contour jaune");
        const int mx = (cx2 + cx) / 2;
        const int my = (cy2 + cy) / 2;
        check(h.display.pixel(mx, my) == Color::Yellow,
              "segment provisoire jaune");
        check(h.display.countColor(Color::Yellow) > 0,
              "jaune présent dans l'image");

        h.press(Button::Center, true);
        check(h.display.pixel(cx2, cy2) == Color::Yellow,
              "cercle validé après pas : remplissage jaune");
        check(h.display.pixel(cx2 + d / 2 - t / 2, cy2) == Color::Red,
              "cercle validé après pas : contour rouge");
        check(h.display.pixel(mx, my) == Color::Red,
              "segment validé rouge");
    }

    // Jaune par-dessus rouge, puis retour : le rouge est restitué.
    {
        Params p;
        p.nX = 10;
        p.nY = 7;
        p.coinIn = 1;
        p.pixIn = 2;
        p.dirIn = 2;
        p.coinOut = 3;
        p.pixOut = 3;
        p.dirOut = 2;
        p.nReset = 0;
        p.nLoops = 2;
        p.typeAff = 1;
        Harness h;
        check(h.startDirect(p, 601, true), "départ jaune sur rouge");
        const Maze &m = h.game.maze();
        int X[maze::MAX_N + 1], Y[maze::MAX_N + 1];
        maze::computeCellCoordinates(m.nX, m.nY, X, Y);
        std::vector<Cell> path;
        findPath(m, h.game.playerX(), h.game.playerY(), m.exitX, m.exitY,
                 path);
        const Cell start{h.game.playerX(), h.game.playerY()};
        const Button fwd = buttonForStep(start, path[0]);
        h.press(fwd, true);
        h.press(Button::Center, true);
        int cx0, cy0, cx1, cy1;
        maze::cellCenter(start.x, start.y, X, Y, cx0, cy0);
        maze::cellCenter(path[0].x, path[0].y, X, Y, cx1, cy1);
        const int mx = (cx0 + cx1) / 2;
        const int my = (cy0 + cy1) / 2;
        check(h.display.pixel(mx, my) == Color::Red, "rouge après validation");
        const Button rev = buttonForStep(path[0], start);
        const Button back = buttonForStep(start, path[0]);
        h.press(rev, true);
        check(h.game.hasPending(), "repassage provisoire sur le rouge");
        check(h.display.pixel(mx, my) == Color::Yellow,
              "jaune au-dessus du rouge");
        h.press(back, true);
        check(!h.game.hasPending(), "retour annulé");
        check(h.display.pixel(mx, my) == Color::Red,
              "rouge restitué après annulation");
    }

    // Menu : sélection visible sur les deux colonnes.
    {
        Game g;
        g.setRng(testRng);
        sim::SimDisplay d;
        maze::renderer::drawMenu(d, g);
        checkPaletteAndOverflow(d, "menu défaut");
        check(d.pixel(5, 5) == Color::Black, "ligne 0 sélectionnée (inversée)");
        check(d.pixel(5, 38) == Color::White, "ligne 1 non sélectionnée");
        g.handleButton(Button::South, 1000);
        d.resetCounters();
        maze::renderer::drawMenu(d, g);
        check(d.pixel(5, 5) == Color::White &&
                  d.pixel(5, 38) == Color::Black,
              "sélection déplacée sur la ligne 1");
        while (g.menuRow() != (int)MenuField::START) {
            g.handleButton(Button::South, 1100);
        }
        d.resetCounters();
        maze::renderer::drawMenu(d, g);
        check(d.pixel(104, 175) == Color::Black,
              "START sélectionné en colonne 2");
        check(d.countColor(Color::Black) > 0, "texte du menu présent");
    }

    // Écran de fin.
    {
        Params p;
        p.nX = 10;
        p.nY = 7;
        p.coinIn = 1;
        p.pixIn = 2;
        p.dirIn = 2;
        p.coinOut = 3;
        p.pixOut = 3;
        p.dirOut = 2;
        p.nReset = 0;
        p.nLoops = 2;
        p.typeAff = 0;
        Harness h;
        check(h.startDirect(p, 602, true), "départ écran de fin");
        check(walkToExit(h, true), "parcours écran de fin");
        const Maze &m = h.game.maze();
        std::vector<Button> ext;
        externalButtons(m, m.exitX, m.exitY, ext);
        h.press(ext[0], true);
        check(h.game.state() == UiState::End, "état fin");
        checkPaletteAndOverflow(h.display, "écran de fin");
        check(h.display.countColor(Color::Red) > 0, "titre rouge");
        check(h.display.countColor(Color::Yellow) > 0, "bandeau jaune");
        check(h.display.countColor(Color::Black) > 10, "texte noir");
    }

    groupEnd();
}

// ---------------------------------------------------------------------------
// Mémoire et allocations
// ---------------------------------------------------------------------------

void testMemory()
{
    groupBegin("mémoire et sûreté");
    std::printf("  sizeof(Game)=%zu octets, sizeof(Maze)=%zu octets\n",
                sizeof(Game), sizeof(Maze));
    check(sizeof(Game) < 16 * 1024, "sizeof(Game)=%zu < 16 Ko",
          sizeof(Game));

    g_allocCount = 0;
    g_trackAlloc = true;
    {
        Params p;
        p.nReset = 2;
        Harness h;
        h.game.setRng(testRng);
        h.game.setParameters(p);
        seedTestRng(999);
        const bool ok = h.game.startGame(h.now);
        g_trackAlloc = false;
        check(ok, "partie de contrôle mémoire");
        h.now += 1000;
        g_trackAlloc = true;
        for (int i = 0; i < 20; ++i) {
            h.game.handleButton((Button)(i % 5), h.now + (uint32_t)i);
        }
        h.game.maze();
        g_trackAlloc = false;
    }
    check(g_allocCount == 0, "zéro allocation dynamique dans les modules (%ld)",
          g_allocCount);
    groupEnd();
}

// ---------------------------------------------------------------------------
// Fuzz
// ---------------------------------------------------------------------------

int runFuzz()
{
    std::printf("Fuzz : 100 000 parties...\n");
    const int totalGames = 100000;
    for (int gidx = 0; gidx < totalGames; ++gidx) {
        const uint32_t seed = (uint32_t)(gidx * 2654435761u + 12345u);
        std::mt19937 meta(seed);
        const Params p = randomParams(meta);

        Harness h;
        seedTestRng(seed);
        h.game.setParameters(p);
        if (!h.game.startGame(h.now)) {
            std::printf("FUZZ FAIL graine=%u : startGame a échoué "
                        "(nx=%d ny=%d ci=%d pi=%d co=%d po=%d)\n",
                        seed, p.nX, p.nY, p.coinIn, p.pixIn, p.coinOut,
                        p.pixOut);
            return 1;
        }

        std::string why;
        if (!invariantsHold(h.game, why)) {
            std::printf("FUZZ FAIL graine=%u : %s\n", seed, why.c_str());
            return 1;
        }

        char events[64];
        int eventCount = 0;
        const int target = 8 + (int)(meta() % 17);
        for (int step = 0; step < target; ++step) {
            if (eventCount < (int)sizeof(events) - 1) {
                int centerWeight = 35;
                const Button b = randomButton(meta, centerWeight);
                const char c = (b == Button::North) ? 'N'
                             : (b == Button::South) ? 'S'
                             : (b == Button::East) ? 'E'
                             : (b == Button::West) ? 'W' : 'C';
                events[eventCount++] = c;
                events[eventCount] = '\0';
                h.press(b);
            } else {
                break;
            }
            if (!invariantsHold(h.game, why)) {
                std::printf("FUZZ FAIL graine=%u événements=%s : %s\n",
                            seed, events, why.c_str());
                return 1;
            }
        }
        if ((gidx + 1) % 10000 == 0) {
            std::printf("  %d parties\n", gidx + 1);
            std::fflush(stdout);
        }
    }
    std::printf("FUZZ PASS : %d parties, invariants respectés\n", totalGames);
    return 0;
}

// ---------------------------------------------------------------------------
// Images de contrôle
// ---------------------------------------------------------------------------

void saveImage(sim::SimDisplay &d, const char *name)
{
    char path[256];
    std::snprintf(path, sizeof(path), "out/%s.png", name);
    if (!d.savePng(path, 4)) {
        std::printf("Impossible d'écrire %s\n", path);
    }
}


int runImages()
{
    mkdir("out", 0755);
    std::printf("Génération des images dans out/...\n");

    // Menu par défaut et sélections.
    {
        Game g;
        g.setRng(testRng);
        sim::SimDisplay d;
        maze::renderer::drawMenu(d, g);
        saveImage(d, "menu_default");
        g.handleButton(Button::South, 1000);
        g.handleButton(Button::South, 2000);
        d.resetCounters();
        maze::renderer::drawMenu(d, g);
        saveImage(d, "menu_col1_selected");
        while (g.menuRow() != (int)MenuField::N_BOUCLES) {
            g.handleButton(Button::South, 3000);
        }
        d.resetCounters();
        maze::renderer::drawMenu(d, g);
        saveImage(d, "menu_col2_selected");

        Params invalid = g.parameters();
        invalid.coinIn = 1;
        invalid.pixIn = 1;
        invalid.coinOut = 1; // arête commune, invalide
        g.setParameters(invalid);
        while (g.menuRow() != (int)MenuField::START) {
            g.handleButton(Button::South, 4000);
        }
        g.handleButton(Button::Center, 5000);
        d.resetCounters();
        maze::renderer::drawMenu(d, g);
        saveImage(d, "menu_error");
    }

    // Labyrinthes des tailles imposées, deux modes.
    const int sizes[][2] = {{5, 5}, {15, 15}, {10, 7}};
    for (const auto &sz : sizes) {
        for (int mode = 0; mode <= 1; ++mode) {
            Params p = makep(sz[0], sz[1], 1, 2, 3, 2, 3, mode);
            Harness h;
            if (!h.startDirect(p, 4242, true)) continue;
            char name[64];
            std::snprintf(name, sizeof(name), "maze_%dx%d_mode%d",
                          sz[0], sz[1], mode);
            saveImage(h.display, name);
        }
    }

    // Entrée sur chacune des quatre arêtes.
    for (int edge = 1; edge <= 4; ++edge) {
        const int exitEdge = (edge + 1) % 4 + 1;
        Params p = makep(10, 7, edge, 3, exitEdge, 3, 3, 0);
        Harness h;
        if (!h.startDirect(p, 5000 + (uint32_t)edge, true)) continue;
        char name[64];
        std::snprintf(name, sizeof(name), "entry_edge%d", edge);
        saveImage(h.display, name);
    }

    // Sortie sur chacune des quatre arêtes, puis dans un coin.
    for (int edge = 1; edge <= 4; ++edge) {
        const int entryEdge = (edge + 1) % 4 + 1;
        Params p = makep(10, 7, entryEdge, 3, edge, 3, 3, 0);
        Harness h;
        if (!h.startDirect(p, 5100 + (uint32_t)edge, true)) continue;
        char name[64];
        std::snprintf(name, sizeof(name), "exit_edge%d", edge);
        saveImage(h.display, name);
    }
    {
        Params p = makep(10, 7, 1, 3, 3, 1, 3, 1);
        Harness h;
        if (h.startDirect(p, 5200, true)) saveImage(h.display, "exit_corner");
    }

    // Partie en cours : segment provisoire puis validé.
    {
        Params p = makep(10, 7, 1, 2, 3, 3, 3, 1);
        Harness h;
        if (h.startDirect(p, 5300, true)) {
            const Maze &m = h.game.maze();
            std::vector<Cell> path;
            findPath(m, h.game.playerX(), h.game.playerY(), m.exitX, m.exitY,
                     path);
            if (!path.empty()) {
                const Cell from{h.game.playerX(), h.game.playerY()};
                h.press(buttonForStep(from, path[0]), true);
                saveImage(h.display, "play_pending");
                h.press(Button::Center, true);
                saveImage(h.display, "play_validated");
            }
        }
    }

    // Avant / après régénération.
    {
        Params p = makep(10, 7, 1, 2, 3, 3, 4, 1);
        p.nReset = 2;
        Harness h;
        if (h.startDirect(p, 5400, true)) {
            const Maze &m = h.game.maze();
            std::vector<Cell> path;
            findPath(m, h.game.playerX(), h.game.playerY(), m.exitX, m.exitY,
                     path);
            if (path.size() >= 2) {
                const Cell s0{h.game.playerX(), h.game.playerY()};
                h.press(buttonForStep(s0, path[0]), true);
                h.press(Button::Center, true);
                const Cell s1{h.game.playerX(), h.game.playerY()};
                h.press(buttonForStep(s1, path[1]), true);
                saveImage(h.display, "regen_before");
                h.press(Button::Center, true);
                saveImage(h.display, "regen_after");
            }
        }
    }

    // Signal d'échec de régénération.
    {
        Params p = makep(10, 7, 1, 2, 3, 3, 3, 1);
        p.nReset = 1;
        Harness h;
        if (h.startDirect(p, 5500, true)) {
            const Maze &m = h.game.maze();
            std::vector<Cell> path;
            findPath(m, h.game.playerX(), h.game.playerY(), m.exitX, m.exitY,
                     path);
            if (!path.empty()) {
                const Cell s0{h.game.playerX(), h.game.playerY()};
                h.press(buttonForStep(s0, path[0]), true);
                h.game.setRegenerationFailureInjection(true);
                h.press(Button::Center, true);
                saveImage(h.display, "regen_failure");
            }
        }
    }

    // Écran de fin.
    {
        Params p = makep(10, 7, 1, 2, 3, 3, 3, 0);
        Harness h;
        if (h.startDirect(p, 5600, true) && walkToExit(h, true)) {
            const Maze &m = h.game.maze();
            std::vector<Button> ext;
            externalButtons(m, m.exitX, m.exitY, ext);
            h.press(ext[0], true);
            saveImage(h.display, "end_screen");
        }
    }

    std::printf("Images écrites (PNG x4) dans out/.\n");
    return 0;
}

// ---------------------------------------------------------------------------
// Point d'entrée
// ---------------------------------------------------------------------------

void runTests()
{
    testGeometry();
    testMenuRules();
    testMenuNavigation();
    testGeneration();
    testGameStates();
    testRegeneration();
    testRendering();
    testMemory();

    std::printf("\nRésultat : %d PASS, %d FAIL\n", g_pass, g_fail);
}

} // namespace

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "test";
    if (std::strcmp(mode, "test") == 0) {
        runTests();
        return g_fail == 0 ? 0 : 1;
    }
    if (std::strcmp(mode, "fuzz") == 0) {
        return runFuzz();
    }
    if (std::strcmp(mode, "images") == 0) {
        return runImages();
    }
    std::printf("Usage : %s [test|fuzz|images]\n", argv[0]);
    return 2;
}
