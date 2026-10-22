# Cahier des charges — Jeu de labyrinthe dynamique sur ESP32-S3 avec écran e-paper 4 couleurs

Version 2, révisée après arbitrage du demandeur. Les points marqués **\[H#\]** sont des hypothèses non encore confirmées (voir §20).

## 1. Objectif

Développer un jeu de labyrinthe complet sur la carte **Waveshare ESP32-S3-ePaper-1.54G** (ESP32-S3-PICO-1 N8R8, écran e-paper 1,54" 200 × 200 px, quatre couleurs : blanc, noir, rouge, jaune), en C++ sous Arduino IDE.

Le joueur se déplace avec un joystick à cinq contacts (nord, sud, est, ouest, centre). Il doit sortir du labyrinthe. Après chaque `n_reset` pas validés, toute la partie non parcourue du labyrinthe est régénérée, boucles comprises, sans toucher au trajet validé.

Le code fourni doit être complet, compilable, commenté, sans pseudocode ni fonction essentielle laissée à écrire.

## 2. Matériel et branchements

### 2.1 Carte et écran

- Carte : Waveshare ESP32-S3-ePaper-1.54G. Écran 200 × 200, quatre couleurs. Les broches de l’écran (SPI, DC, CS, RST, BUSY, éventuelle commande d’alimentation du panneau ou du PMIC) sont internes à la carte. **L’agent doit les reprendre du schéma ou de l’exemple officiel Waveshare, sans les deviner.**
- L’agent identifie la bibliothèque ou le pilote compatible avec ce panneau précis (en priorité le code de démonstration officiel Waveshare ; GxEPD2 seulement si le panneau est effectivement pris en charge), indique la version, et vérifie le mappage des quatre couleurs.
- Toute la configuration matérielle (broches, pilote, couleurs) est isolée dans une section unique en tête du code.
- Carte Arduino : « ESP32S3 Dev Module », PSRAM activée (OPI). Le core Arduino-ESP32 et sa version sont à documenter.

### 2.2 Joystick (appui = contact à la masse, `INPUT_PULLUP`)

| Fonction | Direction | GPIO |
| --- | --- | --- |
| Centre | Validation | GP20 |
| Nord | Haut | GP19 |
| Sud | Bas | GP3 |
| Est | Droite | GP2 |
| Ouest | Gauche | GP1 |

Le commun du joystick va sur la broche **GND** voisine du connecteur.

### 2.3 Points d’attention à documenter

- **GP19 et GP20** sont les broches USB natives (D− et D+) de l’ESP32-S3. Les utiliser comme GPIO rend le port USB natif indisponible à l’exécution. Documenter la marche à suivre pour téléverser (joystick débranché, ou mode bootloader avec BOOT) et le réglage « USB CDC On Boot ». Le port série est donc facultatif (§16).
- **GP3** est une broche de strapping de l’ESP32-S3 et partage la LED de la carte. Ne pas appuyer sur Sud à la mise sous tension.
- Vérifier dans le schéma qu’aucune des cinq broches n’entre en conflit avec l’écran, la carte SD, l’audio ou l’I2C.

## 3. Paramètres du jeu

| Paramètre | Signification | Valeurs |
| --- | --- | --- |
| `n_x` | Cellules horizontales | 5 à 15 |
| `n_y` | Cellules verticales | 5 à 15 |
| `coin_in` | Coin de référence de l’entrée | 1 à 4 |
| `pix_in` | Cellule d’entrée sur l’arête, 1 à L | L = longueur de l’arête |
| `dir_in` | Voisin du premier pas | 1 à 3 |
| `coin_out` | Coin de référence de la sortie | 1 à 4 |
| `pix_out` | Cellule de sortie sur l’arête, 1 à L | L = longueur de l’arête |
| `dir_out` | Voisin par lequel on arrive à la cellule de sortie | 1 à 3 |
| `n_reset` | Pas validés entre deux régénérations | 0 à 32 (0 = jamais) |
| `n_boucles` | Nombre de boucles visé | 0 à 10 |
| `type_aff` | 0 = trajets possibles, 1 = murs infranchissables | 0 ou 1 |

Valeurs par défaut suggérées : 10, 7, 1, 1, 2, 3, 1, 2, 5, 3, 0.

`type_aff` est fixé au lancement et ne change pas pendant la partie.

## 4. Menu de configuration

- Affichage sur deux colonnes de six lignes : colonne 1 = `n_x`, `n_y`, `coin_in`, `pix_in`, `dir_in`, `coin_out` ; colonne 2 = `pix_out`, `dir_out`, `n_reset`, `n_boucles`, `type_aff`, **START**. Chaque ligne fait environ 33 px de haut. La ligne sélectionnée est en inversé ou encadrée, bien visible.
- **Haut / Bas** : ligne précédente ou suivante, dans l’ordre ci-dessus (la fin de la colonne 1 enchaîne sur la colonne 2).
- **Gauche / Droite** : valeur précédente ou suivante, **sans bouclage** (butée aux limites). Un appui = un pas.
- **Validation** : n’agit que sur la ligne START. L’appui sur START n’entre pas dans le compteur d’abandon.
- Chaque appui efficace redessine tout l’écran (pas de rafraîchissement partiel, pas de regroupement). Les appuis sans effet ne rafraîchissent pas.

### 4.1 Valeurs interdites : elles sont sautées

Les valeurs interdites ne sont jamais affichées : Gauche et Droite passent directement à la valeur permise suivante (exemple : coin 1, 2, 4 si 3 est pris).

- `pix_in` : 1 à L(`coin_in`).
- `dir_in`, `dir_out` : une valeur est permise seulement si le voisin correspondant existe dans la grille (§6).
- **Règle d’arête** : l’entrée et la sortie ne doivent avoir aucune arête physique en commun (§5.3). Une valeur de `coin_out` n’est permise que s’il existe au moins un `pix_out` valide ; une valeur de `pix_out` n’est permise que si elle respecte la règle.
- **Correction en cascade** : quand `n_x`, `n_y`, `coin_in`, `pix_in` ou `dir_in` changent, tous les paramètres dépendants sont recalculés et, si la valeur courante devient interdite, remplacés par la valeur permise la plus proche (`dir` retombe sur 2, toujours permis). Idem pour `pix_in`, `pix_out`, `dir_in`, `dir_out` quand une dimension change.
- `validateParameters()` revérifie tout au lancement. En cas d’échec (cas normalement impossible), l’écran affiche un message d’erreur et reste dans le menu.

## 5. Repérage : coins, arêtes, conversion `(coin, pix)` en `(x, y)`

### 5.1 Repère et numérotation

Les coordonnées logiques `(x, y)` vont de `(0, 0)` en haut à gauche à `(n_x−1, n_y−1)` en bas à droite. Le sens de comptage est **trigonométrique** (antihoraire sur l’écran), cohérent avec la numérotation des coins.

Coins : 1 = haut gauche, 2 = bas gauche, 3 = bas droite, 4 = haut droite.

L’arête `a` va du coin `a` au coin `a+1` (le coin 4 vers le coin 1). Une arête a un point de départ, un point d’arrivée, et une normale intérieure.

| Arête | Côté | Départ → arrivée | Tangente t | Normale intérieure n | Normale extérieure | Longueur L |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | gauche | haut → bas | (0, +1) | (+1, 0) | (−1, 0) | n\_y |
| 2 | bas | gauche → droite | (+1, 0) | (0, −1) | (0, +1) | n\_x |
| 3 | droite | bas → haut | (0, −1) | (−1, 0) | (+1, 0) | n\_y |
| 4 | haut | droite → gauche | (−1, 0) | (0, +1) | (0, −1) | n\_x |

Vu de face, en regardant la normale intérieure : le point de départ est à gauche (+90°), la normale est à 0°, le point d’arrivée est à droite (−90°). Le sens de départ vers arrivée est donc celui de comptage.

### 5.2 Conversion `(coin, pix)` en cellule

`pix` va de 1 à L. Il désigne la `pix`-ième cellule du bord à partir du coin, en comptant à partir de 1. La valeur 0 est l’origine, jamais une cellule.

- coin 1 : `(0, pix−1)`
- coin 2 : `(pix−1, n_y−1)`
- coin 3 : `(n_x−1, n_y−pix)`
- coin 4 : `(n_x−pix, 0)`

Exemples pour `n_x = 10`, `n_y = 7` : coin 1 pix 1 = (0,0) ; coin 1 pix 7 = (0,6) ; coin 2 pix 1 = (0,6) ; coin 2 pix 10 = (9,6) ; coin 3 pix 1 = (9,6) ; coin 3 pix 7 = (9,0) ; coin 4 pix 1 = (9,0) ; coin 4 pix 10 = (0,0).

Une cellule de coin possède donc deux désignations. La fonction centrale est `convertEdgeToCell(coin, pix)`.

### 5.3 Arêtes physiques d’une cellule

`edgeSet(x, y)` est l’ensemble des arêtes que touche la cellule : arête 1 si `x = 0`, arête 2 si `y = n_y−1`, arête 3 si `x = n_x−1`, arête 4 si `y = 0`. Une cellule de coin touche deux arêtes.

**Règle** : `edgeSet(entrée) ∩ edgeSet(sortie) = ∅`. Elle implique entrée ≠ sortie et `coin_in ≠ coin_out`. **\[H2\]**

## 6. Directions `dir_in` et `dir_out`

Pour une cellule désignée par `(coin, pix)` sur l’arête `a` (tangente `t`, normale intérieure `n`), `dir` désigne le voisin :

- `dir = 1` : voisin côté point de départ, cellule − t (+90°) ;
- `dir = 2` : voisin vers l’intérieur, cellule + n (0°) ;
- `dir = 3` : voisin côté point d’arrivée, cellule + t (−90°).

Une valeur est permise seulement si ce voisin est dans la grille ; pour une cellule de coin, deux valeurs seulement sont donc permises. **\[H3\]**

- **Entrée** : `dir_in` désigne le voisin du premier pas. La connexion entrée–voisin est **imposée ouverte** ; les autres connexions de la cellule restent libres.
- **Sortie** : `dir_out` désigne le voisin par lequel on arrive dans la cellule de sortie. Cette connexion est **imposée ouverte** ; les autres restent libres.
- Ces deux connexions imposées sont intégrées à la structure logique, à la génération et à chaque régénération.

### 6.1 Ouvertures vers l’extérieur

La sortie est une cellule intérieure. Le joueur quitte le labyrinthe par la normale extérieure de chaque arête que touche cette cellule : **une** voie si elle est sur une arête, **deux** si elle est dans un coin (`edgeSet(sortie)`).

L’entrée est une cellule intérieure sans ouverture vers l’extérieur : le joueur ne peut pas sortir par l’entrée.

## 7. Structure logique et génération

### 7.1 Structure

Le labyrinthe est un graphe non orienté de cellules, dont les arêtes (connexions ouvertes) sont stockées dans des masques de bits par cellule (N, E, S, O), tenus symétriques par une seule fonction `setEdge()`. Tableaux statiques 15 × 15, sans allocation dynamique :

- `open[y][x]` : connexions ouvertes ;
- `red[y][x]` : connexions validées par le joueur (figées) ;
- `forced[y][x]` : connexions imposées par `dir_in` et `dir_out`.

Cette structure est indépendante du rendu ; `type_aff` ne change jamais les déplacements possibles.

### 7.2 Algorithme unique pour la génération et la régénération

1. **Base** `E0` : toutes les connexions rouges et les deux connexions imposées sont ouvertes. Au départ, `E0` ne contient que les connexions imposées.
2. **Connexité** : union-find sur les 225 cellules. Mélanger (Fisher-Yates, `esp_random()`) la liste de toutes les autres arêtes candidates de la grille ; l’ouvrir si elle relie deux composantes distinctes (Kruskal aléatoire). À la fin, le graphe est connexe : **toutes les cellules sont accessibles depuis l’entrée**, partout dans la grille.
3. **Boucles** : voir §8.
4. **Contrôle** : `validateMazeConnectivity()` (parcours en largeur depuis l’entrée : 100 % des cellules atteintes) et `validateExitReachability()` (parcours depuis la **position courante** : la cellule de sortie est atteinte et possède ses ouvertures extérieures ; toutes les arêtes rouges et imposées sont ouvertes). En cas d’échec, nouvelle tentative (50 maximum).
5. Chaque partie démarre avec une nouvelle graine (`esp_random()`), donc « abandonner puis START » donne un autre labyrinthe avec les mêmes paramètres.

Garanties : le trajet rouge est dans `E0`, jamais retiré ; l’accès à la sortie est assuré par la connexité et la connexion `dir_out` imposée ; les culs-de-sac sont permis mais toujours reliés.

## 8. Boucles

**Définition** : le nombre de boucles est le nombre cyclomatique du graphe, `μ = A − S + C` (arêtes ouvertes, sommets, composantes). Pour un graphe connexe, `μ = A − S + 1` ; la fonction `countCycles()` le calcule.

- Après l’étape 2, soit `μ0` le nombre de cycles déjà contenus dans `E0` (cycles du trajet rouge, ou entre connexions imposées). Le nombre d’arêtes à ajouter est `k = max(0, n_boucles − μ0)`.
- Choisir `k` arêtes fermées au hasard et les ouvrir. Règle esthétique souple : éviter de créer un bloc ouvert 2 × 2 ; relâcher cette règle si les candidats manquent.
- Si `μ0 > n_boucles`, aucune arête n’est ajoutée (le trajet validé est prioritaire).
- Si `n_boucles` ne peut pas être atteint, le labyrinthe reste valide et l’écart est signalé dans un commentaire de code et, si le débogage est activé, sur le port série.
- À chaque régénération, les boucles non figées sont supprimées et recréées ailleurs. Une partie d’une boucle déjà parcourue reste (elle est rouge) ; le reste est regénéré.

## 9. Géométrie et dessin

### 9.1 Grille

La grille occupe les 200 × 200 px. Les cellules ne sont pas carrées. Frontières : `X[i] = round(i·200 / n_x)` et `Y[j] = round(j·200 / n_y)` ; les cellules font donc, par exemple, 15 ou 16 px pour `n_x = 13`. Fonctions centralisées : `computeCellCoordinates()`, centre de cellule `((X[i]+X[i+1])/2, (Y[j]+Y[j+1])/2)`.

Toutes les coordonnées sont bornées à 0..199.

### 9.2 Épaisseurs

| Élément | Épaisseur |
| --- | --- |
| Cadre noir extérieur | 2 px (à l’intérieur des 200 × 200) |
| Trajet noir, rouge ou jaune | 5 px |
| Mur infranchissable | 3 px |
| Cercle (position courante) | contour = 25 % du diamètre, arrondi au pixel, 1 px minimum |

### 9.3 Éléments

- **Cercle de position** : diamètre = 80 % de `min(largeur, hauteur de cellule)`, centré dans la cellule.
- **Départ et arrivée** : rectangle à 50 % de la taille de la cellule, centré (pas de cercle).

### 9.4 Cadre et sortie

Le cadre noir fait tout le tour. Seule la cellule de sortie a, sur son ou ses côtés extérieurs, une interruption blanche du cadre sur toute la longueur de la cellule.

### 9.5 Représentations

Ordre de dessin, toujours **tout l’écran redessiné** depuis l’état en mémoire : fond blanc, cadre, structure (selon `type_aff`), trajet rouge, segment jaune, marqueurs.

- **`type_aff = 0`** : chaque connexion ouverte est un trait noir de 5 px reliant les centres de cellules. À la sortie, le trait noir va du centre de la cellule jusqu’au bord de l’écran, par la ou les voies de sortie. Aucune connexion fermée n’est dessinée.
- **`type_aff = 1`** : chaque frontière entre deux cellules **sans** connexion est un mur noir de 3 px, jointif au cadre. Les ouvertures sont les passages. Le trajet rouge et le segment jaune sont des traits de 5 px centre à centre, par-dessus.

## 10. Couleurs et marqueurs

| Couleur | Usage |
| --- | --- |
| Blanc | Fond ; passages en mode 1 ; ouverture de sortie dans le cadre |
| Noir | Cadre ; trajets (mode 0) ; murs (mode 1) ; rectangle de départ |
| Rouge | Trajet validé ; rectangle de sortie |
| Jaune | Segment provisoire (un seul) |

Marqueur du joueur (cercle) :

- position **non validée** (segment provisoire en cours) : contour jaune, remplissage rouge ;
- position **validée** : contour rouge, remplissage jaune.

Au début, le joueur est à la cellule d’entrée, état validé. Le rectangle de départ est noir plein ; le rectangle de sortie est rouge plein. Si le joueur est sur la cellule de départ ou de sortie, le cercle est dessiné par-dessus.

**Échec de régénération** : le rectangle de départ devient noir avec un intérieur rouge (rectangle intérieur à 50 %). Le cercle peut le recouvrir si le joueur est sur la cellule de départ. **\[H5\]**

## 11. Déroulement de la partie

### 11.1 États

Le joueur est dans l’un de ces états :

- **A** : aucun segment provisoire ;
- **B** : un segment provisoire (jaune) existe, du joueur `cellule précédente` vers `cellule courante`.

Il n’y a jamais plus d’un segment provisoire.

### 11.2 Boutons dans l’état A

- **Direction** : si le voisin existe et que la connexion est ouverte, on crée le segment provisoire (état B) et on redessine tout. Sinon l’appui est ignoré (pas de rafraîchissement).
- **Direction vers l’extérieur, depuis la cellule de sortie** : le joueur sort, la partie est terminée (§11.5). Le franchissement est définitif.
- **Validation** : l’écran est rafraîchi, sans autre effet, et l’appui compte pour l’abandon (§13). **\[H4\]**

### 11.3 Boutons dans l’état B

- **Validation** : le segment provisoire devient rouge et figé, le cercle passe en état validé, `n_valid` est incrémenté, le compteur d’abandon est remis à zéro, puis le déclenchement éventuel de la régénération est examiné (§12). Retour à l’état A.
- **Retour** (uniquement la direction qui ramène vers la cellule précédente) : le segment provisoire est annulé, la position précédente est restaurée, tout est redessiné, retour à l’état A. Aucun pas n’est compté.
- Toute autre direction et tout autre appui sont ignorés.

### 11.4 Retour sur un trajet déjà validé

Le joueur peut repasser sur une connexion rouge. Ce déplacement est un nouveau pas : segment jaune provisoire **par-dessus** le rouge, à valider ou à annuler comme les autres. L’annulation restitue le rouge (jamais le blanc ni le noir) : comme tout est redessiné depuis l’état, un segment rouge n’est jamais effacé. **\[H1\]** Le pas validé compte pour `n_reset`.

### 11.5 Sortie et fin de partie

Le joueur doit d’abord entrer dans la cellule de sortie par un segment provisoire, puis le valider (état A). Il appuie alors sur une direction extérieure autorisée (§6.1) : la partie se termine aussitôt.

1. Écran de fin : « SORTIE ! », temps écoulé au format mm:ss (mesuré par `millis()` entre la fin du premier affichage et la sortie, rafraîchissements compris), et « Validation = menu ». L’écran reste affiché (maintenu) jusqu’à l’appui sur Validation. **\[H6\]**
2. Appui sur Validation : retour au menu, **sans redémarrage de l’ESP32**, avec les paramètres précédents.

## 12. Régénération dynamique

- Si `n_reset = 0`, le labyrinthe ne change jamais.
- Sinon, après la validation d’un pas, si `n_valid ≥ n_reset`, on appelle `regenerateUnvisitedMaze()`. Il n’y a jamais de régénération en état B (segment jaune) ni après un retour.
- La régénération applique l’algorithme du §7.2 avec `E0` = toutes les connexions rouges (tous les pas validés, sans exception) + connexions imposées. **Tout le reste est régénéré, boucles comprises**, sur l’ensemble de la grille (pas de zone vide).
- Position du joueur : inchangée.
- Succès : `n_valid = 0`, redessin complet, éventuel signal d’échec retiré. La prochaine régénération aura lieu après `n_reset` nouveaux pas validés.
- Échec (filet de sécurité) : l’ancien labyrinthe est conservé, le signal du §10 est affiché, `n_valid` n’est pas remis à zéro, et la régénération est retentée à la validation suivante. **\[H5\]**
- Le résultat doit respecter les priorités du §19.

## 13. Abandon

- Le joueur abandonne en appuyant **cinq fois de suite sur Validation, à l’état A**, sans appui directionnel accepté entre deux.
- Ne comptent pas : l’appui sur START dans le menu, et l’appui de Validation qui confirme un segment provisoire (état B).
- Le compteur est remis à zéro par tout appui directionnel accepté (déplacement ou retour) et par une validation de segment provisoire. Les appuis directionnels ignorés (mur, direction inactive) ne le modifient pas. **\[H4\]**
- Au cinquième appui, retour au menu avec les mêmes paramètres, sans redémarrage. Une nouvelle partie avec START donne un nouveau labyrinthe.
- Règle de désambiguïsation : aucune temporisation. L’état (A ou B) suffit : la validation est prévisible dans tous les cas.

## 14. Boutons et écran

### 14.1 Boutons

- Lecture par boucle bloquante : attendre un appui (niveau bas) ; exécuter l’action ; redessiner si nécessaire ; attendre le relâchement de **tous** les boutons plus environ 30 ms ; reprendre l’attente.
- Pas d’anti-rebond logiciel élaboré : la durée du rafraîchissement (5 à 10 s, pendant laquelle les boutons ne sont pas lus) fait office de filtre. L’attente du relâchement évite seulement qu’un bouton maintenu ne redéclenche une action. **\[H4\]**
- Priorités en cas d’appuis simultanés : Validation, puis Nord, Sud, Est, Ouest.

### 14.2 Écran

- **Pas de rafraîchissement partiel** : chaque action efficace redessine toute l’image dans un tampon (200 × 200 × 2 bits, environ 10 Ko) puis lance un rafraîchissement complet, bloquant. Le programme n’exécute rien d’autre pendant ce temps, y compris la lecture des boutons.
- Un rafraîchissement complet n’est lancé que si l’image a changé. Pas de veille d’écran (interrupteur de la batterie).
- L’agent confirme les temps de rafraîchissement réels et les consigne dans la documentation.

## 15. Architecture logicielle

Fonctions séparées, la logique indépendante du rendu :

- matériel : `initHardware()`, `readButtons()`, `waitButtonPress()`, `waitAllReleased()` ;
- menu : `displayParameters()`, `handleMenuInput()`, `validateParameters()`, `nextAllowedValue()`, `fixDependentParameters()` ;
- coordonnées : `convertEdgeToCell()`, `edgeSet()`, `neighborFromDir()`, `computeCellCoordinates()`, `cellCenter()` ;
- labyrinthe : `generateMaze()`, `generateLoops()`, `countCycles()`, `validateMazeConnectivity()`, `validateExitReachability()`, `regenerateUnvisitedMaze()`, `setEdge()` ;
- rendu : `drawMaze()`, `drawPath()`, `drawPlayer()`, `drawMarkers()`, `drawEndScreen()` ;
- jeu : `startNewGame()`, `handleMovement()`, `validateMovement()`, `undoPendingMovement()`, `checkExit()`, `handleAbandon()`.

Les noms peuvent être adaptés ; les responsabilités restent distinctes.

## 16. Contraintes techniques

- C++ pour Arduino IDE, complet, compilable, commenté.
- Pas d’allocation dynamique inutile ; tableaux statiques 15 × 15 ; contrôle de tous les indices.
- Aucun accès hors limites ; toutes les coordonnées d’écran bornées.
- Toute la configuration matérielle isolée en tête de fichier.
- Pas de mode de test. Un débogage série optionnel est permis, désactivé par défaut (`#define DEBUG 0`) car les broches USB sont occupées par le joystick.
- Les hypothèses matérielles et les limitations sont documentées en tête du code.

## 17. Vérification manuelle sur le matériel

Pas de mode test intégré. L’agent fournit cette liste à exécuter à la main :

1. Labyrinthe 5 × 5, 15 × 15 et 10 × 7 dessinés dans toute la surface, cadre complet.
2. Entrée sur chacune des quatre arêtes ; sortie sur chacune des quatre arêtes (en respectant la règle d’arête).
3. `pix` extrêmes (1 et L) et cellules de coin avec leurs deux désignations ; menu qui saute les valeurs interdites, correction en cascade quand une dimension change.
4. Convention de comptage : exemples du §5.2 sur les quatre coins.
5. `dir_in` et `dir_out` : voisin correct, connexion imposée ouverte, valeurs de coin sautées.
6. Modes `type_aff = 0` et `type_aff = 1` : mêmes possibilités de déplacement.
7. `n_boucles = 0`, puis plusieurs boucles ; recomptage par `countCycles()`.
8. Tous les cercles et rectangles visibles et bien centrés à 5 × 5 comme à 15 × 15.
9. Pas provisoire puis validation ; pas provisoire puis retour ; autres boutons ignorés pendant le jaune.
10. Retour sur un trajet rouge puis annulation : le rouge est restitué.
11. `n_reset = 0` : aucun changement ; `n_reset = 1` et `n_reset = 5` : régénération après le nombre de pas voulu, trajet rouge intact, aucune zone vide.
12. Boucle partiellement parcourue : la partie rouge reste, le reste change.
13. Boucle non parcourue : elle apparaît ailleurs après régénération.
14. Sortie impossible tant que le dernier pas n’est pas validé ; sortie par une ou deux voies en coin ; écran de fin avec temps ; retour au menu.
15. Abandon : cinq validations à l’état A ; le compteur est bien remis à zéro par un déplacement ; START puis nouveau labyrinthe différent ; aucun redémarrage de l’ESP32.
16. Appui prolongé pendant un rafraîchissement : aucune action doublée.

## 18. Livrables

1. Explication de l’architecture générale.
2. Algorithme de génération et garanties de connexité et d’accès à la sortie.
3. Convention de conversion `(coin, pix)` en `(x, y)` et définition de `dir`.
4. Méthode de comptage et de génération des boucles.
5. Stratégie de régénération avec conservation des trajets validés.
6. Schéma de branchement et notes sur GP1, GP2, GP3, GP19 et GP20.
7. Bibliothèques et versions recommandées, version du core Arduino-ESP32.
8. Code Arduino complet, compilable et commenté.
9. Instructions d’installation, de compilation et de téléversement (y compris la procédure avec les broches USB).
10. Liste de vérification manuelle (§17) et limitations matérielles.

L’agent liste explicitement les hypothèses qu’il fait sur le matériel et ne les résout jamais silencieusement. Si une ambiguïté non traitée ici apparaît, il applique l’option la plus simple, la documente en tête du code et dans sa réponse, puis continue.

## 19. Priorités en cas de conflit

1. Ne jamais effacer ni modifier un segment validé.
2. Ne jamais bloquer la sortie ni la rendre inaccessible.
3. Conserver la cohérence de la structure logique.
4. Conserver la position actuelle du joueur.
5. Essayer d’atteindre le nombre de boucles demandé.
6. Optimiser le rendu et les temps de rafraîchissement.

## 20. Hypothèses à confirmer

Ces points reprennent les propositions faites pendant la discussion ; ils sont appliqués tant qu’ils ne sont pas corrigés.

- **\[H1\]** Le retour sur un trajet rouge est possible : segment jaune par-dessus le rouge, annulation qui restitue le rouge. Cela corrige la phrase « le jaune n’arrive jamais sur du rouge », sinon le joueur serait bloqué dans une impasse validée.
- **\[H2\]** Règle d’arête physique : entrée et sortie ne partagent aucune arête, cellules de coin comprises (deux arêtes chacune), et non seulement des numéros de coin différents.
- **\[H3\]** Convention de `dir` identique pour l’entrée et la sortie (1 = côté départ, 2 = intérieur, 3 = côté arrivée) ; connexion imposée ouverte, valeurs sans voisin sautées.
- **\[H4\]** Validation sans segment provisoire : valide, rafraîchit l’écran, compte pour l’abandon ; attente du relâchement des boutons (+ 30 ms) après chaque action, sans autre anti-rebond.
- **\[H5\]** Échec de régénération : nouvelle tentative à chaque validation suivante ; signal « départ noir et rouge ».
- **\[H6\]** Écran de fin plein écran avec texte et temps (et non le labyrinthe avec bandeau) ; Validation = retour au menu.
- **\[H7\]** Mode 0 : un trait noir relie la cellule de sortie au bord de l’écran, et le cadre y est interrompu. Mode 1 : seul le cadre est interrompu.
