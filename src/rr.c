/*
 * $Id: rr.c,v 1.4 2003/04/26 03:24:16 kenta Exp $
 *
 * Copyright 2003 Kenta Cho. All rights reserved.
 */

/**
 * rRootage main routine.
 *
 * @version $Revision: 1.4 $
 */
#include "SDL.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "rr.h"
#include "screen.h"
#include "vector.h"
#include "foe_mtd.h"
#include "brgmng_mtd.h"
#include "degutil.h"
#include "boss_mtd.h"
#include "ship.h"
#include "laser.h"
#include "frag.h"
#include "shot.h"
#include "background.h"
#include "soundmanager.h"
#include "attractmanager.h"
#include "pack.h"
#include "postfx.h"

static int noSound = 0;

// Initialize and load preference.
static void initFirst() {
  time_t timer;
  time(&timer);
  srand(timer);

  loadPreference();
  initBarragemanager();
  initAttractManager();
  if ( !noSound ) initSound();
  initGameStateFirst();
}

// Quit and save preference.
void quitLast() {
  if ( !noSound ) closeSound();
  savePreference();
  closeFoes();
  closeBarragemanager();
  closeSDL();
  SDL_Quit();
  exit(1);
}

int status;

void initTitleStage(int stg) {
  initFoes();
  initStageState(stg);
}

void initTitle() {
  int stg;
  status = TITLE;

  stg = initTitleAtr();
  initBoss();
  initShip();
  initLasers();
  initFrags();
  initShots();
  initBackground(0);
  initTitleStage(stg);
  left = -1;
}

void initGame(int stg) {
  int sn;
  status = IN_GAME;

  initBoss();
  initFoes();
  initShip();
  initLasers();
  initFrags();
  initShots();

  initGameState(stg);
  sn = stg%SAME_RANK_STAGE_NUM;
  initBackground(sn);
  if ( sn == SAME_RANK_STAGE_NUM-1 ) {
    playMusic(rand()%(SAME_RANK_STAGE_NUM-1));
  } else {
    playMusic(sn);
  }
}

void initGameover() {
  status = GAMEOVER;
  initGameoverAtr();
}

static void move() {
  switch ( status ) {
  case TITLE:
    moveTitleMenu();
    moveBoss();
    moveFoes();
    moveBackground();
    break;
  case IN_GAME:
  case STAGE_CLEAR:
    moveShip();
    moveBoss();
    moveLasers();
    moveShots();
    moveFoes();
    moveFrags();
    moveBackground();
    break;
  case GAMEOVER:
    moveGameover();
    moveBoss();
    moveFoes();
    moveFrags();
    moveBackground();
    break;
  case PAUSE:
    movePause();
    break;
  }
  moveScreenShake();
}

static void draw() {
  switch ( status ) {
  case TITLE:
    drawBackground();
    drawBoss();
    drawBulletsWake();
    drawBullets();
    startDrawBoards();
    drawSideBoards();
    drawTitle();
    endDrawBoards();
    break;
  case IN_GAME:
  case STAGE_CLEAR:
    drawBackground();
    drawBoss();
    drawLasers();
    drawShots();
    drawBulletsWake();
    drawFrags();
    drawShip();
    drawBullets();
    startDrawBoards();
    drawSideBoards();
    drawBossState();
    endDrawBoards();
    break;
  case GAMEOVER:
    drawBackground();
    drawBoss();
    drawBulletsWake();
    drawFrags();
    drawBullets();
    startDrawBoards();
    drawSideBoards();
    drawGameover();
    endDrawBoards();
    break;
  case PAUSE:
    drawBackground();
    drawBoss();
    drawLasers();
    drawShots();
    drawBulletsWake();
    drawFrags();
    drawShip();
    drawBullets();
    startDrawBoards();
    drawSideBoards();
    drawBossState();
    drawPause();
    endDrawBoards();
    break;
  }
}

static int accframe = 0;

static void usage(char *argv0) {
  fprintf(stderr, "Usage: %s [-lowres] [-nosound] [-fullscreen] [-reverse] [-nowait] [-accframe]\n", argv0);
}

