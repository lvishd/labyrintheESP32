/*
 * game.h — menu, machine à états et déroulement d'une partie.
 *
 * Aucune dépendance Arduino : les boutons et l'horloge sont injectés.
 */
#ifndef GAME_H
#define GAME_H

#include "maze_logic.h"

namespace maze {

enum class Button : uint8_t {
    North,
    South,
    East,
    West,
    Center
};

enum class UiState : uint8_t {
    Menu,
    Playing,
    End
};

struct ActionResult {
    bool redraw;
    UiState state;
    ActionResult() : redraw(false), state(UiState::Menu) {}
    ActionResult(bool r, UiState s) : redraw(r), state(s) {}
};

// Lignes du menu, dans l'ordre imposé (colonne 1 puis colonne 2).
enum class MenuField : uint8_t {
    N_X = 0,
    N_Y,
    COIN_IN,
    PIX_IN,
    DIR_IN,
    COIN_OUT,
    PIX_OUT,
    DIR_OUT,
    N_RESET,
    N_BOUCLES,
    TYPE_AFF,
    START,
    COUNT
};

constexpr int MENU_FIELD_COUNT = (int)MenuField::COUNT;

const char *fieldLabel(MenuField f);
int fieldValue(const Params &p, MenuField f);
void setFieldValue(Params &p, MenuField f, int value);

// Valeur autorisée ? (règle d'arête, voisin existant, bornes).
bool isFieldValueAllowed(MenuField f, int value, const Params &p);

// Valeur permise suivante (delta = +1 / -1), sans bouclage.
int nextAllowedValue(MenuField f, int current, int delta, const Params &p);

// Correction en cascade des paramètres dépendants.
void fixDependentParameters(Params &p);

// Revérification complète avant lancement.
bool validateParameters(const Params &p);

class Game {
public:
    Game();

    void setRng(RngFn rng);
    void setParameters(const Params &p);
    const Params &parameters() const { return p_; }

    // Lance une nouvelle partie. false + menuError si les paramètres sont
    // invalides ou si la génération échoue.
    bool startGame(uint32_t nowMs);

    ActionResult handleButton(Button b, uint32_t nowMs);

    // À appeler après le premier rafraîchissement complet : le chronomètre
    // démarre à la fin de ce premier affichage.
    void notifyDisplayDone(uint32_t nowMs);

    UiState state() const { return state_; }
    const Maze &maze() const { return maze_; }
    int playerX() const { return playerX_; }
    int playerY() const { return playerY_; }
    int previousX() const { return prevX_; }
    int previousY() const { return prevY_; }
    bool hasPending() const { return pending_; }
    int validSteps() const { return nValid_; }
    int abandonCount() const { return abandon_; }
    int menuRow() const { return menuRow_; }
    bool menuError() const { return menuError_; }
    bool regenerationFailed() const { return regenFailed_; }
    int lastCycles() const { return lastCycles_; }
    int lastCycleTarget() const { return lastCycleTarget_; }
    uint32_t elapsedMs() const;

    // Injection réservée aux tests de la simulation PC (jamais utilisée par
    // le croquis). Force le prochain essai de régénération à échouer.
    void setRegenerationFailureInjection(bool force) {
        forceRegenFailure_ = force;
    }

private:
    bool regenerateUnvisitedMaze();
    ActionResult handleMenu(Button b, uint32_t nowMs);
    ActionResult handlePlaying(Button b, uint32_t nowMs);
    void validatePending();
    void undoPending();
    bool moveFromButton(Button b, int &dx, int &dy) const;

    Params p_;
    Maze maze_;
    RngFn rng_;

    UiState state_;
    int menuRow_;
    bool menuError_;

    int playerX_, playerY_;
    int prevX_, prevY_;
    bool pending_;

    int nValid_;
    int abandon_;
    bool regenFailed_;
    bool forceRegenFailure_;
    int lastCycles_;
    int lastCycleTarget_;

    bool awaitingFirstFrame_;
    uint32_t startMs_;
    uint32_t endMs_;
};

} // namespace maze

#endif // GAME_H
