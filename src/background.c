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
#include <math.h>
#include <stdlib.h>
#include "genmcr.h"
#include "background.h"
#include "screen.h"

#define PLANE_MAX 4

static void initPath();

static Plane plane[PLANE_MAX];
static int planeNum;

void initBackground(int s) {
  int i;
  Plane *pl;
  initPath();
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
}

// The background is a wireframe tunnel. Each Plane describes one layer of the
// tunnel: xn lines run along the tunnel around its circumference and rings
// of spacing `height` scroll toward the camera. The layers' mx/my/d1 values
// spin, scroll and roll the tunnel.
#define TUNNEL_RADIUS 9.0f
#define TUNNEL_FRONT_Z 8.0f
#define TUNNEL_DEPTH_MULT 2      // Tunnel length relative to yn*height.
#define LINE_STEPS 40            // Segments of each line along the tunnel.
#define RING_SEGMENTS_PER_GAP 4
#define ALPHA_BOOST 0.7f
#define TWO_PI 6.28318531f

// The centre line of the tunnel is a random path sampled every PATH_STEP
// units of depth. It scrolls toward the camera and new points are appended
// at the far end, so the tunnel keeps winding left, right, up and down.
#define PATH_STEP 4.0f
#define PATH_N 48
#define PATH_SPEED 0.08f
#define PATH_LIMIT 7.0f

static float pathX[PATH_N], pathY[PATH_N];
static float pathVX, pathVY;
static float pathOfs;

static void appendPathPoint(float *x, float *y) {
  pathVX = pathVX*0.8f + ((rand()%2001)-1000)/1000.0f*0.9f;
  pathVY = pathVY*0.8f + ((rand()%2001)-1000)/1000.0f*0.9f;
  *x += pathVX - *x*0.08f;
  *y += pathVY - *y*0.08f;
  if ( *x >  PATH_LIMIT ) *x =  PATH_LIMIT;
  if ( *x < -PATH_LIMIT ) *x = -PATH_LIMIT;
  if ( *y >  PATH_LIMIT ) *y =  PATH_LIMIT;
  if ( *y < -PATH_LIMIT ) *y = -PATH_LIMIT;
}

static void initPath() {
  int i;
  float x = 0, y = 0;
  pathVX = pathVY = 0;
  pathOfs = 0;
  for ( i=0 ; i<PATH_N ; i++ ) {
    appendPathPoint(&x, &y);
    pathX[i] = x; pathY[i] = y;
  }
}

static void movePath() {
  int i;
  pathOfs += PATH_SPEED;
  if ( pathOfs >= PATH_STEP ) {
    pathOfs -= PATH_STEP;
    for ( i=0 ; i<PATH_N-1 ; i++ ) {
      pathX[i] = pathX[i+1]; pathY[i] = pathY[i+1];
    }
    pathX[PATH_N-1] = pathX[PATH_N-2];
    pathY[PATH_N-1] = pathY[PATH_N-2];
    appendPathPoint(&pathX[PATH_N-1], &pathY[PATH_N-1]);
  }
}

// Smoothly interpolated centre of the tunnel at the given depth.
static void pathAt(float depth, float *cx, float *cy) {
  float u = (depth + pathOfs) / PATH_STEP;
  int i = (int)u;
  float f;
  if ( i > PATH_N-2 ) { i = PATH_N-2; u = (float)i + 1; }
  f = (1.0f - cosf((u - i) * 3.14159265f)) * 0.5f;
  *cx = pathX[i] + (pathX[i+1]-pathX[i])*f;
  *cy = pathY[i] + (pathY[i+1]-pathY[i])*f;
}

void moveBackground() {
  int i;
  Plane *pl;
  movePath();
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

static int fadeAlpha(int a, float t) {
  int v = (int)(a * ALPHA_BOOST * (1.0f - t*t));
  return v > 255 ? 255 : (v < 0 ? 0 : v);
}

void drawBackground() {
  int i, lx, k, s, n, segs, rings;
  float gap, off, depth, total, ang, cx, cy, bx, by;
  Plane *pl;
  pathAt(0, &bx, &by);
  for ( i=0 ; i<planeNum ; i++ ) {
    pl = &(plane[i]);
    gap = TWO_PI / pl->xn;
    off = pl->ox / pl->width * gap + (float)pl->d1 * TWO_PI / 1024;
    rings = pl->yn * TUNNEL_DEPTH_MULT;
    total = pl->height * rings;
    segs = pl->xn * RING_SEGMENTS_PER_GAP;
    // Lines along the tunnel, fading out with distance.
    for ( lx=0 ; lx<pl->xn ; lx++ ) {
      ang = off + lx*gap;
      glBegin(GL_LINE_STRIP);
      for ( n=0 ; n<=LINE_STEPS ; n++ ) {
	depth = total * n / LINE_STEPS;
	pathAt(depth, &cx, &cy);
	glColor4ub(pl->r, pl->g, pl->b, fadeAlpha(pl->a, (float)n/LINE_STEPS));
	glVertex3f(TUNNEL_RADIUS*cosf(ang) + cx - bx,
		   TUNNEL_RADIUS*sinf(ang) + cy - by,
		   TUNNEL_FRONT_Z - depth);
      }
      glEnd();
    }
    // Rings around the tunnel.
    for ( k=0 ; k<rings ; k++ ) {
      depth = k*pl->height + pl->oy;
      pathAt(depth, &cx, &cy);
      glColor4ub(pl->r, pl->g, pl->b, fadeAlpha(pl->a, depth/total));
      glBegin(GL_LINE_LOOP);
      for ( s=0 ; s<segs ; s++ ) {
	ang = off + TWO_PI*s/segs;
	glVertex3f(TUNNEL_RADIUS*cosf(ang) + cx - bx,
		   TUNNEL_RADIUS*sinf(ang) + cy - by,
		   TUNNEL_FRONT_Z - depth);
      }
      glEnd();
    }
  }
}
