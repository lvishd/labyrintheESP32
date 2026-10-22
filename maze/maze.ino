/*
 * maze.ino — jeu de labyrinthe dynamique sur Waveshare ESP32-S3-ePaper-1.54G.
 *
 * Voir instructions/plan.md pour le cahier des charges et README.md pour
 * l'installation, la compilation, le téléversement et les limites matérielles.
 *
 * CONFIGURATION MATÉRIELLE (résumé ; détails dans hardware_config.h et
 * dans le pilote officiel DEV_Config.h) :
 *  - écran : EPD_PWR=GPIO6, BUSY=GPIO8, RST=GPIO9, DC=GPIO10,
 *    CS=GPIO11, SCK=GPIO12, MOSI=GPIO13 ;
 *  - joystick (commun GND, pull-up interne) : Centre=GPIO20, Nord=GPIO19,
 *    Sud=GPIO3, Est=GPIO2, Ouest=GPIO1.
 *
 * Particularités matérielles :
 *  - GP19/GP20 = USB natif, réutilisés par le joystick (Nord / Centre).
 *    Téléverser joystick débranché, ou maintenir BOOT à la mise sous tension.
 *  - GP3 = strapping et LED de la carte (Sud) : ne pas le maintenir au boot.
 *  - Écran officiel Waveshare : 20 s en rafraîchissement complet, 15 s en
 *    mode rapide, d'après la documentation du produit (à confirmer sur carte).
 */
#define DEBUG 0

#include <Arduino.h>
#include <esp_system.h>

#include "game.h"
#include "renderer.h"
#include "hw_epd.h"
#include "hw_input.h"

using namespace maze;

namespace {
Game game;
EpdDisplay display;

uint32_t randomSource()
{
    return esp_random();
}
} // namespace

void setup()
{
    initButtonHardware();
    initDisplayHardware();

    game.setRng(randomSource);
    game.setParameters(Params());

    renderer::drawCurrent(display, game);
    display.flush();

#if DEBUG
    Serial.begin(115200);
    Serial.println("Labyrinthe ESP32-S3 e-paper");
#endif
}

void loop()
{
    const Button b = waitButtonPress();
    const uint32_t pressedAt = millis();

#if DEBUG
    Serial.print("Bouton ");
    Serial.println((int)b);
#endif

#if DEBUG
    const UiState previousState = game.state();
#endif
    const ActionResult result = game.handleButton(b, pressedAt);
#if DEBUG
    if (previousState != UiState::Playing &&
        game.state() == UiState::Playing &&
        game.lastCycles() != game.lastCycleTarget()) {
        Serial.printf("Boucles : %d demandees, %d obtenues\n",
                      game.lastCycleTarget(), game.lastCycles());
    }
#endif
    if (result.redraw) {
        renderer::drawCurrent(display, game);
        display.flush();
        // Le chronomètre démarre à la fin du premier affichage de la partie.
        game.notifyDisplayDone(millis());
    }

    waitAllReleased();
}
