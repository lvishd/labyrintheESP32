# Labyrinthe dynamique — Waveshare ESP32-S3-ePaper-1.54G

Jeu de labyrinthe complet pour la carte **Waveshare ESP32-S3-ePaper-1.54G**
(ESP32-S3-PICO-1 N8R8, écran e-paper 200 × 200, quatre couleurs), en C++ pour
Arduino IDE. Le joueur se déplace au joystick, valide ses pas, et une partie
non encore parcourue du labyrinthe est régénérée après `n_reset` pas validés.

Ce dépôt contient :

```
maze/                 croquis Arduino (le dossier porte le nom du .ino)
  maze.ino            setup()/loop() minimaux + carte Arduino
  maze_logic.h/.cpp   géométrie, génération, boucles, validations (C++ pur)
  game.h/.cpp         menu et machine à états A/B/fin/abandon (C++ pur)
  renderer.h/.cpp     dessin complet dans l'interface Display (C++ pur)
  display_iface.h/.cpp interface de dessin, ligne/cercle/texte communs
  hw_epd.h/.cpp       afficheur réel (tampon 2 bits/pixel, pilote Waveshare)
  hw_input.h/.cpp     joystick, lecture bloquante, priorité des boutons
  hardware_config.h   broches du joystick et notes matérielles
  EPD_1in54g.*        pilote officiel Waveshare (copie, voir en-têtes)
  DEV_Config.*        pilote SPI officiel Waveshare (copie)
  Debug.h             en-têtes de débogage du pilote (USE_DEBUG=0)
tests/                hors du croquis, jamais compilé par arduino-cli
  Makefile            make test / make fuzz / make images
  sim_display.*       écran simulé + export PNG
  sim_main.cpp        tests, fuzz, scénarios et images de contrôle
  out/                images PNG générées
build/                sortie de compilation arduino-cli (généré)
README.md             ce fichier
RAPPORT.md            commandes exécutées, résultats et limites
instructions/         cahier des charges fourni
```

## Résultats vérifiés dans cet environnement

- `arduino-cli compile` : **0 erreur, 0 avertissement** ;
  342 676 octets de flash (10 % de 3 342 336) et 34 824 octets de RAM
  globale (10 % de 327 680).
- `make test` : **235 303 PASS, 0 FAIL**, avec ASan/UBSan silencieux.
- `make fuzz` : **100 000 parties**, invariants respectés.
- `make images` : 25 PNG agrandis ×4 examinés (menu, labyrinthes 5×5/10×7/15×15,
  entrées/sorties sur chaque arête, coin, segment jaune/rouge, avant/après
  régénération, échec de régénération, écran de fin).
- `sizeof(Game)` = **816 octets** (< 16 Ko exigés) ; aucune allocation
  dynamique dans les modules du jeu.

Détail des commandes et des sorties : voir `RAPPORT.md`.

## 1. Matériel et câblage

