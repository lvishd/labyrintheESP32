/*
 * Pilote officiel Waveshare pour la carte ESP32-S3-ePaper-1.54G.
 * https://github.com/waveshareteam/ESP32-S3-ePaper-1.54G
 * Source : Example/Arduino_3.2.0/examples/08_E_paper_test/Debug.h
 * Commit : 55eb1278f482c0cfd1e0cba5611a4132022eea48 (2026-09-20)
 * Copie non modifiée du pilote officiel Waveshare, sauf Debug.h (USE_DEBUG=0).
 */
/*****************************************************************************
* | File      	:	Debug.h
* | Author      :   Waveshare team
* | Function    :	debug with printf
* | Info        :
*   Image scanning
*      Please use progressive scanning to generate images or fonts
*----------------
* |	This version:   V1.0
* | Date        :   2018-01-11
* | Info        :   Basic version
*
******************************************************************************/
#ifndef __DEBUG_H
#define __DEBUG_H

#include <Wire.h>

// Journal série du pilote désactivé : les broches USB sont utilisées
// par le joystick (GP19/GP20), et le débogage du jeu est optionnel.
#define USE_DEBUG 0
#if USE_DEBUG
	#define Debug(__info) Serial.print(__info)
#else
	#define Debug(__info)  
#endif

#endif

