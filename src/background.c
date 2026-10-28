/*
 * $Id: background.c,v 1.1.1.1 2003/03/16 07:03:49 kenta Exp $
 *
 * Copyright 2003 Kenta Cho. All rights reserved.
 */

/**
 * Screen background.
 *
 * @version $Revision: 1.1.1.1 $
 */
#include "genmcr.h"
#include "background.h"
#include "screen.h"

#define PLANE_MAX 4

static Plane plane[PLANE_MAX];
static int planeNum;

static void fixScrollSpeed();

void initBackground(int s) {
  int i;
  Plane *pl;
  switch ( s ) {
  case 0:
    planeNum = 3;
    for ( i=0 ; i<planeNum ; i++ ) {
      pl = &(plane[i]);
      pl->xn = 8; pl->yn = 16;
      pl->width = 3.0f; pl->height = 2.0f;
      pl->x = -pl->width*pl->xn/2; 
      pl->y = -pl->height*pl->yn/2;
      pl->z = -10.0f;
      pl->ox = pl->oy = 0;
      switch ( i ) {
      case 0:
	pl->r = 250; pl->g = 200; pl->b = 20; pl->a = 100;
	pl->d1 = 0;
	pl->mx = 0; pl->my = 0.05;
	break;
      case 1:
	pl->r = 200; pl->g = 150; pl->b = 20; pl->a = 50;
	pl->d1 = 32;
	pl->mx = 0.04; pl->my = 0.05;
	break;
      case 2:
	pl->r = 200; pl->g = 150; pl->b = 20; pl->a = 50;
	pl->d1 = -32&1023;
	pl->mx = -0.04; pl->my = 0.05;
	break;
      }
    }
    break;
  case 1:
    planeNum = 2;
    for ( i=0 ; i<planeNum ; i++ ) {
      pl = &(plane[i]);
      pl->xn = 8; pl->yn = 8;
      pl->width = 3.0f; pl->height = 3.0f;
      pl->x = -pl->width*pl->xn/2; 
      pl->y = -pl->height*pl->yn/2;
      pl->z = -10.0f;
      pl->ox = pl->oy = 0;
      switch ( i ) {
      case 0:
	pl->r = 200; pl->g = 100; pl->b = 200; pl->a = 150;
	pl->d1 = 4;
	pl->mx = 0; pl->my = 0.12;
	break;
      case 1:
	pl->r = 120; pl->g = 100; pl->b = 120; pl->a = 120;
	pl->d1 = 4;
	pl->mx = 0; pl->my = -0.05;
	break;
      }
    }
    break;
  case 2:
    planeNum = 3;
    for ( i=0 ; i<planeNum ; i++ ) {
      pl = &(plane[i]);
      switch ( i ) {
      case 0:
	pl->xn = 6; pl->yn = 6;
	pl->width = 5.0f; pl->height = 5.0f;
	break;
      case 1:
      case 2:
	pl->xn = 16; pl->yn = 16;
	pl->width = 1.5f; pl->height = 1.5f;
	break;
      }
      pl->x = -pl->width*pl->xn/2; 
      pl->y = -pl->height*pl->yn/2;
      pl->z = -10.0f;
      pl->ox = pl->oy = 0;
      switch ( i ) {
      case 0:
	pl->r = 150; pl->g = 200; pl->b = 150; pl->a = 125;
	pl->d1 = 0;
	pl->mx = 0; pl->my = 0.04;
	break;
      case 1:
	pl->r = 170; pl->g = 200; pl->b = 170; pl->a = 60;
	pl->d1 = 0;
	pl->mx = 0.01; pl->my = 0.01;
	break;
      case 2:
	pl->r = 170; pl->g = 200; pl->b = 170; pl->a = 60;
	pl->d1 = 0;
	pl->mx = -0.01; pl->my = 0.01;
	break;
      }
    }
    break;
  case 3:
    planeNum = 4;
    for ( i=0 ; i<planeNum ; i++ ) {
      pl = &(plane[i]);
      pl->xn = 8; pl->yn = 16;
      pl->width = 4.0f; pl->height = 2.5f;
      pl->x = -pl->width*pl->xn/2; 
      pl->y = -pl->height*pl->yn/2;
      pl->z = -10.0f;
      pl->ox = pl->oy = 0;
      pl->r = 200; pl->g = 200; pl->b = 100; 
      switch ( i ) {
      case 0:
	pl->a = 72;
	pl->d1 = 0;
	pl->mx = -0.05; pl->my = 0.1;
	break;
      case 1:
	pl->a = 40;
	pl->d1 = 10;
	pl->mx = -0.025; pl->my = 0;
	break;
      case 2:
	pl->a = 40;
	pl->d1 = -10;
	pl->mx = 0.025; pl->my = 0;
	break;
      case 3:
	pl->a = 72;
	pl->d1 = 0;
	pl->mx = -0.025; pl->my = 0.1;
	break;
      }
    }
    break;
  }
  fixScrollSpeed();
}

// Make sure every layer scrolls towards the player.
#define MIN_SCROLL_SPEED 0.04f

static void fixScrollSpeed() {
  int i;
  for ( i=0 ; i<planeNum ; i++ ) {
    if ( plane[i].my < MIN_SCROLL_SPEED ) plane[i].my = MIN_SCROLL_SPEED;
  }
}

void moveBackground() {
  int i;
  Plane *pl;
  for ( i=0 ; i<planeNum ; i++ ) {
    pl = &(plane[i]);
    pl->ox -= pl->mx;
    if ( pl->ox < 0 )          pl->ox += pl->width;
    if ( pl->ox >= pl->width ) pl->ox -= pl->width;
    pl->oy -= pl->my;
    if ( pl->oy < 0 )           pl->oy += pl->height;
    if ( pl->oy >= pl->height ) pl->oy -= pl->height;
  }
}

// The grid is drawn over the whole visible ground, up to where it fades out in the distance.
#define GROUND_X 28.0f
#define GROUND_Y_NEAR -12.0f
#define GROUND_Y_FAR 30.0f

void drawBackground() {
  int k, kmin, kmax;
  float x, y, z;
  Plane *pl;
  int i;
  for ( i=0 ; i<planeNum ; i++ ) {
    pl = &(plane[i]);
    z = getGroundZ(pl->z);
    kmax = (int)(GROUND_X/pl->width) + 1;
    for ( k=-kmax ; k<=kmax ; k++ ) {
      x = pl->ox + k*pl->width;
      drawGroundLine(x, GROUND_Y_NEAR, x, GROUND_Y_FAR, z, pl->r, pl->g, pl->b, pl->a, pl->d1);
    }
    kmin = (int)(GROUND_Y_NEAR/pl->height) - 1;
    kmax = (int)(GROUND_Y_FAR/pl->height) + 1;
    for ( k=kmin ; k<=kmax ; k++ ) {
      y = pl->oy + k*pl->height;
      drawGroundLine(-GROUND_X, y, GROUND_X, y, z, pl->r, pl->g, pl->b, pl->a, pl->d1);
    }
  }
}
