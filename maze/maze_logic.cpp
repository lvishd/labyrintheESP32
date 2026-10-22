/*
 * maze_logic.cpp — conversions, génération et validation du labyrinthe.
 */
#include "maze_logic.h"

#include <string.h>

namespace maze {

namespace {

constexpr int CAND_MAX = 2 * MAX_N * (MAX_N - 1); // 420 arêtes internes max

int popcount8(uint8_t v)
{
    int n = 0;
    while (v) {
        n += v & 1u;
        v >>= 1;
    }
    return n;
}

int dsuFind(int parent[MAX_CELLS], int a)
{
    while (parent[a] != a) {
        parent[a] = parent[parent[a]];
        a = parent[a];
    }
    return a;
}

void dsuUnion(int parent[MAX_CELLS], int a, int b)
{
    a = dsuFind(parent, a);
    b = dsuFind(parent, b);
    if (a != b) parent[b] = a;
}

uint16_t encodeEdge(int x, int y, int vertical)
{
    // 4 bits x, 5 bits y (0..14), 1 bit orientation.
    return (uint16_t)((y << 5) | (x << 1) | vertical);
}

void decodeEdge(uint16_t code, int &x, int &y, int &vertical)
{
    vertical = code & 1u;
    x = (code >> 1) & 0x0F;
    y = (code >> 5) & 0x1F;
}

void shuffleEdges(uint16_t *items, int count, RngFn rng)
{
    if (!rng) return;
    for (int i = count - 1; i > 0; --i) {
        int j = (int)(rng() % (uint32_t)(i + 1));
        uint16_t tmp = items[i];
        items[i] = items[j];
        items[j] = tmp;
    }
}

bool blockIsOpen(const Maze &m, int bx, int by)
{
    if (bx < 0 || by < 0 || bx + 1 >= m.nX || by + 1 >= m.nY) return false;
    const uint8_t bitE = (uint8_t)(1u << EAST);
    const uint8_t bitS = (uint8_t)(1u << SOUTH);
    return (m.open[by][bx] & bitE) &&
           (m.open[by][bx] & bitS) &&
           (m.open[by][bx + 1] & bitS) &&
           (m.open[by + 1][bx] & bitE);
}

// Vérifie uniquement les blocs 2x2 qui contiennent l'arête qui vient d'être
// ouverte : le contrôle reste constant et la génération est rapide.
bool createsOpenBlock(const Maze &m, int x, int y, int vertical)
{
    if (vertical) {
        return blockIsOpen(m, x - 1, y) || blockIsOpen(m, x, y);
    }
    return blockIsOpen(m, x, y - 1) || blockIsOpen(m, x, y);
}

void openCandidate(Maze &m, uint16_t code)
{
    int x, y, vertical;
    decodeEdge(code, x, y, vertical);
    if (vertical) m.setOpen(x, y, x, y + 1);
    else          m.setOpen(x, y, x + 1, y);
}

bool candidateIsClosed(const Maze &m, uint16_t code)
{
    int x, y, vertical;
    decodeEdge(code, x, y, vertical);
    if (vertical) return !m.isOpen(x, y, SOUTH);
    return !m.isOpen(x, y, EAST);
}

} // namespace

int edgeLength(int coin, int nX, int nY)
{
    if (coin == 1 || coin == 3) return nY;
    if (coin == 2 || coin == 4) return nX;
    return 0;
}

bool convertEdgeToCell(int coin, int pix, int nX, int nY, int &x, int &y)
{
    if (nX < 1 || nY < 1) return false;
    const int len = edgeLength(coin, nX, nY);
    if (len <= 0 || pix < 1 || pix > len) return false;

    switch (coin) {
    case 1: x = 0;         y = pix - 1;     break;
    case 2: x = pix - 1;   y = nY - 1;      break;
    case 3: x = nX - 1;    y = nY - pix;    break;
    case 4: x = nX - pix;  y = 0;           break;
    default: return false;
    }
    return x >= 0 && x < nX && y >= 0 && y < nY;
}

uint8_t cellEdgeSet(int x, int y, int nX, int nY)
{
    uint8_t set = 0;
    if (x == 0)      set |= EDGE_LEFT;
    if (y == nY - 1) set |= EDGE_BOTTOM;
    if (x == nX - 1) set |= EDGE_RIGHT;
    if (y == 0)      set |= EDGE_TOP;
    return set;
}

bool neighborFromDir(int coin, int pix, int dir, int nX, int nY,
                     int &x, int &y)
{
    int cx, cy;
    if (!convertEdgeToCell(coin, pix, nX, nY, cx, cy)) return false;

    int tx = 0, ty = 0;   // tangente départ -> arrivée
    int nx = 0, ny = 0;   // normale intérieure
    switch (coin) {
    case 1: tx = 0;  ty = 1;  nx = 1;  ny = 0;  break;
    case 2: tx = 1;  ty = 0;  nx = 0;  ny = -1; break;
    case 3: tx = 0;  ty = -1; nx = -1; ny = 0;  break;
    case 4: tx = -1; ty = 0;  nx = 0;  ny = 1;  break;
    default: return false;
    }

    switch (dir) {
    case DIR_START:  x = cx - tx; y = cy - ty; break;
    case DIR_INWARD: x = cx + nx; y = cy + ny; break;
    case DIR_END:    x = cx + tx; y = cy + ty; break;
    default: return false;
    }
    return x >= 0 && x < nX && y >= 0 && y < nY;
}

int countExitOpenings(int x, int y, int nX, int nY)
{
    if (x < 0 || x >= nX || y < 0 || y >= nY) return 0;
    return popcount8(cellEdgeSet(x, y, nX, nY));
}

void computeCellCoordinates(int nX, int nY, int X[MAX_N + 1], int Y[MAX_N + 1])
{
    if (nX < 1) nX = 1;
    if (nY < 1) nY = 1;
    for (int i = 0; i <= nX; ++i) {
        X[i] = (i * SCREEN_W + nX / 2) / nX;
    }
    for (int j = 0; j <= nY; ++j) {
        Y[j] = (j * SCREEN_H + nY / 2) / nY;
    }
    X[nX] = SCREEN_W;
    Y[nY] = SCREEN_H;
}

void cellCenter(int x, int y, const int X[MAX_N + 1], const int Y[MAX_N + 1],
                int &cx, int &cy)
{
    cx = (X[x] + X[x + 1]) / 2;
    cy = (Y[y] + Y[y + 1]) / 2;
}

void compassDelta(int compass, int &dx, int &dy)
{
    switch (compass) {
    case NORTH: dx = 0;  dy = -1; break;
    case EAST:  dx = 1;  dy = 0;  break;
    case SOUTH: dx = 0;  dy = 1;  break;
    case WEST:  dx = -1; dy = 0;  break;
    default:    dx = 0;  dy = 0;  break;
    }
}

int oppositeCompass(int compass)
{
    return (compass + 2) & 3;
}

Maze::Maze()
    : nX(0), nY(0), entryX(0), entryY(0), exitX(0), exitY(0)
{
    memset(open, 0, sizeof(open));
    memset(red, 0, sizeof(red));
    memset(forced, 0, sizeof(forced));
}

void Maze::init(int width, int height, int ex, int ey, int xx, int xy)
{
    nX = width;
    nY = height;
    entryX = ex;
    entryY = ey;
    exitX = xx;
    exitY = xy;
    memset(open, 0, sizeof(open));
    memset(red, 0, sizeof(red));
    memset(forced, 0, sizeof(forced));
}

bool Maze::inBounds(int x, int y) const
{
    return x >= 0 && x < nX && y >= 0 && y < nY;
}

void Maze::setEdge(uint8_t masks[MAX_N][MAX_N],
                   int x1, int y1, int x2, int y2)
{
    if (x1 < 0 || x1 >= MAX_N || y1 < 0 || y1 >= MAX_N) return;
    if (x2 < 0 || x2 >= MAX_N || y2 < 0 || y2 >= MAX_N) return;

    const int dx = x2 - x1;
    const int dy = y2 - y1;
    int compass;
    if (dx == 1 && dy == 0)       compass = EAST;
    else if (dx == -1 && dy == 0) compass = WEST;
    else if (dx == 0 && dy == 1)  compass = SOUTH;
    else if (dx == 0 && dy == -1) compass = NORTH;
    else return;

    masks[y1][x1] |= (uint8_t)(1u << compass);
    masks[y2][x2] |= (uint8_t)(1u << oppositeCompass(compass));
}

void Maze::clearEdge(uint8_t masks[MAX_N][MAX_N],
                     int x1, int y1, int x2, int y2)
{
    if (x1 < 0 || x1 >= MAX_N || y1 < 0 || y1 >= MAX_N) return;
    if (x2 < 0 || x2 >= MAX_N || y2 < 0 || y2 >= MAX_N) return;

    const int dx = x2 - x1;
    const int dy = y2 - y1;
    int compass;
    if (dx == 1 && dy == 0)       compass = EAST;
    else if (dx == -1 && dy == 0) compass = WEST;
    else if (dx == 0 && dy == 1)  compass = SOUTH;
    else if (dx == 0 && dy == -1) compass = NORTH;
    else return;

    masks[y1][x1] &= (uint8_t)~(1u << compass);
    masks[y2][x2] &= (uint8_t)~(1u << oppositeCompass(compass));
}

bool Maze::hasEdge(const uint8_t masks[MAX_N][MAX_N],
                   int x1, int y1, int x2, int y2)
{
    if (x1 < 0 || x1 >= MAX_N || y1 < 0 || y1 >= MAX_N) return false;
    if (x2 < 0 || x2 >= MAX_N || y2 < 0 || y2 >= MAX_N) return false;

    const int dx = x2 - x1;
    const int dy = y2 - y1;
    int compass;
    if (dx == 1 && dy == 0)       compass = EAST;
    else if (dx == -1 && dy == 0) compass = WEST;
    else if (dx == 0 && dy == 1)  compass = SOUTH;
    else if (dx == 0 && dy == -1) compass = NORTH;
    else return false;

    return (masks[y1][x1] & (uint8_t)(1u << compass)) != 0;
}

void Maze::setOpen(int x1, int y1, int x2, int y2)
{
    if (!inBounds(x1, y1) || !inBounds(x2, y2)) return;
    setEdge(open, x1, y1, x2, y2);
}

void Maze::setForcedEdge(int x1, int y1, int x2, int y2)
{
    if (!inBounds(x1, y1) || !inBounds(x2, y2)) return;
    setEdge(forced, x1, y1, x2, y2);
    setEdge(open, x1, y1, x2, y2);
}

void Maze::setRedEdge(int x1, int y1, int x2, int y2)
{
    if (!inBounds(x1, y1) || !inBounds(x2, y2)) return;
    setEdge(red, x1, y1, x2, y2);
    setEdge(open, x1, y1, x2, y2);
}

bool Maze::isOpen(int x, int y, int compass) const
{
    if (!inBounds(x, y) || compass < 0 || compass > 3) return false;
    return (open[y][x] & (uint8_t)(1u << compass)) != 0;
}

bool Maze::isRed(int x, int y, int compass) const
{
    if (!inBounds(x, y) || compass < 0 || compass > 3) return false;
    return (red[y][x] & (uint8_t)(1u << compass)) != 0;
}

void Maze::resetOpenToBase()
{
    for (int y = 0; y < MAX_N; ++y) {
        for (int x = 0; x < MAX_N; ++x) {
            open[y][x] = (uint8_t)(red[y][x] | forced[y][x]);
        }
    }
}

bool Maze::generate(int nLoops, RngFn rng, int *outCycles)
{
    if (nX < 1 || nX > MAX_N || nY < 1 || nY > MAX_N) return false;

    const int cellCount = nX * nY;
    int parent[MAX_CELLS];
    for (int i = 0; i < cellCount; ++i) parent[i] = i;

    // E0 existe déjà dans open : on fusionne ses composantes.
    for (int y = 0; y < nY; ++y) {
        for (int x = 0; x < nX; ++x) {
            const int id = y * nX + x;
            if ((open[y][x] & (1u << EAST)) && x + 1 < nX) {
                dsuUnion(parent, id, id + 1);
            }
            if ((open[y][x] & (1u << SOUTH)) && y + 1 < nY) {
                dsuUnion(parent, id, id + nX);
            }
        }
    }

    uint16_t candidates[CAND_MAX];
    int candidateCount = 0;
    for (int y = 0; y < nY; ++y) {
        for (int x = 0; x < nX; ++x) {
            if (x + 1 < nX && !(open[y][x] & (1u << EAST))) {
                candidates[candidateCount++] = encodeEdge(x, y, 0);
            }
            if (y + 1 < nY && !(open[y][x] & (1u << SOUTH))) {
                candidates[candidateCount++] = encodeEdge(x, y, 1);
            }
        }
    }

    // Kruskal aléatoire : connexité garantie sur toute la grille.
    shuffleEdges(candidates, candidateCount, rng);
    for (int i = 0; i < candidateCount; ++i) {
        int x, y, vertical;
        decodeEdge(candidates[i], x, y, vertical);
        const int a = y * nX + x;
        const int b = vertical ? (y + 1) * nX + x : y * nX + (x + 1);
        if (dsuFind(parent, a) != dsuFind(parent, b)) {
            openCandidate(*this, candidates[i]);
            dsuUnion(parent, a, b);
        }
    }

    const int mu0 = countCycles();
    int toAdd = nLoops - mu0;
    if (toAdd < 0) toAdd = 0;

    if (toAdd > 0) {
        // Liste courante des arêtes encore fermées, mélangée.
        candidateCount = 0;
        for (int y = 0; y < nY; ++y) {
            for (int x = 0; x < nX; ++x) {
                if (x + 1 < nX && !(open[y][x] & (1u << EAST))) {
                    candidates[candidateCount++] = encodeEdge(x, y, 0);
                }
                if (y + 1 < nY && !(open[y][x] & (1u << SOUTH))) {
                    candidates[candidateCount++] = encodeEdge(x, y, 1);
                }
            }
        }
        shuffleEdges(candidates, candidateCount, rng);

        int opened = 0;

        // Première passe : éviter les blocs 2x2 entièrement ouverts.
        for (int i = 0; i < candidateCount && opened < toAdd; ++i) {
            if (!candidateIsClosed(*this, candidates[i])) continue;
            int x, y, vertical;
            decodeEdge(candidates[i], x, y, vertical);
            openCandidate(*this, candidates[i]);
            if (createsOpenBlock(*this, x, y, vertical)) {
                if (vertical) clearEdge(open, x, y, x, y + 1);
                else          clearEdge(open, x, y, x + 1, y);
            } else {
                ++opened;
            }
        }

        // Seconde passe : relâche la règle esthétique si nécessaire.
        for (int i = 0; i < candidateCount && opened < toAdd; ++i) {
            if (!candidateIsClosed(*this, candidates[i])) continue;
            openCandidate(*this, candidates[i]);
            ++opened;
        }
        // Si le nombre d'arêtes fermées restantes ne suffit pas, le
        // labyrinthe reste valide mais le nombre de boucles demandé n'est
        // pas atteint : l'écart est visible via le nombre cyclomatique
        // renvoyé (outCycles) et signalé sur le port série si DEBUG=1.
    }

    if (outCycles) *outCycles = countCycles();
    return true;
}

int Maze::countCycles() const
{
    return countCyclesForMasks(open, nX, nY);
}

int Maze::countBaseCycles() const
{
    uint8_t base[MAX_N][MAX_N];
    for (int y = 0; y < MAX_N; ++y) {
        for (int x = 0; x < MAX_N; ++x) {
            base[y][x] = (uint8_t)(red[y][x] | forced[y][x]);
        }
    }
    return countCyclesForMasks(base, nX, nY);
}

int Maze::openEdgeCount() const
{
    int count = 0;
    for (int y = 0; y < nY; ++y) {
        for (int x = 0; x < nX; ++x) {
            if (x + 1 < nX && (open[y][x] & (1u << EAST))) ++count;
            if (y + 1 < nY && (open[y][x] & (1u << SOUTH))) ++count;
        }
    }
    return count;
}

bool Maze::validateMazeConnectivity() const
{
    if (nX < 1 || nY < 1 || !inBounds(entryX, entryY)) return false;

    int queue[MAX_CELLS];
    uint8_t seen[MAX_CELLS];
    memset(seen, 0, sizeof(seen));

    int head = 0, tail = 0;
    const int start = entryY * nX + entryX;
    queue[tail++] = start;
    seen[start] = 1;

    while (head < tail) {
        const int id = queue[head++];
        const int x = id % nX;
        const int y = id / nX;
        for (int c = 0; c < 4; ++c) {
            if (!(open[y][x] & (1u << c))) continue;
            int dx, dy;
            compassDelta(c, dx, dy);
            const int nx2 = x + dx;
            const int ny2 = y + dy;
            if (!inBounds(nx2, ny2)) return false; // masque incohérent
            const int nid = ny2 * nX + nx2;
            if (!seen[nid]) {
                seen[nid] = 1;
                queue[tail++] = nid;
            }
        }
    }
    return tail == nX * nY;
}

bool Maze::validateExitReachability(int fromX, int fromY) const
{
    if (!inBounds(fromX, fromY) || !inBounds(exitX, exitY)) return false;
    if (!allRedAndForcedOpen()) return false;
    if (countExitOpenings(exitX, exitY, nX, nY) < 1) return false;

    int queue[MAX_CELLS];
    uint8_t seen[MAX_CELLS];
    memset(seen, 0, sizeof(seen));

    int head = 0, tail = 0;
    const int start = fromY * nX + fromX;
    queue[tail++] = start;
    seen[start] = 1;

    while (head < tail) {
        const int id = queue[head++];
        const int x = id % nX;
        const int y = id / nX;
        for (int c = 0; c < 4; ++c) {
            if (!(open[y][x] & (1u << c))) continue;
            int dx, dy;
            compassDelta(c, dx, dy);
            const int nx2 = x + dx;
            const int ny2 = y + dy;
            if (!inBounds(nx2, ny2)) return false;
            const int nid = ny2 * nX + nx2;
            if (!seen[nid]) {
                seen[nid] = 1;
                queue[tail++] = nid;
            }
        }
    }
    return seen[exitY * nX + exitX] != 0;
}

bool Maze::allRedAndForcedOpen() const
{
    for (int y = 0; y < nY; ++y) {
        for (int x = 0; x < nX; ++x) {
            const uint8_t o = open[y][x];
            if ((red[y][x] & (uint8_t)~o) != 0) return false;
            if ((forced[y][x] & (uint8_t)~o) != 0) return false;
        }
    }
    return true;
}

uint32_t Maze::hashOpen() const
{
    uint32_t h = 2166136261u;
    for (int y = 0; y < nY; ++y) {
        for (int x = 0; x < nX; ++x) {
            h ^= open[y][x];
            h *= 16777619u;
        }
    }
    return h;
}

int countCyclesForMasks(const uint8_t masks[MAX_N][MAX_N],
                        int nX, int nY)
{
    if (nX < 1 || nY < 1) return 0;

    const int cellCount = nX * nY;
    int parent[MAX_CELLS];
    for (int i = 0; i < cellCount; ++i) parent[i] = i;

    int edges = 0;
    for (int y = 0; y < nY; ++y) {
        for (int x = 0; x < nX; ++x) {
            const int id = y * nX + x;
            if (x + 1 < nX && (masks[y][x] & (1u << EAST))) {
                ++edges;
                dsuUnion(parent, id, id + 1);
            }
            if (y + 1 < nY && (masks[y][x] & (1u << SOUTH))) {
                ++edges;
                dsuUnion(parent, id, id + nX);
            }
        }
    }

    int components = 0;
    for (int i = 0; i < cellCount; ++i) {
        if (dsuFind(parent, i) == i) ++components;
    }
    return edges - cellCount + components; // A - S + C
}

} // namespace maze
