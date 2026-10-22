# Instructions à l’agent — Compilation et simulation sur PC du jeu de labyrinthe

Complément au « Cahier des charges v2 — Labyrinthe dynamique ESP32-S3 e-paper ». Il décrit comment vérifier le code **sans la carte** : compilation Arduino pour l’ESP32-S3, puis simulation complète de la logique et du rendu sur PC.

## 1. Principe et honnêteté du compte rendu

- Le firmware ne contient aucun mode de test. Tous les tests vivent **hors du croquis**, dans un dossier séparé, et compilent la **même source** que le firmware (aucune copie du code du jeu).
- Deux vérifications distinctes : (1) la compilation avec `arduino-cli` prouve seulement que le code est valide pour la carte ; (2) la simulation sur PC prouve que la logique et le dessin sont corrects.
- L’agent ne déclare jamais « compilé » ou « testé » sans avoir exécuté les commandes. Il colle dans son rapport les commandes exactes et les extraits de sortie (versions, nombre de tests, usage mémoire).
- Si une étape est impossible (pas d’accès réseau, outil absent), il le dit clairement, indique ce qui reste non vérifié et ne contourne pas en prétendant avoir réussi.
- Ce qui ne peut **pas** être validé sans la carte est listé au §9.

## 2. Organisation du code (obligatoire pour permettre la simulation)

```
projet/
  maze/                    croquis Arduino (le dossier porte le nom du .ino)
    maze.ino               setup() et loop() minimaux
    maze_logic.h / .cpp    C++ pur : grille, conversions, génération, boucles, régénération
    game.h / .cpp          menu et machine à états du jeu (A, B, fin, abandon)
    renderer.h / .cpp      dessin dans une interface Display (aucun accès matériel)
    display_iface.h        interface abstraite de dessin
    hw_epd.cpp / hw_input.cpp   pilote écran, boutons, horloge, RNG (ESP32 seulement)
  tests/                   hors du croquis, jamais compilé par arduino-cli
    Makefile
    sim_display.cpp        écran simulé et export d’images
    sim_main.cpp           lanceur de tests et de scénarios
    out/                   images et rapports générés
```

Règles :

- `maze_logic`, `game` et `renderer` **n’incluent pas** `Arduino.h` ni aucun en-tête `esp_*`. Ils n’utilisent que la bibliothèque standard (`<stdint.h>`, `<string.h>`, `<stdlib.h>`).
- L’aléatoire est **injecté** : le code reçoit un générateur `uint32_t rng()`. Le firmware branche `esp_random()` ; les tests branchent un xorshift32 initialisé par une graine, pour des résultats reproductibles.
- Le temps est injecté de la même façon (`millis()` côté carte, horloge simulée côté PC).
- Les boutons arrivent à la logique sous forme d’événements (`Button::North, South, East, West, Center`). La logique renvoie un résultat : `redraw` (oui/non) et l’état (menu, jeu, fin). Elle n’attend jamais, ne lit aucune broche, ne dessine pas.
- Le rendu passe uniquement par `display_iface.h` : effacer, rectangle plein, ligne épaisse, cercle avec contour et remplissage, texte, et `flush()`. L’écran réel et l’écran simulé sont deux implémentations de cette interface. **Le code de dessin du jeu existe une seule fois.**
- Aucune allocation dynamique dans ces trois modules (tableaux statiques 15 × 15).
- `hw_epd.cpp` et `hw_input.cpp` ne sont pas compilés sur PC.

## 3. Compilation Arduino (validité du code)

### 3.1 Installation

```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
arduino-cli config init
arduino-cli core update-index
arduino-cli core install esp32:esp32@<VERSION>
arduino-cli lib install <BIBLIOTHEQUE>@<VERSION>
```

- Choisir et **épingler** la version du core Arduino-ESP32 et celle de chaque bibliothèque ; les écrire dans le rapport et dans le README.
- Si la bibliothèque d’écran n’est pas disponible via le gestionnaire, copier dans le dossier du croquis les fichiers du code de démonstration officiel Waveshare pour cette carte, avec l’URL de la source et la version ou le commit dans un commentaire.

### 3.2 Compilation

