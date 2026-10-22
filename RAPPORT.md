# Rapport — compilation, simulation et vérifications

Projet : labyrinthe dynamique sur **Waveshare ESP32-S3-ePaper-1.54G**.
Cahier des charges : `instructions/plan.md` (v2).
Consignes de vérification : `instructions/compilation_et_simulation.md`.

Ce rapport ne contient que des résultats réellement exécutés dans
l'environnement décrit ci-dessous. Les points qui ne peuvent pas être validés
sans la carte sont listés à la fin, sans être minimisés.

## 1. Environnement et versions

| Élément | Version |
| --- | --- |
| Machine | Linux 6.18.33.1-microsoft-standard-WSL2, Debian 13 (x86-64) |
| arduino-cli | 1.5.1 (commit 01f3d4f2b) |
| Core Arduino-ESP32 | **esp32:esp32 3.3.12** |
| Compilateur des tests | g++ 15.3.0 (toolchain portable Bootlin x86-64 glibc stable 2026.08-1) |
| GNU Make | 4.4.1 |
| Sanitizers | AddressSanitizer + UndefinedBehaviorSanitizer |

La machine hôte ne disposait ni de `g++`, ni de `make`, ni de `arduino-cli`.
Ils ont été installés en espace utilisateur dans `/tmp` pour cette session :
arduino-cli via le script officiel, toolchain g++ via
`toolchains.bootlin.com`, make via le paquet Debian extrait. Le `Makefile`
livré reste portable : il utilise `g++` par défaut. Les commandes exactes
utilisées ici sont reproduites plus bas.

Le pilote écran est la **copie officielle Waveshare** pour cette carte :

- dépôt : `waveshareteam/ESP32-S3-ePaper-1.54G` ;
- fichier : `Example/Arduino_3.2.0/examples/08_E_paper_test/` ;
- commit : `55eb1278f482c0cfd1e0cba5611a4132022eea48` (2026-09-20) ;
- source et date rappelées en tête de chaque fichier copié ;
- seule modification : `USE_DEBUG = 0` dans `Debug.h` (journal série du
  pilote désactivé).

Aucune bibliothèque Arduino externe n'est utilisée.

## 2. Compilation Arduino

Commande exécutée :

```bash
arduino-cli compile \
  --fqbn 'esp32:esp32:esp32s3:PSRAM=opi,FlashSize=8M,PartitionScheme=default_8MB,CDCOnBoot=default,USBMode=hwcdc' \
  --warnings all --output-dir build maze
```

Sortie finale (code retour 0, **aucun avertissement, aucune erreur**) :

```
Sketch uses 342676 bytes (10%) of program storage space. Maximum is 3342336 bytes.
Global variables use 34824 bytes (10%) of dynamic memory, leaving 292856 bytes for local variables. Maximum is 327680 bytes.
```

- Flash : 342 676 octets sur 3 342 336 (marge 2 999 660 octets).
- RAM globale : 34 824 octets sur 327 680 (marge 292 856 octets).
- Options : PSRAM OPI (8 Mo), flash 8 Mo, partition 8 Mo (3 Mo APP),
  USB CDC désactivé au boot, USB en mode Hardware CDC/JTAG.
- Le croquis n'utilise ni PSRAM ni le port USB natif à l'exécution ; GP19/GP20
  sont réservés au joystick.

Les quatre modules portables (`maze_logic.cpp`, `game.cpp`, `renderer.cpp`,
`display_iface.cpp`) ont aussi été compilés à part avec `-Wall -Wextra` :
aucun avertissement.

## 3. Tests automatiques sur PC

Commandes exactes exécutées (depuis `tests/`, avec la toolchain temporaire) :

```bash
TC=/tmp/opencode/x86-64--glibc--stable-2026.08-1
export LD_LIBRARY_PATH=$TC/lib64:$TC/x86_64-buildroot-linux-gnu/lib64
make CXX=$TC/bin/x86_64-buildroot-linux-gnu-g++ \
     CXXFLAGS='-std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer' \
     LDFLAGS="-fsanitize=address,undefined -Wl,-rpath,$TC/lib64 -Wl,-rpath,$TC/x86_64-buildroot-linux-gnu/lib64" \
     test
# mêmes variables CXX/CXXFLAGS/LDFLAGS pour : make ... fuzz, make ... images
```

Les sources compilées sont **directement** `../maze/maze_logic.cpp`,
`../maze/game.cpp`, `../maze/renderer.cpp`, `../maze/display_iface.cpp` ;
aucune copie du code du jeu n'est utilisée.

Résultat final (code retour 0) :