static void parseArgs(int argc, char *argv[]) {
  int i;
  for ( i=1 ; i<argc ; i++ ) {
    if ( strcmp(argv[i], "-lowres") == 0 ) {
      lowres = 1;
    } else if ( strcmp(argv[i], "-nosound") == 0 ) {
      noSound = 1;
    } else if ( strcmp(argv[i], "-fullscreen") == 0 ) {
      windowMode = 0;
    } else if ( strcmp(argv[i], "-reverse") == 0 ) {
      buttonReversed = 1;
    }
    /* else if ( (strcmp(argv[i], "-brightness") == 0) && argv[i+1] ) {
      i++;
      brightness = (int)atoi(argv[i]);
      if ( brightness < 0 || brightness > 256 ) {
	brightness = DEFAULT_BRIGHTNESS;
      }
      }*/ 
    else if ( strcmp(argv[i], "-nowait") == 0 ) {
      nowait = 1;
    } else if ( strcmp(argv[i], "-accframe") == 0 ) {
      accframe = 1;
    } else {
      usage(argv[0]);
      exit(1);
    }
  }
}

// Ctrl+Q quits everywhere; Cmd+Q also quits on macOS.
#ifdef __APPLE__
#define QUIT_MOD (KMOD_CTRL | KMOD_GUI)
#else
#define QUIT_MOD KMOD_CTRL
#endif

int interval = INTERVAL_BASE;
int tick = 0;
static int pPrsd = 1;

int main(int argc, char *argv[]) {
  int done = 0;
  long prvTickCount = 0;
  int i;
  SDL_Event event;
  int numKeys;
  long nowTick;
  int frame;
  int buttons;

  windowMode = 1;
  parseArgs(argc, argv);

  initDegutil();
  if ( !packOpen() ) exit(1);
  initSDL();
  initFirst();
  initTitle();

  while ( !done ) {
    while ( SDL_PollEvent(&event) ) {
      if ( event.type == SDL_QUIT ) done = 1;
      if ( event.type == SDL_WINDOWEVENT &&
           event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ) {
        int w, h;
        SDL_GL_GetDrawableSize(window, &w, &h);
        resized(w, h);
      }
      // Handle hotkeys
      if ( event.type == SDL_KEYDOWN ) {
        int modKey;
#ifdef __APPLE__
        modKey = KMOD_GUI;
#else
        modKey = KMOD_CTRL;
#endif
        // Cmd/Ctrl+1 to restore the initial window size
        if ( (event.key.keysym.mod & modKey) && event.key.keysym.scancode == SDL_SCANCODE_1 ) {
          resetWindowSize();
        }
        // Cmd/Ctrl+2 to double window size
        if ( (event.key.keysym.mod & modKey) && event.key.keysym.scancode == SDL_SCANCODE_2 ) {
          doubleWindowSize();
        }
        // Cmd/Ctrl+B to toggle the bloom/trail effects
        if ( (event.key.keysym.mod & modKey) && event.key.keysym.scancode == SDL_SCANCODE_B ) {
          postfxToggle();
        }
        // Cmd/Ctrl+F for fullscreen
        if ( (event.key.keysym.mod & modKey) && event.key.keysym.scancode == SDL_SCANCODE_F ) {
          toggleFullscreen();
        }
      }
    }
    keys = SDL_GetKeyboardState(&numKeys);
    buttons = getButtonState();
    if ( keys[SDL_SCANCODE_ESCAPE] == SDL_PRESSED ) done = 1;
    if ( keys[SDL_SCANCODE_Q] == SDL_PRESSED && (SDL_GetModState() & QUIT_MOD) ) done = 1;
    if ( buttons & PAD_BUTTONP ) {
      if ( !pPrsd ) {
	if ( status == IN_GAME ) {
	  status = PAUSE;
	} else if ( status == PAUSE ) {
	  status = IN_GAME;
	}
      }
      pPrsd = 1;
    } else {
      pPrsd = 0;
    }

    nowTick = SDL_GetTicks();
    frame = (int)(nowTick-prvTickCount) / interval;
    if ( frame <= 0 ) {
      frame = 1;
      SDL_Delay(prvTickCount+interval-nowTick);
      if ( accframe ) {
	prvTickCount = SDL_GetTicks();
      } else {
	prvTickCount += interval;
      }
    } else if ( frame > 5 ) {
      frame = 5;
      prvTickCount = nowTick;
    } else {
      prvTickCount += frame*interval;
    }
    for ( i=0 ; i<frame ; i++ ) {
      move();
      tick++;
    }

    drawGLSceneStart();
    draw();
    drawGLSceneEnd();
    swapGLScene();
  }
  quitLast();
  return 0;
}
