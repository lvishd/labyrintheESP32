/*
 * game.cpp — menu, déplacements, abandon, sortie et régénération.
 */
#include "game.h"

#include <string.h>

namespace maze {

namespace {

bool entryCell(const Params &p, int &x, int &y)
{
    return convertEdgeToCell(p.coinIn, p.pixIn, p.nX, p.nY, x, y);
}

bool exitCell(const Params &p, int &x, int &y)
{
    return convertEdgeToCell(p.coinOut, p.pixOut, p.nX, p.nY, x, y);
}

bool coinOutHasValidPixel(int coinOut, const Params &p)
{
    int ex, ey, xx, xy;
    if (!entryCell(p, ex, ey)) return false;
    const uint8_t entryEdges = cellEdgeSet(ex, ey, p.nX, p.nY);
    const int len = edgeLength(coinOut, p.nX, p.nY);
    for (int pix = 1; pix <= len; ++pix) {
        if (!convertEdgeToCell(coinOut, pix, p.nX, p.nY, xx, xy)) continue;
        if ((cellEdgeSet(xx, xy, p.nX, p.nY) & entryEdges) == 0) return true;
    }
    return false;
}

int nearestAllowed(MenuField f, int current, const Params &p, int lo, int hi)
{
    if (current < lo) current = lo;
    if (current > hi) current = hi;
    for (int d = 0; d <= hi - lo; ++d) {
        const int lower = current - d;
        if (lower >= lo && isFieldValueAllowed(f, lower, p)) return lower;
        const int upper = current + d;
        if (upper <= hi && isFieldValueAllowed(f, upper, p)) return upper;
    }
    return current;
}

int fieldMin(MenuField f, const Params &p)
{
    (void)p;
    switch (f) {
    case MenuField::N_X:
    case MenuField::N_Y:       return 5;
    case MenuField::COIN_IN:
    case MenuField::COIN_OUT:  return 1;
    case MenuField::PIX_IN:    return 1;
    case MenuField::PIX_OUT:   return 1;
    case MenuField::DIR_IN:
    case MenuField::DIR_OUT:   return 1;
    case MenuField::N_RESET:   return 0;
    case MenuField::N_BOUCLES: return 0;
    case MenuField::TYPE_AFF:  return 0;
    case MenuField::START:
    default:                   return 0;
    }
}

int fieldMax(MenuField f, const Params &p)
{
    switch (f) {
    case MenuField::N_X:
    case MenuField::N_Y:       return 15;
    case MenuField::COIN_IN:   return 4;
    case MenuField::COIN_OUT:  return 4;
    case MenuField::PIX_IN:    return edgeLength(p.coinIn, p.nX, p.nY);
    case MenuField::PIX_OUT:   return edgeLength(p.coinOut, p.nX, p.nY);
    case MenuField::DIR_IN:
    case MenuField::DIR_OUT:   return 3;
    case MenuField::N_RESET:   return 32;
    case MenuField::N_BOUCLES: return 10;
    case MenuField::TYPE_AFF:  return 1;
    case MenuField::START:
    default:                   return 0;
    }
}

bool compassFromButton(Button b, int &compass)
{
    switch (b) {
    case Button::North: compass = NORTH; return true;
    case Button::South: compass = SOUTH; return true;
    case Button::East:  compass = EAST;  return true;
    case Button::West:  compass = WEST;  return true;
    case Button::Center:
    default: return false;
    }
}

} // namespace

const char *fieldLabel(MenuField f)
{
    switch (f) {
    case MenuField::N_X:       return "N_X";
    case MenuField::N_Y:       return "N_Y";
    case MenuField::COIN_IN:   return "COIN_IN";
    case MenuField::PIX_IN:    return "PIX_IN";
    case MenuField::DIR_IN:    return "DIR_IN";
    case MenuField::COIN_OUT:  return "COIN_OUT";
    case MenuField::PIX_OUT:   return "PIX_OUT";
    case MenuField::DIR_OUT:   return "DIR_OUT";
    case MenuField::N_RESET:   return "N_RESET";
    case MenuField::N_BOUCLES: return "N_BOUCLES";
    case MenuField::TYPE_AFF:  return "TYPE_AFF";
    case MenuField::START:     return "START";
    default:                   return "?";
    }
}

int fieldValue(const Params &p, MenuField f)
{
    switch (f) {
    case MenuField::N_X:       return p.nX;
    case MenuField::N_Y:       return p.nY;
    case MenuField::COIN_IN:   return p.coinIn;
    case MenuField::PIX_IN:    return p.pixIn;
    case MenuField::DIR_IN:    return p.dirIn;
    case MenuField::COIN_OUT:  return p.coinOut;
    case MenuField::PIX_OUT:   return p.pixOut;
    case MenuField::DIR_OUT:   return p.dirOut;
    case MenuField::N_RESET:   return p.nReset;
    case MenuField::N_BOUCLES: return p.nLoops;
    case MenuField::TYPE_AFF:  return p.typeAff;
    case MenuField::START:
    default:                   return 0;
    }
}

void setFieldValue(Params &p, MenuField f, int value)
{
    switch (f) {
    case MenuField::N_X:       p.nX = value; break;
    case MenuField::N_Y:       p.nY = value; break;
    case MenuField::COIN_IN:   p.coinIn = value; break;
    case MenuField::PIX_IN:    p.pixIn = value; break;
    case MenuField::DIR_IN:    p.dirIn = value; break;
    case MenuField::COIN_OUT:  p.coinOut = value; break;
    case MenuField::PIX_OUT:   p.pixOut = value; break;
    case MenuField::DIR_OUT:   p.dirOut = value; break;
    case MenuField::N_RESET:   p.nReset = value; break;
    case MenuField::N_BOUCLES: p.nLoops = value; break;
    case MenuField::TYPE_AFF:  p.typeAff = value; break;
    case MenuField::START:
    default: break;
    }
}

bool isFieldValueAllowed(MenuField f, int value, const Params &p)
{
    int x, y;
    switch (f) {
    case MenuField::N_X:
    case MenuField::N_Y:
        return value >= 5 && value <= 15;
    case MenuField::COIN_IN:
        return value >= 1 && value <= 4;
    case MenuField::PIX_IN: {
        const int len = edgeLength(p.coinIn, p.nX, p.nY);
        return value >= 1 && value <= len;
    }
    case MenuField::DIR_IN:
        return neighborFromDir(p.coinIn, p.pixIn, value,
                               p.nX, p.nY, x, y);
    case MenuField::COIN_OUT:
        return value >= 1 && value <= 4 && coinOutHasValidPixel(value, p);
    case MenuField::PIX_OUT: {
        int ex, ey, xx, xy;
        if (!entryCell(p, ex, ey)) return false;
        const int len = edgeLength(p.coinOut, p.nX, p.nY);
        if (value < 1 || value > len) return false;
        if (!convertEdgeToCell(p.coinOut, value, p.nX, p.nY, xx, xy)) {
            return false;
        }
        const uint8_t entryEdges = cellEdgeSet(ex, ey, p.nX, p.nY);
        return (cellEdgeSet(xx, xy, p.nX, p.nY) & entryEdges) == 0;
    }
    case MenuField::DIR_OUT:
        return neighborFromDir(p.coinOut, p.pixOut, value,
                               p.nX, p.nY, x, y);
    case MenuField::N_RESET:
        return value >= 0 && value <= 32;
    case MenuField::N_BOUCLES:
        return value >= 0 && value <= 10;
    case MenuField::TYPE_AFF:
        return value == 0 || value == 1;
    case MenuField::START:
    default:
        return false;
    }
}

int nextAllowedValue(MenuField f, int current, int delta, const Params &p)
{
    if (f == MenuField::START || delta == 0) return current;
    const int lo = fieldMin(f, p);
    const int hi = fieldMax(f, p);
    int value = current + (delta > 0 ? 1 : -1);
    while (value >= lo && value <= hi) {
        if (isFieldValueAllowed(f, value, p)) return value;
        value += (delta > 0 ? 1 : -1);
    }
    return current;
}

void fixDependentParameters(Params &p)
{
    if (p.nX < 5) p.nX = 5;
    if (p.nX > 15) p.nX = 15;
    if (p.nY < 5) p.nY = 5;
    if (p.nY > 15) p.nY = 15;

    p.pixIn = nearestAllowed(MenuField::PIX_IN, p.pixIn, p, 1,
                             edgeLength(p.coinIn, p.nX, p.nY));
    p.dirIn = nearestAllowed(MenuField::DIR_IN, p.dirIn, p, 1, 3);

    p.coinOut = nearestAllowed(MenuField::COIN_OUT, p.coinOut, p, 1, 4);
    p.pixOut = nearestAllowed(MenuField::PIX_OUT, p.pixOut, p, 1,
                              edgeLength(p.coinOut, p.nX, p.nY));
    p.dirOut = nearestAllowed(MenuField::DIR_OUT, p.dirOut, p, 1, 3);
}

bool validateParameters(const Params &p)
{
    for (int i = 0; i < MENU_FIELD_COUNT; ++i) {
        const MenuField f = (MenuField)i;
        if (f == MenuField::START) continue;
        const int lo = fieldMin(f, p);
        const int hi = fieldMax(f, p);
        const int v = fieldValue(p, f);
        if (v < lo || v > hi) return false;
        if (!isFieldValueAllowed(f, v, p)) return false;
    }
    return true;
}

Game::Game()
    : p_(), maze_(), rng_(nullptr),
      state_(UiState::Menu), menuRow_(0), menuError_(false),
      playerX_(0), playerY_(0), prevX_(0), prevY_(0), pending_(false),
      nValid_(0), abandon_(0), regenFailed_(false),
      forceRegenFailure_(false), lastCycles_(0), lastCycleTarget_(0),
      awaitingFirstFrame_(false), startMs_(0), endMs_(0)
{
}

void Game::setRng(RngFn rng)
{
    rng_ = rng;
}

void Game::setParameters(const Params &p)
{
    p_ = p;
    menuError_ = false;
}

bool Game::startGame(uint32_t nowMs)
{
    menuError_ = false;
    if (!validateParameters(p_)) {
        menuError_ = true;
        state_ = UiState::Menu;
        return false;
    }

    int ex, ey, xx, xy;
    if (!entryCell(p_, ex, ey) || !exitCell(p_, xx, xy)) {
        menuError_ = true;
        state_ = UiState::Menu;
        return false;
    }

    maze_.init(p_.nX, p_.nY, ex, ey, xx, xy);

    int nx, ny;
    if (neighborFromDir(p_.coinIn, p_.pixIn, p_.dirIn,
                        p_.nX, p_.nY, nx, ny)) {
        maze_.setForcedEdge(ex, ey, nx, ny);
    } else {
        menuError_ = true;
        state_ = UiState::Menu;
        return false;
    }
    if (neighborFromDir(p_.coinOut, p_.pixOut, p_.dirOut,
                        p_.nX, p_.nY, nx, ny)) {
        maze_.setForcedEdge(xx, xy, nx, ny);
    } else {
        menuError_ = true;
        state_ = UiState::Menu;
        return false;
    }

    bool generated = false;
    int cycles = 0;
    for (int attempt = 0; attempt < 50; ++attempt) {
        maze_.resetOpenToBase();
        if (!maze_.generate(p_.nLoops, rng_, &cycles)) break;
        if (maze_.validateMazeConnectivity() &&
            maze_.validateExitReachability(ex, ey) &&
            maze_.allRedAndForcedOpen()) {
            generated = true;
            break;
        }
    }

    if (!generated) {
        menuError_ = true;
        state_ = UiState::Menu;
        return false;
    }

    lastCycles_ = cycles;
    lastCycleTarget_ = p_.nLoops;
    playerX_ = ex;
    playerY_ = ey;
    prevX_ = ex;
    prevY_ = ey;
    pending_ = false;
    nValid_ = 0;
    abandon_ = 0;
    regenFailed_ = false;
    state_ = UiState::Playing;
    awaitingFirstFrame_ = true;
    startMs_ = nowMs;
    endMs_ = 0;
    return true;
}

ActionResult Game::handleButton(Button b, uint32_t nowMs)
{
    switch (state_) {
    case UiState::Menu:
        return handleMenu(b, nowMs);
    case UiState::Playing:
        return handlePlaying(b, nowMs);
    case UiState::End:
        if (b == Button::Center) {
            state_ = UiState::Menu;
            menuError_ = false;
            return ActionResult(true, state_);
        }
        return ActionResult(false, state_);
    default:
        return ActionResult(false, state_);
    }
}

ActionResult Game::handleMenu(Button b, uint32_t nowMs)
{
    if (b == Button::Center) {
        const MenuField f = (MenuField)menuRow_;
        if (f != MenuField::START) return ActionResult(false, state_);
        startGame(nowMs);
        return ActionResult(true, state_);
    }

    if (b == Button::North || b == Button::South) {
        // Parcours cyclique de la liste aplatie (colonne 1 puis colonne 2).
        // Le cahier des charges n'impose pas de butée pour Haut/Bas.
        const int step = (b == Button::North) ? MENU_FIELD_COUNT - 1 : 1;
        menuRow_ = (menuRow_ + step) % MENU_FIELD_COUNT;
        return ActionResult(true, state_);
    }

    if (b == Button::East || b == Button::West) {
        const MenuField f = (MenuField)menuRow_;
        if (f == MenuField::START) return ActionResult(false, state_);
        const int delta = (b == Button::East) ? 1 : -1;
        const int current = fieldValue(p_, f);
        const int next = nextAllowedValue(f, current, delta, p_);
        if (next == current) return ActionResult(false, state_);
        setFieldValue(p_, f, next);
        fixDependentParameters(p_);
        menuError_ = false;
        return ActionResult(true, state_);
    }

    return ActionResult(false, state_);
}

bool Game::moveFromButton(Button b, int &dx, int &dy) const
{
    int compass = 0;
    if (!compassFromButton(b, compass)) return false;
    compassDelta(compass, dx, dy);
    return true;
}

ActionResult Game::handlePlaying(Button b, uint32_t nowMs)
{
    if (pending_) {
        if (b == Button::Center) {
            validatePending();
            return ActionResult(true, state_);
        }
        int dx, dy;
        if (!moveFromButton(b, dx, dy)) return ActionResult(false, state_);
        const bool isReturn = (prevX_ == playerX_ + dx &&
                               prevY_ == playerY_ + dy);
        if (!isReturn) return ActionResult(false, state_);
        undoPending();
        return ActionResult(true, state_);
    }

    if (b == Button::Center) {
        ++abandon_;
        if (abandon_ >= 5) {
            abandon_ = 0;
            pending_ = false;
            state_ = UiState::Menu;
            menuError_ = false;
            return ActionResult(true, state_);
        }
        return ActionResult(true, state_);
    }

    int dx, dy;
    if (!moveFromButton(b, dx, dy)) return ActionResult(false, state_);
    const int tx = playerX_ + dx;
    const int ty = playerY_ + dy;

    if (tx < 0 || tx >= maze_.nX || ty < 0 || ty >= maze_.nY) {
        if (playerX_ == maze_.exitX && playerY_ == maze_.exitY) {
            state_ = UiState::End;
            endMs_ = nowMs;
            return ActionResult(true, state_);
        }
        return ActionResult(false, state_);
    }

    int compass = 0;
    if (!compassFromButton(b, compass)) return ActionResult(false, state_);
    if (!maze_.isOpen(playerX_, playerY_, compass)) {
        return ActionResult(false, state_);
    }

    prevX_ = playerX_;
    prevY_ = playerY_;
    playerX_ = tx;
    playerY_ = ty;
    pending_ = true;
    abandon_ = 0;
    return ActionResult(true, state_);
}

void Game::validatePending()
{
    maze_.setRedEdge(prevX_, prevY_, playerX_, playerY_);
    pending_ = false;
    ++nValid_;
    abandon_ = 0;

    if (p_.nReset > 0 && nValid_ >= p_.nReset) {
        if (regenerateUnvisitedMaze()) {
            nValid_ = 0;
            regenFailed_ = false;
        } else {
            regenFailed_ = true;
        }
    }
}

void Game::undoPending()
{
    playerX_ = prevX_;
    playerY_ = prevY_;
    pending_ = false;
    abandon_ = 0;
}

bool Game::regenerateUnvisitedMaze()
{
    if (forceRegenFailure_) {
        regenFailed_ = true;
        return false;
    }

    uint8_t backup[MAX_N][MAX_N];
    memcpy(backup, maze_.open, sizeof(backup));

    bool generated = false;
    int cycles = 0;
    lastCycleTarget_ = p_.nLoops;

    for (int attempt = 0; attempt < 50; ++attempt) {
        maze_.resetOpenToBase();
        if (!maze_.generate(p_.nLoops, rng_, &cycles)) continue;
        lastCycles_ = cycles;
        const int mu0 = maze_.countBaseCycles();
        lastCycleTarget_ = (p_.nLoops > mu0) ? p_.nLoops : mu0;
        if (maze_.validateMazeConnectivity() &&
            maze_.validateExitReachability(playerX_, playerY_) &&
            maze_.allRedAndForcedOpen()) {
            generated = true;
            break;
        }
    }

    if (!generated) {
        memcpy(maze_.open, backup, sizeof(backup));
        regenFailed_ = true;
        return false;
    }

    regenFailed_ = false;
    return true;
}

void Game::notifyDisplayDone(uint32_t nowMs)
{
    if (awaitingFirstFrame_ && state_ == UiState::Playing) {
        startMs_ = nowMs;
        awaitingFirstFrame_ = false;
    }
}

uint32_t Game::elapsedMs() const
{
    // En fin de partie seulement ; la soustraction non signée gère aussi le
    // rebouclage de millis() après 49 jours.
    if (state_ != UiState::End) return 0;
    return endMs_ - startMs_;
}

} // namespace maze