```
Résultat : 235303 PASS, 0 FAIL
```

Tableau par groupe (tous PASS) :

| Groupe | Résultat |
| --- | --- |
| Conversions et directions | PASS |
| Paramètres et menu (règles, exhaustif + 100 000 corrections) | PASS |
| Navigation du menu et machine à états du menu | PASS |
| Génération (5 000 graines × 4 tailles, boucles 0..10) | PASS |
| Machine à états et déplacements | PASS |
| Régénération dynamique (10 000 parties de robot) | PASS |
| Rendu (palette, cadre, marqueurs, cercle, trajets) | PASS |
| Mémoire et sûreté | PASS |

Points particuliers vérifiés par la suite :

- conversions et double désignation des coins pour `5×5`, `15×15`, `10×7`,
  `7×10`, exemples exacts du §5.2 ;
- règles d'arête physique entrée/sortie, `coin_out`/`pix_out` interdits,
  valeurs de `dir` sans voisin sautées ;
- 100 000 états de paramètres aléatoires corrigés en cascade puis validés ;
- 20 000 générations (5 000 par taille), entrées/sorties sur toutes les
  arêtes et coins, directions 1/2/3, boucles 0 à 10, connexité à 100 %,
  sortie atteignable, masques symétriques, connexions imposées ouvertes,
  nombre cyclomatique exact ;
- reproductibilité à graine égale et diversité des graines ;
- `type_aff` sans effet sur la structure logique ;
- états A/B, retour, repassage sur rouge, blocages sans rafraîchissement,
  abandon par cinq validations, START non compté, sortie refusée en état B,
  sortie par une ou deux voies en coin, fin et retour au menu ;
- régénération : 2 000 parties par `n_reset` ∈ {0, 1, 2, 5, 32}, contrôle
  après chaque événement, conservation du rouge et de la position, boucles
  exactes, aucun changement si `n_reset = 0`, échec simulé conservant
  l'ancien labyrinthe avec reprise à la validation suivante ;
- rendu : toutes les tailles de 5×5 à 15×15, deux modes, par sondage de
  pixels (cadre, interruptions de sortie, connexions/murs, rectangles
  départ/sortie, cercle validé/provisoire, trajet rouge/jaune) ;
- zéro débordement hors 0..199, uniquement les quatre couleurs.

Mémoire :

```
sizeof(Game)=816 octets, sizeof(Maze)=700 octets
zéro allocation dynamique dans les modules
```

Vérification complémentaire des symboles des quatre modules (nm) :

```
AUCUN symbole malloc/calloc/realloc/operator new dans les quatre modules
```

## 4. Fuzz

Commande exécutée :

```bash
make ... fuzz
```

Sortie finale (code retour 0, durée ~54 s) :

```
FUZZ PASS : 100000 parties, invariants respectés
```

Après **chaque** événement, le fuzz vérifie : position valide, masques
symétriques et sans ouverture hors grille, rouge et imposé ouverts, un seul
segment provisoire, connexité depuis l'entrée, sortie atteignable depuis la
position courante, compteurs dans leurs bornes. En cas de violation, la
graine et la suite d'événements sont affichées (aucune violation observée).

## 5. Images de contrôle

Commande exécutée :

```bash
make ... images
```

25 PNG ×4 sont écrits dans `tests/out/` :

```
maze_5x5_mode0/1, maze_10x7_mode0/1, maze_15x15_mode0/1
entry_edge1..4, exit_edge1..4, exit_corner
menu_default, menu_col1_selected, menu_col2_selected, menu_error
play_pending, play_validated, regen_before, regen_after, regen_failure
end_screen
```

Images ouvertes et examinées. Constats :

- **Menu** : deux colonnes de six lignes, valeurs lisibles, ligne sélectionnée
  entièrement inversée (fond noir, texte blanc) sur les deux colonnes ;
  START centré ; bandeau rouge « ERREUR PARAM » lisible.
- **Labyrinthes** : 5×5, 10×7 et 15×15 remplissent les 200 × 200, cadre 2 px
  complet, interruption blanche sur le ou les côtés extérieurs de la sortie.
  En mode 0, le trait de sortie noir rejoint bien le bord ; en mode 1,
  l'ouverture est blanche et les murs 3 px sont jointifs au cadre. Aucun
  élément coupé ni couleur parasite.
- **Entrées/sorties** : cercle de position et rectangles de départ/sortie
  centrés, y compris aux quatre arêtes et dans un coin (deux interruptions).
- **Partie en cours** : segment provisoire jaune et cercle à remplissage
  rouge/contour jaune ; après validation, segment rouge et cercle à
  remplissage jaune/contour rouge.