Carte : **Waveshare ESP32-S3-ePaper-1.54G** (ESP32-S3-PICO-1-N8R8,
8 Mo de flash, 8 Mo de PSRAM OPI, écran 1,54" 200 × 200 rouge/jaune/noir/blanc).

### Écran (broches internes à la carte)

Extraites du pilote officiel Waveshare (`DEV_Config.h`, commit indiqué dans le
fichier) et de la documentation produit :

| Signal | GPIO |
| --- | --- |
| EPD_PWR (commande d'alimentation) | 6 |
| EPD_BUSY | 8 |
| EPD_RST | 9 |
| EPD_DC | 10 |
| EPD_CS | 11 |
| EPD_SCK | 12 |
| EPD_MOSI | 13 |

Le pilote officiel `EPD_1in54g.cpp` + `DEV_Config.cpp` est copié dans `maze/`
(source et commit en tête de chaque fichier). Aucune bibliothèque externe
n'est nécessaire. Les couleurs du panneau sont : **0 = noir, 1 = blanc,
2 = jaune, 3 = rouge** (documentation et démonstration Waveshare).

### Joystick cinq contacts (commun sur GND, `INPUT_PULLUP`)

| Fonction | Direction | GPIO |
| --- | --- | --- |
| Centre (validation) | — | **20** |
| Nord (haut) | — | **19** |
| Sud (bas) | — | **3** |
| Est (droite) | — | **2** |
| Ouest (gauche) | — | **1** |

Le commun du joystick doit être relié à la masse (GND) de la carte. Les
contacts ferment à la masse ; le code active les pull-ups internes.

### Points d'attention

- **GP19/GP20 sont les broches USB natives (D-/D+)** de l'ESP32-S3. Utilisées
  par le joystick, elles rendent le port USB natif indisponible pendant
  l'exécution. Pour téléverser : débrancher le joystick de GP19/GP20, ou
  maintenir **BOOT** puis remettre sous tension pour entrer en mode
  téléchargement. Le port série USB n'est alors pas utilisable ; le débogage
  série est désactivé par défaut (`#define DEBUG 0`). La UART0 reste
  disponible sur le connecteur d'extension (GPIO43 TX / GPIO44 RX).
- **GP3 est une broche de strapping** (et partage la LED selon le cahier des
  charges). Ne pas maintenir **Sud** enfoncé à la mise sous tension.
- GP1, GP2, GP3, GP19 et GP20 ne sont en conflit avec aucune broche de
  l'écran, de la carte SD, de l'audio ni de l'I2C (tableau officiel des GPIO).
- Réglages Arduino conseillés : carte **ESP32S3 Dev Module**, **PSRAM = OPI**,
  **Flash 8 MB**, partition **8M with spiffs (3MB APP)**, **USB CDC On Boot =
  Disabled**, **USB Mode = Hardware CDC and JTAG**. Le téléversement se fait
  avec le joystick débranché des broches USB.

## 2. Bibliothèques et versions épinglées

| Élément | Version / source |
| --- | --- |
| arduino-cli | 1.5.1 |
| Core Arduino-ESP32 | **esp32:esp32@3.3.12** |
| Pilote écran | copie officielle Waveshare, commit `55eb1278f482c0cfd1e0cba5611a4132022eea48` (exemple `08_E_paper_test`) |
| Bibliothèque externe | aucune |
| Compilateur hôte des tests | g++ 15.3.0 (toolchain portable Bootlin x86-64, glibc stable 2026.08-1) |
| Sanitizers | AddressSanitizer + UndefinedBehaviorSanitizer |

La démonstration Waveshare cible Arduino-ESP32 3.2.0 ; le code compile sans
avertissement avec 3.3.12 (version utilisée et épinglée ici).

## 3. Compilation Arduino

### Compilation avec Arduino IDE

1. Installer **Arduino IDE 2.x**.
2. Ajouter l'URL
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   dans **Fichier > Préférences > URLs de gestionnaire de cartes
   supplémentaires**.
3. Dans **Outils > Gestionnaire de cartes**, installer **esp32 by Espressif
   Systems** en version **3.3.12**.
4. Ouvrir directement `maze/maze.ino` via **Fichier > Ouvrir**. Laisser tous
   les fichiers dans le dossier `maze/` : le dossier doit garder le nom du
   croquis.
5. Dans **Outils**, sélectionner la carte **ESP32S3 Dev Module**, puis :
   - PSRAM : **OPI PSRAM** ;
   - Flash Size : **8MB (64Mb)** ;
   - Partition Scheme : **8M with spiffs (3MB APP/1.5MB SPIFFS)** ;
   - USB CDC On Boot : **Disabled** ;
   - USB Mode : **Hardware CDC and JTAG**.
6. Cliquer sur **Vérifier** (Ctrl+R). Aucune bibliothèque externe n'est
   nécessaire : le pilote de l'écran est déjà dans `maze/`.
7. Pour téléverser, suivre la sous-section « Téléversement avec les broches
   USB occupées » ci-dessous : débrancher le joystick de **GP19/GP20** ou
   maintenir **BOOT** au reset.

### Compilation avec arduino-cli

```bash
# Installation (une fois)
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
arduino-cli config init
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.12

# Compilation (sans téléversement)
arduino-cli compile \
  --fqbn 'esp32:esp32:esp32s3:PSRAM=opi,FlashSize=8M,PartitionScheme=default_8MB,CDCOnBoot=default,USBMode=hwcdc' \
  --warnings all --output-dir build maze
```

Options de compilation documentées :

- `PSRAM=opi` : PSRAM 8 Mo en mode octal (la carte est équipée d'une PSRAM
  OPI ; le Sketch ne l'utilise pas, mais l'option correspond au matériel).
- `FlashSize=8M`, `PartitionScheme=default_8MB` : 8 Mo de flash.
- `CDCOnBoot=default` (désactivé) et `USBMode=hwcdc` (défaut de la carte) :
  le port USB natif n'est pas utilisé par `Serial` car GP19/GP20 servent au
  joystick.
- Aucun téléversement n'est effectué par cette commande.

### Téléversement avec les broches USB occupées

1. Débrancher les fils du joystick reliés à **GP19 (Nord)** et **GP20
   (Centre)**, ou débrancher le connecteur du joystick.
2. Brancher le câble USB Type-C, sélectionner la carte et les options
   ci-dessus, puis téléverser.
3. Si la carte ne se met pas en mode téléchargement : maintenir **BOOT**,
   remettre sous tension (ou appuyer sur RESET), relâcher BOOT, puis
   téléverser. Le bouton BOOT est le GPIO0.
4. Après téléversement, remettre le joystick en place et redémarrer.

Le programme n'utilise pas de redémarrage logiciel : abandon et fin de partie
reviennent au menu sans reset de l'ESP32.

## 4. Architecture logicielle

Les modules **`maze_logic`, `game` et `renderer` n'incluent ni `Arduino.h` ni
les en-têtes `esp_*`** ; ils n'utilisent que la bibliothèque standard et
compilent à l'identique sur PC et sur la carte. L'aléatoire est injecté
(`uint32_t (*rng)()`, `esp_random()` côté carte, xorshift côté tests) et le
temps est injecté (`millis()` côté carte, horloge simulée côté tests). Les
boutons arrivent à la logique sous forme d'événements `Button::North/South/
East/West/Center` ; `Game::handleButton()` renvoie `{redraw, état}` et
n'attend, ne lit aucune broche et ne dessine jamais.

Le rendu passe uniquement par `Display` (`display_iface.h`) : effacer,
rectangle plein, ligne épaisse, cercle avec contour/remplissage, texte,
`flush()`. `SimDisplay` (PC) et `EpdDisplay` (carte) sont les deux
implémentations ; le code de dessin du jeu existe une seule fois. Le tampon
réel est en 2 bits/pixel (10 000 octets) ; le tampon simulé est en 1 octet/
pixel pour les tests de palette.

### Menu et machine à états

- Menu sur deux colonnes de six lignes, 33 px par ligne, ligne sélectionnée
  inversée. Haut/Bas parcourt la liste aplatie (colonne 1 puis colonne 2) et
  boucle en fin de liste ; Gauche/Droite change la valeur sans bouclage et
  saute les valeurs interdites.
- Les corrections en cascade recalculent `pix_in`, `dir_in`, `coin_out`,
  `pix_out`, `dir_out` après tout changement de dimension ou d'entrée.
- `validateParameters()` revérifie tout au lancement ; en cas d'échec
  (normalement impossible après correction), un bandeau rouge « ERREUR
  PARAM » s'affiche et le menu reste actif.
- États du jeu : **A** (aucun segment provisoire), **B** (un segment jaune),
  **End**. En B, seuls le retour et la validation agissent. L'abandon est
  déclenché par cinq validations consécutives en A.
- Le chronomètre démarre à la fin du premier affichage de la partie et
  s'arrête à l'appui qui fait sortir le joueur ; il inclut les
  rafraîchissements intermédiaires.

## 5. Conventions de repérage

Coordonnées `(x, y)` de `(0,0)` en haut à gauche à `(n_x−1, n_y−1)` en bas à
droite. Coins : 1 = haut gauche, 2 = bas gauche, 3 = bas droite, 4 = haut
droite. L'arête `a` va du coin `a` au coin `a+1` (4 → 1).

`convertEdgeToCell(coin, pix)` (pix de 1 à L) :

| coin | cellule |
| --- | --- |
| 1 | `(0, pix−1)` |
| 2 | `(pix−1, n_y−1)` |
| 3 | `(n_x−1, n_y−pix)` |
| 4 | `(n_x−pix, 0)` |

`dir` désigne le voisin, avec `t` la tangente départ → arrivée et `n` la
normale intérieure : `1 = cellule − t`, `2 = cellule + n`, `3 = cellule + t`.
Une valeur n'est permise que si le voisin est dans la grille (deux valeurs
seulement dans un coin, `dir = 2` toujours possible).

L'entrée et la sortie ne doivent partager aucune **arête physique**
(`edgeSet(entrée) ∩ edgeSet(sortie) = ∅`, coins compris). La sortie est une
cellule de bord ; ses voies extérieures sont les normales extérieures des
arêtes qu'elle touche (une sur une arête, deux dans un coin). L'entrée n'a
aucune ouverture extérieure.

## 6. Génération et garanties

Le labyrinthe est un graphe non orienté de cellules, stocké en masques de
bits N/E/S/O dans `open[15][15]`, `red[15][15]` (pas validés) et `forced[15][15]`
(connexions imposées par `dir_in`/`dir_out`). `setEdge()` est la seule
fonction qui ouvre une connexion, toujours symétriquement.

Algorithme unique (départ et régénération) :

1. **E0** = toutes les connexions rouges + les deux connexions imposées.
2. **Connexité** : union-find sur les `n_x·n_y` cellules ; mélange
   Fisher-Yates de toutes les arêtes candidates avec le RNG injecté, puis
   Kruskal aléatoire. Toutes les cellules deviennent accessibles depuis
   l'entrée, partout dans la grille.
3. **Boucles** : `μ0 = countCycles()` sur E0 après connexité, puis
   `k = max(0, n_boucles − μ0)` arêtes fermées tirées au hasard et ouvertes.
   Une première passe évite les blocs 2 × 2 entièrement ouverts ; une seconde
   passe relâche cette règle si les candidats manquent. Le nombre cyclomatique
   final vaut `max(n_boucles, μ0)`. Si ce nombre ne peut pas être atteint
   (candidats insuffisants), le labyrinthe reste valide et l'écart est signalé
   par `outCycles` (et par le port série si `DEBUG=1`).
4. **Contrôle** : `validateMazeConnectivity()` (BFS depuis l'entrée, 100 % des
   cellules), `validateExitReachability()` (BFS depuis la position courante,
   sortie atteinte et connexions rouges/imposées ouvertes). Jusqu'à 50
   tentatives.

Le nombre de boucles est `μ = A − S + C` (arêtes ouvertes, sommets,
composantes), calculé par `countCycles()`. Garanties : le trajet rouge est
dans E0 et n'est jamais retiré ; toutes les cellules sont accessibles ; la
sortie est toujours atteignable ; les connexions imposées restent ouvertes ;
les culs-de-sac sont permis mais reliés.

## 7. Régénération

Après chaque pas validé, si `n_reset > 0` et `n_valid ≥ n_reset`, tout le
labyrinthe est régénéré avec `E0 = red | forced`, en conservant la position
du joueur. Les boucles non figées sont supprimées et recréées ailleurs. En
cas de succès : `n_valid = 0`, redessin complet, signal d'échec retiré. En cas
d'échec (filet de sécurité ou injection de test) : l'ancien labyrinthe est
conservé, le marqueur de départ devient noir avec un intérieur rouge,
`n_valid` n'est pas remis à zéro et la tentative est refaite à la validation
suivante. Aucune régénération n'a lieu en état B ni après un retour.

## 8. Rendu

Tout l'écran est redessiné depuis l'état mémoire dans un tampon puis rafraîchi
complètement (bloquant, 15 à 20 s selon le mode du panneau). Ordre : fond
blanc, cadre 2 px, structure, trajet rouge 5 px, segment jaune 5 px, marqueurs,
cercle du joueur. Les cellules sont calculées par
`X[i] = round(i·200/n_x)` et `Y[j] = round(j·200/n_y)`.

- **`type_aff = 0`** : traits noirs 5 px centre à centre sur chaque connexion
  ouverte ; à la sortie, trait noir du centre jusqu'au bord ; cases fermées
  non dessinées.
- **`type_aff = 1`** : murs noirs 3 px sur chaque frontière sans connexion,
  jointifs au cadre ; passages blancs ; trajet rouge/jaune par-dessus.
- Cadre interrompu en blanc sur toute la longueur de la ou des faces
  extérieures de la cellule de sortie (le trait de sortie noir du mode 0
  traverse l'interruption).
- Départ : rectangle noir plein à 50 % de la cellule ; sortie : rectangle
  rouge plein à 50 %. Échec de régénération : départ noir avec intérieur
  rouge.
- Joueur : cercle de diamètre 80 % du plus petit côté, contour de 25 % du
  diamètre (arrondi, 1 px minimum). Non validé : contour jaune, remplissage
  rouge ; validé : contour rouge, remplissage jaune.
- Sur les petites cellules des bords, le cercle est décalé d'au plus 1 px vers
  l'intérieur pour ne pas recouvrir le cadre de 2 px (le diamètre est inchangé).
- Écran de fin plein écran : « SORTIE ! », temps `mm:ss`, bandeau jaune
  « VALIDATION = MENU ».

## 9. Tests PC et simulation

```bash
cd tests
make test       # suite automatique, ASan/UBSan, échec => code retour 1
make fuzz       # 100 000 parties de robot, invariants après chaque événement
make images     # images PNG ×4 dans tests/out/
```

La cible `make test` compile directement les sources de `../maze` (aucune
copie) avec :

```bash
g++ -std=c++17 -O1 -g -Wall -Wextra \
    -fsanitize=address,undefined -fno-omit-frame-pointer -I../maze \
    ../maze/maze_logic.cpp ../maze/game.cpp ../maze/renderer.cpp \
    ../maze/display_iface.cpp sim_display.cpp sim_main.cpp -o sim
```

L'écran simulé est un tampon 200 × 200, palette 0=blanc, 1=noir, 2=rouge,
3=jaune ; toute commande hors 0..199 incrémente un compteur de débordements
et le test exige zéro. Les PNG sont écrits sans dépendance externe (encodeur
PNG minimal intégré), agrandis ×4.

Contenu de la suite :

- conversions (coin, pix) pour 5×5, 15×15, 10×7, 7×10, double désignation des
  coins, exemples du §5.2, directions, voies extérieures ;
- règles du menu (exhaustif entrée/sortie, 100 000 corrections en cascade),
  navigation, saut des valeurs interdites, bornes ;
- génération : 5 000 graines par taille (20 000 labyrinthes), boucles 0 à 10,
  entrées/sorties sur toutes les arêtes et coins, directions 1/2/3, connexité,
  sortie, symétrie, reproductibilité ;
- machine à états : A/B, retour, repassage sur rouge, blocages, abandon,
  validation de START, sortie, coin à deux voies, sortie refusée en B ;
- régénération : 10 000 parties de robot (2 000 par `n_reset` ∈ {0,1,2,5,32}),
  contrôle exact après chaque régénération, échec simulé et reprise ;
- rendu : toutes les tailles 5×5 à 15×15, deux modes, par sondage de pixels
  (cadre, interruptions, murs/connexions, rectangles, cercle, trajets) ;
- mémoire : `sizeof(Game)` affiché, zéro allocation dynamique.

Le fuzz vérifie après chaque événement : position valide, masques symétriques
et sans ouverture vers l'extérieur, rouge/imposé ouvert, un seul segment
provisoire au plus, connexité depuis l'entrée, sortie atteignable depuis la
position courante, compteurs dans leurs bornes. En cas de violation, la
graine et la suite d'événements sont affichées.

## 10. Vérification manuelle sur la carte

Liste à exécuter à la main (aucun mode test n'est embarqué) :

1. Labyrinthes 5 × 5, 15 × 15 et 10 × 7 sur toute la surface, cadre complet.
2. Entrée sur chacune des quatre arêtes ; sortie sur chacune des quatre
   arêtes en respectant la règle d'arête.
3. `pix` extrêmes (1 et L), cellules de coin et leurs deux désignations ;
   valeurs interdites sautées ; correction en cascade quand une dimension
   change.
4. Convention de comptage du §5.2 sur les quatre coins.
5. `dir_in`/`dir_out` : bon voisin, connexion imposée ouverte, valeurs de coin
   sautées.
6. Modes 0 et 1 : mêmes possibilités de déplacement.
7. `n_boucles = 0` puis plusieurs boucles ; cohérence du rendu.
8. Cercles et rectangles visibles et centrés à 5 × 5 comme à 15 × 15.
9. Pas provisoire puis validation ; pas provisoire puis retour ; autres
   boutons ignorés pendant le jaune.
10. Retour sur un trajet rouge puis annulation : le rouge est restitué.
11. `n_reset = 0` (aucun changement), `n_reset = 1` et `5` (régénération au
    bon nombre de pas, trajet rouge intact, aucune zone vide).
12. Boucle partiellement parcourue : la partie rouge reste, le reste change.
13. Boucle non parcourue : elle réapparaît ailleurs après régénération.
14. Sortie impossible tant que le dernier pas n'est pas validé ; sortie par
    une ou deux voies en coin ; écran de fin avec temps ; retour au menu.
15. Abandon par cinq validations en A ; compteur remis à zéro par un
    déplacement ; START puis nouveau labyrinthe différent ; aucun redémarrage.
16. Appui prolongé pendant un rafraîchissement : aucune action doublée.

## 11. Ce qui ne peut pas être vérifié sans la carte

- Le pilote réel, le mappage effectif des quatre couleurs vers le contrôleur,
  l'orientation exacte du panneau, la durée réelle d'un rafraîchissement
  (documentation : 20 s complet, 15 s rapide) et l'absence d'artefacts.
- Le comportement électrique de GP1, GP2, GP3, GP19 et GP20 (contact à la
  masse, pull-ups, conflit USB, strapping de GP3, LED).
- La procédure de téléversement avec le joystick sur GP19/GP20 et le réglage
  « USB CDC On Boot ».
- Le ressenti de jeu avec 15 à 20 s par action (le cahier des charges évoque
  5 à 10 s ; le panneau 4 couleurs est plus lent).
- L'usage réel de la mémoire à l'exécution (seule l'estimation de compilation
  est connue).
- La gravure du texte sur le panneau : la police 5 × 7 est rendue en pixels
  par le logiciel ; la lisibilité réelle est à confirmer.

Conseil de mise en route : commencer par un petit croquis qui affiche quatre
bandes de couleur et le nom du bouton pressé, pour valider l'écran et le
câblage avant le jeu complet.

## 12. Hypothèses et choix documentés

- **Bouclage du menu** : Haut/Bas boucle de la dernière ligne vers la première
  (et inversement) ; Gauche/Droite ne boucle jamais, conformément au cahier
  des charges.
- **Chronomètre** : démarre après le `flush()` du premier écran de jeu, et
  s'arrête à l'instant de l'appui qui fait sortir.
- **Cercle aux bords** : décalage d'au plus 1 px vers l'intérieur pour
  préserver le cadre de 2 px ; diamètre et contour inchangés.
- **Validation en A sans segment** : rafraîchit l'écran et compte pour
  l'abandon (conforme au §11.2 et au test §5.4.7), même si l'image est
  identique.
- **Couleurs** : rouge et jaune sont des teintes pleines dans la simulation ;
  le panneau e-paper les rend selon ses pigments (rouge sombre, jaune ocre).
- **Pilote** : copie officielle Waveshare plutôt que GxEPD2, le panneau 4
  couleurs 1,54" étant directement pris en charge par la démonstration
  officielle de cette carte. Seul `USE_DEBUG` a été mis à 0 dans `Debug.h`.
- **Bibliothèque** : aucune bibliothèque Arduino externe ; le pilote est dans
  le croquis, ce qui garantit la version utilisée.
- **Injection d'échec de régénération** : méthode publique réservée aux tests
  PC (`setRegenerationFailureInjection`) ; elle n'est jamais appelée par le
  croquis et ne constitue pas un mode de test embarqué.
