# TuxDAN

Créateur de cartouches pour le **CPC Dandanator mini**, sous la forme d'un
plugin de [TuxAPE](../TuxAPE). Des jeux (instantanés `.sna`, ROM de 16K, ou
le jeu en cours dans TuxAPE) deviennent une cartouche de 512K, un fichier
`.des`, avec un menu pour choisir le jeu sur le CPC. La cartouche se branche
dans le Dandanator de TuxAPE (onglet **Extensions** des réglages), ou se grave
dans la mémoire flash d'une vraie cartouche.

Il demande TuxAPE 1.1.8 ou plus récent (interface des plugins version 5).

## Ce qu'il fait

La fenêtre s'ouvre par le menu **Plugins ▸ TuxDAN – Dandanator Cartridge
Maker...**, par son bouton du panneau du bas, ou par Ctrl+Maj+F11.

- **Add** : des instantanés (`.sna`, versions 1 à 3, 64K ou 128K) ou des ROM
  de 16K (`.rom`, `.bin`, avec ou sans en-tête AMSDOS). Les fichiers se
  déposent aussi sur la fenêtre ; un `.des` déposé est ouvert.
- **Add Game Played** : le jeu en cours dans TuxAPE, tel qu'il est à cet
  instant. Chargez la cassette ou la disquette dans TuxAPE, attendez l'écran
  titre ou le menu du jeu, puis ajoutez-le.
- La liste : le nom que montrera le menu (31 caractères, à changer sur
  place), le genre, la machine sur laquelle l'instantané a été pris, la place
  prise une fois compressé, et le jeu qui **démarre seul** à l'allumage (une
  touche tenue au démarrage ramène au menu).
- **Up**, **Down**, **Remove** rangent la liste ; **Export** enregistre un jeu
  en instantané.
- La jauge dit la place prise sur les 480K disponibles.
- **Save** enregistre la cartouche (`.des`), **Open** rouvre une cartouche
  faite par TuxDAN.
- **Try in TuxAPE** branche la cartouche dans le Dandanator de TuxAPE et
  démarre la machine dessus. TuxDAN en écrit une copie dans son propre
  dossier : ce que la machine écrirait dans la flash ne touche pas votre
  fichier.

Sur le CPC, le menu se pilote avec les flèches ou le joystick ; Espace,
Entrée, Return ou le bouton de tir lancent le jeu.

## La cartouche

- **Slot 0** : le menu et le chargeur (`z80/cartridge.asm`), le répertoire
  des jeux (en &3000), la police (en &3800).
- **Slot 1** : un en-tête de 512 octets par jeu : l'en-tête de l'instantané,
  la table de ses blocs, ses registres, et les quelques instructions qui
  finissent le chargement, écrites pour chaque jeu avec ses valeurs.
- **Slots 2 à 31** : la mémoire des jeux, par blocs de 16K compressés ; les
  ROM prennent des slots entiers à partir du 31 (le bouton gauche de la
  cartouche démarre sur le slot 31 : une ROM de test y est à sa place).

Le chargeur remet la mémoire sans pile ni RAM à lui : toute la RAM est celle
du jeu. Les zones de la cartouche se lisent à la place de la RAM, mais les
écritures vont à la RAM en dessous ; chaque bloc est décompressé dans un quart
de mémoire qu'aucune zone ne couvre. Les quarts 1 à 3 (et la seconde banque
d'un 6128) viennent d'abord, le code en &0000 ; le quart 0 ensuite, avec la
copie du chargeur assemblée en &A000. Viennent ensuite le son, le PPI, le Gate
Array et le CRTC, puis les registres, dépilés depuis le slot 1. Au `RET`
final, la cartouche se cache et ne prend plus de commande : le jeu repart là
où l'instantané l'avait laissé.

Ce qui ne peut pas aller sur une cartouche : les instantanés de Plus (l'ASIC
n'est pas remis), ceux de plus de 128K, et ceux dont la RAM est dans une
configuration de CP/M. Trois octets sous la pile du jeu sont écrasés, comme
par une interruption.

## Ce qui a été vérifié

`tools/trycart.cpp`, compilé avec le cœur de TuxAPE, fabrique une cartouche,
la démarre dans un CPC émulé avec un Dandanator et compare, au moment où la
cartouche se cache, la mémoire et les registres avec ceux de l'instantané.
Dix jeux de 64K et 128K reviennent à l'identique ; le menu descend de deux
lignes et lance le troisième jeu ; une ROM (Amstrad Diagnostics) démarre
depuis le menu.

    g++ -std=c++20 -O1 -I../TuxAPE/src -Isrc/engine tools/trycart.cpp src/engine/*.cpp \
        ../TuxAPE/build/src/core/libtuxape_core.a -o build/trycart
    build/trycart DOSSIER jeu1.sna jeu2.sna jeu3.sna

## Compiler

Avec le Qt de TuxAPE (dans `../TuxAPE/.deps`) :

    cmake --preset dev
    cmake --build --preset dev
    ctest --preset dev

L'archive à installer est alors dans `dist/` ; signée si la clé de l'auteur
est donnée (`-DTUXAPE_SIGN_KEY=...`).

Le code Z80 s'assemble avec l'assembleur de TuxAPE, quand il change :

    g++ -std=c++20 -O1 -I../TuxAPE/src tools/assemble.cpp ../TuxAPE/src/core/assembler.cpp -o build/assemble
    build/assemble z80/cartridge.asm src/engine/z80code.h

Le plugin lui-même n'a besoin d'aucune partie de TuxAPE.

## Installer

Dans TuxAPE, **Settings ▸ Plugins ▸ Install...**, puis l'archive
`tuxdan-<version>-linux-x86_64.zip`.

## Les langues

L'anglais est la langue des sources ; le français est dans
`translations/tuxdan_fr.ts`. Le plugin parle la langue des fenêtres de TuxAPE.
Le menu de la cartouche est en anglais.

## Tests

- `engine_pack` : la compression des blocs de 16K, et le refus d'un flux abîmé.
- `engine_cartridge` : les instantanés lus et écrits, la cartouche faite et
  relue, ce que lit son chargeur.
- `gui_dan` : la fenêtre pilotée comme par un utilisateur.
- `plugin_window` : le plugin chargé comme TuxAPE le charge, en anglais et en
  français.

## Licence

MIT, voir `LICENSE`. La police du menu est `font8x8_basic` de Daniel Hepper,
dans le domaine public.