Récupérer d’abord les noms exacts des options avec `arduino-cli board details -b esp32:esp32:esp32s3`, puis compiler pour l’ESP32-S3 avec la PSRAM activée (la carte a une PSRAM de 8 Mo en mode octal) :

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:PSRAM=opi --warnings all --output-dir projet/build projet/maze
```

Les options `PartitionScheme`, `FlashSize`, `USBMode` et `CDCOnBoot` doivent correspondre à la carte (8 Mo de flash) ; l’agent les documente. Il n’y a **pas** de téléversement dans cette étape.

### 3.3 Critères

- Zéro erreur.
- Zéro avertissement venant du code du projet (les avertissements des bibliothèques tierces sont notés, pas corrigés).
- Usage flash et RAM relevés dans la sortie, avec la marge restante.
- Une compilation avec des drapeaux plus stricts (`-Wall -Wextra`) passe aussi sur les modules du projet.

## 4. Simulation sur PC : mise en place

### 4.1 Compilation des tests

`tests/Makefile` compile directement les fichiers du croquis (chemins relatifs, sans les copier) avec :

```bash
g++ -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -I../maze ../maze/maze_logic.cpp ../maze/game.cpp ../maze/renderer.cpp sim_display.cpp sim_main.cpp -o sim
```

Cibles : `make test` (tous les tests automatiques, code de retour non nul au moindre échec), `make images` (images de contrôle), `make fuzz` (simulation longue). Les sanitizers restent actifs : tout débordement de tableau ou comportement indéfini est un échec.

### 4.2 Écran simulé

- Tampon de 200 × 200 pixels, un octet par pixel, palette limitée à : 0 = blanc, 1 = noir, 2 = rouge, 3 = jaune.
- Toute commande de dessin qui sort de 0..199 incrémente un compteur de débordements ; le test exige **zéro**. Toute couleur hors palette est une erreur.
- Export en image PNG, agrandie 4 fois, avec les vraies couleurs (blanc, noir, rouge, jaune). Si aucune bibliothèque PNG n’est disponible, écrire un fichier PPM et le convertir avec Python (Pillow) ou ImageMagick.

### 4.3 Pilotage par scripts

- Un scénario est une suite d’événements de boutons, par exemple `N E E C W C` (Nord, Est, Est, Centre, Ouest, Centre), avec la graine et les paramètres de départ.
- Le lanceur applique chaque événement à la logique, appelle le rendu si `redraw` est vrai, et peut enregistrer une image après chaque étape.
- Un « robot » joue aussi des parties au hasard (graine fixe) pour le test long : il alterne déplacements, validations, retours, appuis bloqués, validations seules.

## 5. Tests automatiques

Chaque test affiche PASS ou FAIL avec le détail (graine, paramètres, étape) pour rejouer l’échec.

### 5.1 Conversions et directions

1. Pour `5 × 5`, `15 × 15`, `10 × 7` et `7 × 10`, tous les coins et toutes les valeurs de `pix` (1 à L) : la cellule est dans la grille et appartient à l’arête demandée (`edgeSet`).
2. Exemples du cahier des charges §5.2 exactement : (0,0), (0,6), (0,6), (9,6), (9,6), (9,0), (9,0), (0,0) pour `n_x = 10`, `n_y = 7`.
3. Les deux désignations d’une cellule de coin donnent la même cellule ; le nombre de cellules distinctes du bord vaut `2·(n_x + n_y) − 4`.
4. `dir` : pour chaque arête, `dir = 1` est le voisin côté départ, `2` le voisin intérieur, `3` le voisin côté arrivée. Une cellule hors coin accepte les trois valeurs, une cellule de coin deux seulement.
5. Voies de sortie : une pour une cellule d’arête, deux pour une cellule de coin ; aucune pour la cellule d’entrée.

### 5.2 Paramètres et menu

1. Énumération exhaustive des combinaisons entrée/sortie : les valeurs permises par le menu sont exactement celles qui respectent la règle d’arête (aucune arête physique commune, cellules de coin comprises).
2. Depuis n’importe quel état de départ (aléatoire, 100 000 essais), modifier un paramètre aboutit à un état valide après la correction en cascade.
3. Navigation : Haut/Bas sur les deux colonnes, but ée aux limites, valeurs interdites sautées, `n_reset` de 0 à 32, `n_boucles` de 0 à 10.
4. Validation sur une autre ligne que START : sans effet. Validation sur START : lance la partie.

### 5.3 Génération

Pour au moins 5 000 graines et au moins les tailles `5 × 5`, `15 × 15`, `10 × 7`, `7 × 10`, avec entrée et sortie sur toutes les arêtes (dont les coins), `n_boucles` de 0 à 10 :

1. Toutes les cellules sont atteintes depuis l’entrée (parcours en largeur).
2. La cellule de sortie est atteinte, avec ses voies extérieures.
3. Les connexions imposées par `dir_in` et `dir_out` sont ouvertes.
4. Les masques de connexions sont symétriques (si A ouvre vers B, B ouvre vers A) et aucun n’ouvre vers l’extérieur de la grille, sauf les voies de sortie.
5. Le nombre de boucles recalculé par `countCycles()` vaut `n_boucles` ; tout écart doit être signalé par la fonction et expliqué, et ne doit pas se produire sur `5 × 5` pour `n_boucles ≤ 10`.
6. Même graine = même labyrinthe ; graines différentes = labyrinthes différents.
7. `type_aff` n’intervient pas : le tableau de connexions est identique pour les deux modes.

### 5.4 Machine à états et déplacements

Scénarios scriptés vérifiant, à chaque pas, l’état en mémoire **et** le drapeau `redraw` :

1. Déplacement possible : segment provisoire créé, position mise à jour, rien n’est encore compté.
2. Déplacement vers un mur ou hors grille : ignoré, pas de `redraw`.
3. En état B (segment jaune), seules « retour » et Validation agissent ; les trois autres directions sont ignorées sans `redraw`.
4. Validation en B : le segment devient rouge, `n_valid` augmente de 1, compteur d’abandon à zéro.
5. Retour en B : segment annulé, position précédente restaurée, aucun pas compté, tableau `red` inchangé.
6. Repassage sur un segment rouge : jaune provisoire ; annulation = redevient rouge, jamais effacé (vérifié aussi sur l’image : pixels rouges présents).
7. Validation en état A : `redraw` vrai, compteur d’abandon +1 ; cinq validations de suite = abandon, retour au menu avec les mêmes paramètres, sans autre effet.
8. Un déplacement accepté ou la validation d’un segment remet le compteur à zéro ; un appui bloqué ne le modifie pas ; l’appui sur START ne compte pas.
9. Après abandon puis START : nouveau labyrinthe (graine différente) avec les mêmes paramètres.

### 5.5 Sortie

1. Direction extérieure depuis une cellule autre que la sortie : ignorée.
2. Depuis la cellule de sortie avec un segment provisoire non validé : ignorée ; après validation : la partie se termine.
3. Sortie en coin : les deux directions extérieures sont acceptées ; une troisième direction est ignorée.
4. Après la sortie : écran de fin avec le temps (horloge simulée), puis Validation ramène au menu avec les paramètres précédents.

### 5.6 Régénération

Avec un robot, pour `n_reset` dans {0, 1, 2, 5, 32} et au moins 2 000 parties :

1. `n_reset = 0` : le tableau de connexions ne change jamais (comparaison par empreinte).
2. À chaque régénération : toutes les connexions rouges sont inchangées et ouvertes ; la position du joueur est inchangée ; aucune régénération n’a lieu en état B ni après un retour.
3. Après chaque régénération : toutes les cellules sont atteintes depuis l’entrée, la sortie est atteinte depuis la position courante, les connexions imposées sont ouvertes.
4. `n_valid` est remis à zéro après un succès ; la régénération suivante arrive exactement après `n_reset` nouveaux pas.
5. Le reste du labyrinthe **change réellement** : sur l’ensemble des essais, la proportion de connexions non rouges modifiées est strictement positive, et des boucles non parcourues se retrouvent à d’autres emplacements.
6. Boucles : après régénération, `μ = max(n_boucles, μ0)` où `μ0` est le nombre de cycles contenus dans le trajet rouge et les connexions imposées ; aucun cycle rouge n’est supprimé.
7. Échec simulé (en forçant la validation à échouer) : l’ancien labyrinthe est conservé, le signal « départ noir et rouge » est dessiné, `n_valid` n’est pas remis à zéro, et la tentative suivante a lieu à la validation d’après.

### 5.7 Rendu (contrôles par sondage de pixels)

Pour toutes les tailles de 5 × 5 à 15 × 15 (au moins les extrêmes et `10 × 7`) et les deux modes :

1. Zéro débordement hors de 0..199 ; uniquement les quatre couleurs de la palette.
2. Le cadre de 2 px est noir sur tout le pourtour, sauf l’interruption blanche sur le ou les côtés extérieurs de la cellule de sortie.
3. Les cellules couvrent l’écran sans trou : `X[i]` et `Y[j]` strictement croissants, tailles de cellules écartées d’au plus 1 px.
4. Centre de chaque rectangle de départ (noir) et de sortie (rouge) : bonne couleur ; taille de 50 % de la cellule, à 1 px près.
5. Cercle de position : diamètre 80 % du plus petit côté de cellule, contour de 25 % (arrondi, 1 px minimum) ; couleurs jaune/rouge pour non validé, rouge/jaune pour validé.
6. Mode 0 : un trait noir de 5 px sur chaque connexion ouverte, aucun trait sur les connexions fermées ; mode 1 : murs noirs de 3 px sur les connexions fermées, passages blancs.
7. Trajet rouge de 5 px sur tous les pas validés ; le jaune ne recouvre jamais un rouge sans être au-dessus, et disparaît au retour.

### 5.8 Mémoire et sûreté

- Remplacer `operator new` et `malloc` pendant les tests des trois modules du jeu : zéro allocation.
- Taille de l’état du jeu (`sizeof`) affichée et inférieure à 16 Ko.
- Aucune erreur des sanitizers sur toute la suite.

## 6. Simulation longue (fuzz)

`make fuzz` joue 100 000 parties de robot avec paramètres aléatoires valides (dimensions, entrée, sortie, `dir`, `n_reset`, `n_boucles`, `type_aff`), des appuis aléatoires dont des appuis bloqués, et des abandons. Après **chaque** événement, des invariants sont contrôlés : trajet rouge ouvert, position valide, connexité, sortie atteignable, un seul segment provisoire au maximum, compteurs dans leurs bornes. Toute violation arrête le programme en affichant la graine et la suite d’événements pour la rejouer.

## 7. Images de contrôle (à regarder)

`make images` écrit dans `tests/out/` des PNG agrandis. L’agent les **ouvre et les examine** (outil de lecture d’images) et note ce qu’il constate :

1. Menu par défaut et menu avec une ligne sélectionnée sur chaque colonne ; lisibilité du texte et de la sélection.
2. Labyrinthes `5 × 5`, `15 × 15`, `10 × 7`, dans les deux modes.
3. Entrée sur chacune des quatre arêtes ; sortie sur chacune des quatre arêtes ; sortie en coin avec ses deux ouvertures.
4. Partie en cours : trajet rouge, segment jaune, cercle en état non validé, puis validé.
5. Même partie juste avant et juste après une régénération : trajet rouge identique, reste différent.
6. Signal d’échec de régénération.
7. Écran de fin.

Critères visuels : aucun élément coupé ou superposé de façon illisible, marqueurs centrés, épaisseurs visibles et proportionnées, cadre complet avec l’interruption à la sortie, quatre couleurs seulement.

## 8. Ordre de travail

1. Écrire les modules selon §2, puis la compilation de la logique et des tests sur PC.
2. Faire passer `make test`, puis `make fuzz`, puis examiner `make images`.
3. Compiler le croquis avec `arduino-cli` (§3).
4. Rejouer l’ensemble (`make test`, compilation) après **toute** correction. Aucune correction n’est livrée sans nouvelle exécution complète.

## 9. Ce qui reste non vérifié sans la carte

Le rapport doit lister, sans les minimiser :

- le pilote d’écran réel, le mappage des couleurs vers le contrôleur, la durée réelle d’un rafraîchissement et l’absence d’artefacts ;
- le comportement des broches GP1, GP2, GP3, GP19 et GP20 (contact à la masse, conflit avec le port USB, strapping de GP3) ;
- la procédure de téléversement avec les broches USB utilisées par le joystick ;
- le ressenti de jeu avec 5 à 10 s par action ;
- l’usage réel de la mémoire à l’exécution (seule l’estimation de la compilation est connue).

Ces points relèvent de la liste de vérification manuelle du cahier des charges (§17). Conseil de mise en route : d’abord un petit programme qui affiche quatre bandes de couleur et le nom du bouton pressé, pour valider l’écran et le câblage avant le jeu complet.

## 10. Livrables et critères de sortie

Livrables :

1. Le croquis `maze/` et le dossier `tests/` selon §2.
2. Un `README` avec versions épinglées, commandes d’installation, de compilation et de test.
3. Un rapport : commandes exécutées, sorties utiles, tableau PASS/FAIL par test, usage flash et RAM, images de contrôle et observations, liste du §9.

Critères de sortie :

- `arduino-cli compile` sans erreur ni avertissement du projet ;
- `make test` et `make fuzz` entièrement PASS, sanitizers silencieux ;
- images examinées et conformes au §7 ;
- tout échec restant signalé explicitement, jamais masqué.
