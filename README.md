# rRootage

for macOS, Linux, Windows98 or later (OpenGL required)

ver. 0.24b

Defeat autocreated huge battleships.
Shootem up game, 'rRootage'.

![screenshot](http://www.asahi-net.or.jp/~cs8k-cyu/windows/rr_1.gif)

### How to play.

- Movement Arrow key, [W][A][S][D] / Joystick
- Laser [Z], [Space] / Trigger 1, Trigger 4
- Special [X] / Trigger 2, Trigger 3
- Pause [P] / Trigger 5,6,7,8,9
- Quit [Esc] / Ctrl/CMD + q
- [F1] / Show or hide the key help screen
- Ctrl/CMD + 1 / Reset the window size
- Ctrl/CMD + 2 / Double the window size
- Ctrl/CMD + f / Go full screen

Select the stage by a keyboard or a joystick.
Press a laser key or [Enter] to start the game.

You can also select the game mode from 4 types.
Press a special key or [Tab] to change the game mode.

#### Normal mode

This is the standard game mode.
Your ship becomes slow while holding the laser key.

- Special -> Bomb

The bomb wipes enemyies bullets.
The number of bombs are displayed at the right-down corner.

#### PSY mode

As your ship grazes a bullet,
the graze meter(displayed at the right-down corner) increases.
When the graze meter becomes full, the ship becomes invincible for a while.

- Special -> Rolling

This movement widen the range that the ship can graze.
While holding this key, the ship becomes slow.
If you want to move faster, tap this key.

#### IKA mode

Your ship has two attributes, white and black.
All bullets also have these attributes,
and your ship can absorb bullets that has the same ttribute.
Absorbed bullets are changed into lasers automatically.

- Special -> Attribution change

Change your ships attribute.

#### GW mode

Your ship can use the reflector.
The reflector reflects bullets around your ship.

- Special -> Reflector

To use the reflector,
you have to hold this key until
the reflector meter(displayed at the right-down corner) becomes empty.
You can use the reflector only if the reflector meter displays 'OK'.

Control your ship and avoid the barrage.
Use the laser to destroy the battleship of the enemy.
You can cause more damage if you fire the laser close to the enemy.

When all ships are destroyed, the game is over.
The ship extends 200,000 and every 500,000 points.

These options are available:

- -fullscreen Launch the game in the full-screen.
- -nosound Stop the sound.
- -reverse Reverse the laser key and the bomb key.
- -lowres Use the low resolution mode.

### How to build

`make` builds both `bin/rr` and the asset pack `bin/rr.dat`. Keep the two files in the same directory (on Windows, also put the SDL2 DLLs there). The game can then be started from any directory.

#### Windows

1. Install [MSYS2](http://www.msys2.org/).

1. Open mingw32.exe.

1. Install gcc with pacman.

   ```
   $ pacman -S base-devel mingw-w64-i686-toolchain
   ```

1. Install SDL2, SDL2_mixer and libopenmpt for MinGW (e.g. `pacman -S mingw-w64-i686-SDL2 mingw-w64-i686-SDL2_mixer mingw-w64-i686-libopenmpt`).

1. Clone or download this repository.

1. Enter the src directory.

1. Make.

   ```
   $ make
   ```

#### macOS

1. Install the Xcode command line tools and the dependencies: `brew install sdl2 sdl2_mixer libopenmpt pkgconf`.

1. Build [libBulletML 0.0.6](http://shinh.skr.jp/libbulletml/index_en.html) for macOS and save the archive as `src/libbulletml_macos.a` (the bundled `libbulletml.a` is a Windows archive).

1. In the src directory, run `make -f Makefile.macos`, then run `./bin/rr`.

#### Linux

1. Install the dependencies (Debian/Ubuntu shown): `sudo apt install build-essential curl bzip2 pkg-config libsdl2-dev libsdl2-mixer-dev libopenmpt-dev libglu1-mesa-dev`.

1. In the src directory, run `make -f Makefile.linux`. The first run downloads and builds libBulletML 0.0.6.

1. Run `./bin/rr`.

If you want to build rRootage in Visual Studio, you can use [the cmake branch](https://github.com/abagames/rrootage/tree/cmake). Details are given in [PR #8](https://github.com/abagames/rrootage/pull/8).

### Music

The BGM is played from tracker modules by libopenmpt. Put the modules in `sounds/` as
`stg_a`, `stg_b` and `stg_c` with one of the extensions `.it`, `.xm`, `.s3m`, `.mod` or `.mptm`
(the formats are detected from the file contents), then run `make` to rebuild `bin/rr.dat`.
Modules loop forever. A missing module just means that stage has no music.

### Webpage

[rRootage webpage](http://www.asahi-net.or.jp/~cs8k-cyu/windows/rr_e.html)

### Acknowledgement

[libBulletML](http://shinh.skr.jp/libbulletml/index_en.html) is used to parse BulletML files.

[Simple DirectMedia Layer](http://www.libsdl.org/) is used for the display handling.

[SDL_mixer](http://www.libsdl.org/projects/SDL_mixer/) to play the sound effects.

[libopenmpt](https://lib.openmpt.org/libopenmpt/) to play the BGM (tracker modules).

[Mersenne Twister](http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html) to create the random number.

### History

2026 10/26 ver. 024b

- Update to SDL2
- Add macOS and Linux builds.
- Add WASD movement and Space to shoot.
- Pack the assets into `bin/rr.dat`; the game can now be started from any directory.
- Save the high scores to the per-user preference directory.
- Add Ctrl/CMD + q to quit.
- Add Ctrl/CMD + 1 to reset the window size.
- Add Enter to start a stage and Tab to change the game mode on the title screen.

2018 1/15 ver. 0.24

- Update SDL.

2003 8/15 ver. 0.23

- Adjust barrages.
- Update libBulletML.
- Fix memory handling bugs.

2003 4/26 ver. 0.22

- Adjust barrages.

2003 4/ 4 ver. 0.21

- Add new barrages.
- Adjust barrages.

2003 3/21 ver. 0.2

- Add new game modes.
- Timer stops while the battleship changing the shape.
- Adjust bullets.

2003 3/16 ver. 0.1
