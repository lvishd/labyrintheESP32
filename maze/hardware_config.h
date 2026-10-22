/*
 * hardware_config.h — configuration matérielle du jeu.
 *
 * CARTE : Waveshare ESP32-S3-ePaper-1.54G (ESP32-S3-PICO-1-N8R8).
 * ÉCRAN : 1,54" 200 x 200, 4 couleurs, pilote officiel Waveshare.
 * Les broches de l'écran sont définies dans le fichier officiel DEV_Config.h,
 * copié depuis la démonstration Waveshare (commit indiqué dans ce fichier) :
 *   EPD_PWR=GPIO6, EPD_BUSY=GPIO8, EPD_RST=GPIO9, EPD_DC=GPIO10,
 *   EPD_CS=GPIO11, EPD_SCK=GPIO12, EPD_MOSI=GPIO13.
 *
 * JOYSTICK : contacts à la masse, entrées avec pull-up interne.
 *   Centre (validation) = GP20, Nord = GP19, Sud = GP3,
 *   Est = GP2, Ouest = GP1. Commun du joystick sur GND.
 *
 * ATTENTION : GP19/GP20 sont les broches USB natives (D-/D+) de l'ESP32-S3.
 * Les utiliser pour le joystick rend le port USB natif indisponible pendant
 * l'exécution. Voir le README pour le téléversement (joystick débranché ou
 * mode bootloader par le bouton BOOT). GP3 est une broche de strapping :
 * ne pas maintenir Sud enfoncé à la mise sous tension.
 *
 * Le panneau 4 couleurs annonce 20 s en rafraîchissement complet et 15 s en
 * mode rapide (documentation Waveshare). Ces durées sont à confirmer sur la
 * carte ; elles expliquent le choix d'un rafraîchissement complet bloquant.
 */
#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#define JOY_PIN_CENTER 20
#define JOY_PIN_NORTH  19
#define JOY_PIN_SOUTH  3
#define JOY_PIN_EAST   2
#define JOY_PIN_WEST   1

#define EPD_EXPECTED_REFRESH_MS 15000

#endif // HARDWARE_CONFIG_H
