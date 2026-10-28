/*
 * Post-processing effects: bloom and fading trails.
 *
 * The scene is rendered into an offscreen framebuffer and then composited to
 * the window through GLSL shaders. If shaders or framebuffer objects aren't
 * available the game is rendered directly, as before.
 */
#ifndef POSTFX_H_
#define POSTFX_H_

// Load GL functions and create shaders. Needs a current GL context.
// Returns 1 if the effects are available.
int postfxInit(void);
void postfxClose(void);
// Window size and the (letterboxed) game viewport inside it, in pixels.
void postfxResize(int windowW, int windowH, int viewX, int viewY, int viewW, int viewH);
// Redirect rendering into the offscreen buffer. Call before clearing the scene.
void postfxBegin(void);
// Apply the effects and draw the result to the window. Call before swapping.
void postfxEnd(void);
// Turn the effects on/off.
void postfxToggle(void);

#endif