- **Régénération** : avant/après, le trajet rouge et la position sont
  identiques, le reste du labyrinthe change réellement.
- **Échec de régénération** : marqueur de départ noir avec intérieur rouge,
  nettement visible.
- **Écran de fin** : « SORTIE ! » en rouge, temps `mm:ss` en noir, bandeau
  jaune « VALIDATION = MENU » ; quatre couleurs seulement.

Les images sont volontairement des PNG non compressés (encodeur PNG minimal
sans zlib) : fichiers d'environ 1,9 Mo, sans dépendance externe.

## 6. Commandes exactes utilisées (résumé)

```bash
# Compilation Arduino
arduino-cli compile \
  --fqbn 'esp32:esp32:esp32s3:PSRAM=opi,FlashSize=8M,PartitionScheme=default_8MB,CDCOnBoot=default,USBMode=hwcdc' \
  --warnings all --output-dir build maze

# Tests PC (toolchain temporaire de cette session)
cd tests
make clean
TC=/tmp/opencode/x86-64--glibc--stable-2026.08-1
export LD_LIBRARY_PATH=$TC/lib64:$TC/x86_64-buildroot-linux-gnu/lib64
make CXX=$TC/bin/x86_64-buildroot-linux-gnu-g++ \
  CXXFLAGS='-std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS="-fsanitize=address,undefined -Wl,-rpath,$TC/lib64 -Wl,-rpath,$TC/x86_64-buildroot-linux-gnu/lib64" \
  test
make ... fuzz
make ... images
```

Sur une machine disposant de g++ et de make, les commandes se réduisent à :

```bash
cd tests
make test
make fuzz
make images
cd ..
arduino-cli compile --fqbn 'esp32:esp32:esp32s3:PSRAM=opi,FlashSize=8M,PartitionScheme=default_8MB,CDCOnBoot=default,USBMode=hwcdc' --warnings all --output-dir build maze
```

## 7. Ce qui reste non vérifié sans la carte

Conformément au §9 des consignes :

- le pilote réel, le mappage effectif des couleurs vers le contrôleur, la
  durée réelle d'un rafraîchissement et l'absence d'artefacts. La
  documentation Waveshare annonce **20 s en complet** et **15 s en rapide** ;
  le code utilise `Init_Fast` + `Clear` + `Display` et ces durées restent à
  mesurer. Le cahier des charges évoque 5 à 10 s par action : l'écart est
  matériel, pas logiciel ;
- le comportement des broches GP1, GP2, GP3, GP19 et GP20 (contact à la
  masse, pull-ups, conflit USB natif, strapping GP3, LED) ;
- la procédure de téléversement avec le joystick sur GP19/GP20 et le réglage
  « USB CDC On Boot » (le code utilise UART0 pour un éventuel débogage) ;
- le ressenti de jeu avec 15 à 20 s par action ;
- l'usage réel de la mémoire à l'exécution (seule l'estimation de compilation
  est connue) ;
- la lisibilité réelle de la police 5 × 7 sur le panneau et l'orientation
  physique exacte de l'image (la rotation 0 du pilote officiel est utilisée).

## 8. Écarts, hypothèses et limitations logicielles

- **Bouclage du menu** : Haut/Bas boucle en fin de liste ; Gauche/Droite ne
  boucle jamais (cahier des charges).
- **Rafraîchissement en validation A** : l'écran est rafraîchi même si
  l'image est identique, comme demandé au §11.2 et au test §5.4.7.
- **Cercle aux bords** : décalé d'au plus 1 px vers l'intérieur sur les
  petites cellules de bord pour laisser le cadre de 2 px intact ; diamètre
  et contour inchangés.
- **Chronomètre** : démarre après le `flush()` du premier écran de jeu.
- **Échec de régénération** : l'ancien labyrinthe est conservé et la
  tentative est refaite à la validation suivante, avec le signal « départ
  noir et rouge ».
- **Mode 1** : les murs sont dessinés sur les frontières internes fermées ;
  le cadre extérieur 2 px joue le rôle de mur extérieur, avec interruption
  blanche à la sortie.
- **Pilote** : copie officielle plutôt que GxEPD2 ; la version est figée dans
  le dépôt (commit ci-dessus). Aucun gestionnaire de bibliothèques n'est
  requis.
- **Injection d'échec** : `Game::setRegenerationFailureInjection()` est une
  méthode de test, jamais appelée par le croquis ; il n'y a pas de mode test
  embarqué.
- Aucun échec de test ou de compilation ne reste à signaler dans cet
  environnement ; les points matériels ci-dessus restent ouverts.
