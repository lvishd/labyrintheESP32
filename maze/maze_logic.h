/*
 * maze_logic.h — logique pure du labyrinthe (aucune dépendance Arduino).
 *
 * Ce module ne fait aucun accès matériel, aucune allocation dynamique et
 * n'utilise que la bibliothèque standard. Il est compilé tel quel par le
 * croquis Arduino et par la simulation PC.
 */
#ifndef MAZE_LOGIC_H
#define MAZE_LOGIC_H

#include <stdint.h>

namespace maze {

constexpr int MAX_N = 15;                 // dimensions maximales de la grille
constexpr int MAX_CELLS = MAX_N * MAX_N;
constexpr int SCREEN_W = 200;             // écran e-paper 200 x 200
constexpr int SCREEN_H = 200;

// Générateur aléatoire injecté : esp_random() côté carte, xorshift côté PC.
using RngFn = uint32_t (*)();

// Numéros d'arêtes physiques (convention du cahier des charges).
enum EdgeBits : uint8_t {
    EDGE_LEFT   = 1u << 0,   // arête 1, coin 1 -> coin 2
    EDGE_BOTTOM = 1u << 1,   // arête 2, coin 2 -> coin 3
    EDGE_RIGHT  = 1u << 2,   // arête 3, coin 3 -> coin 4
    EDGE_TOP    = 1u << 3    // arête 4, coin 4 -> coin 1
};

// Valeurs de dir : 1 = côté départ, 2 = intérieur, 3 = côté arrivée.
enum DirValue : uint8_t {
    DIR_START  = 1,
    DIR_INWARD = 2,
    DIR_END    = 3
};

// Directions de déplacement à l'écran (y croît vers le bas).
enum Compass : uint8_t {
    NORTH = 0,
    EAST  = 1,
    SOUTH = 2,
    WEST  = 3
};

// Paramètres du jeu, dans l'ordre exact du menu.
struct Params {
    int nX;
    int nY;
    int coinIn;
    int pixIn;
    int dirIn;
    int coinOut;
    int pixOut;
    int dirOut;
    int nReset;
    int nLoops;
    int typeAff;   // 0 = trajets possibles, 1 = murs infranchissables

    Params()
        : nX(10), nY(7),
          coinIn(1), pixIn(1), dirIn(2),
          coinOut(3), pixOut(1), dirOut(2),
          nReset(5), nLoops(3), typeAff(0) {}
};

// Longueur L de l'arête portant le coin donné (1..4).
int edgeLength(int coin, int nX, int nY);

// Conversion (coin, pix) -> cellule. Retourne false si hors bornes.
bool convertEdgeToCell(int coin, int pix, int nX, int nY, int &x, int &y);

// Arêtes physiques touchées par une cellule (masque EdgeBits).
uint8_t cellEdgeSet(int x, int y, int nX, int nY);

// Voisin désigné par dir (1..3) pour une cellule d'arête. false si hors grille.
bool neighborFromDir(int coin, int pix, int dir, int nX, int nY,
                     int &x, int &y);

// Nombre de voies extérieures d'une cellule (1 sur une arête, 2 dans un coin).
int countExitOpenings(int x, int y, int nX, int nY);

// Coordonnées de grille arrondies : X[i] = round(i * 200 / n).
void computeCellCoordinates(int nX, int nY, int X[MAX_N + 1], int Y[MAX_N + 1]);
void cellCenter(int x, int y, const int X[MAX_N + 1], const int Y[MAX_N + 1],
                int &cx, int &cy);

// Conversion direction de déplacement -> delta écran.
void compassDelta(int compass, int &dx, int &dy);
// Direction opposée (retour).
int oppositeCompass(int compass);

struct Maze {
    int nX;
    int nY;
    int entryX, entryY;
    int exitX, exitY;

    // Masques N/E/S/O par cellule. open contient toujours red et forced.
    uint8_t open[MAX_N][MAX_N];
    uint8_t red[MAX_N][MAX_N];
    uint8_t forced[MAX_N][MAX_N];

    Maze();

    void init(int width, int height, int ex, int ey, int xx, int xy);
    bool inBounds(int x, int y) const;

    // Une seule fonction met à jour les deux extrémités d'une connexion.
    static void setEdge(uint8_t masks[MAX_N][MAX_N],
                        int x1, int y1, int x2, int y2);
    static void clearEdge(uint8_t masks[MAX_N][MAX_N],
                          int x1, int y1, int x2, int y2);
    static bool hasEdge(const uint8_t masks[MAX_N][MAX_N],
                        int x1, int y1, int x2, int y2);

    void setOpen(int x1, int y1, int x2, int y2);
    void setForcedEdge(int x1, int y1, int x2, int y2);
    void setRedEdge(int x1, int y1, int x2, int y2);

    bool isOpen(int x, int y, int compass) const;
    bool isRed(int x, int y, int compass) const;

    // open = red | forced (base E0 d'une régénération).
    void resetOpenToBase();

    // Ajoute les connexions manquantes pour rendre le graphe connexe puis
    // ouvre des boucles. Suppose que open contient déjà E0. Met à jour
    // *outCycles avec le nombre cyclomatique final.
    bool generate(int nLoops, RngFn rng, int *outCycles);

    int countCycles() const;
    int countBaseCycles() const;      // cycles de red | forced uniquement
    int openEdgeCount() const;

    bool validateMazeConnectivity() const;              // depuis l'entrée
    bool validateExitReachability(int fromX, int fromY) const;
    bool allRedAndForcedOpen() const;

    uint32_t hashOpen() const;        // empreinte pour les tests
};

// Compteur de cycles d'un ensemble de masques quelconque (utile aux tests).
int countCyclesForMasks(const uint8_t masks[MAX_N][MAX_N],
                        int nX, int nY);

} // namespace maze

#endif // MAZE_LOGIC_H
