/*
 * Post-processing effects: bloom and fading trails.
 *
 * Per frame:
 *   1. The scene is drawn into sceneTarget.
 *   2. Trail pass:  trail[new] = max(scene, trail[old]*decay)
 *   3. Bloom: bright pass -> blur (1/2 and 1/4 resolution)
 *   4. Composite: trail + bloom to the window.
 */
#include <stdio.h>
#include <string.h>

#include "SDL.h"
#include "screen.h"
#include "postfx.h"

// ---- Tunables ----
#define TRAIL_DECAY 0.84f       // Brightness kept by the trails each frame.
#define TRAIL_FADE_STEP 0.012f  // Subtracted each frame so 8-bit trails fully fade out.
#define BLOOM_THRESHOLD 0.30f   // Brightness above which a pixel glows.
#define BLOOM_INTENSITY 0.425f

// ---- GL 2.0 / FBO entry points (not exported by opengl32 on Windows) ----
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif

typedef char PfxChar;

#define PFX_FUNCS \
  F(GLuint, glCreateShader, (GLenum type)) \
  F(void, glShaderSource, (GLuint s, GLsizei n, const PfxChar **src, const GLint *len)) \
  F(void, glCompileShader, (GLuint s)) \
  F(void, glGetShaderiv, (GLuint s, GLenum p, GLint *v)) \
  F(void, glGetShaderInfoLog, (GLuint s, GLsizei max, GLsizei *len, PfxChar *log)) \
  F(void, glDeleteShader, (GLuint s)) \
  F(GLuint, glCreateProgram, (void)) \
  F(void, glAttachShader, (GLuint p, GLuint s)) \
  F(void, glLinkProgram, (GLuint p)) \
  F(void, glGetProgramiv, (GLuint p, GLenum n, GLint *v)) \
  F(void, glGetProgramInfoLog, (GLuint p, GLsizei max, GLsizei *len, PfxChar *log)) \
  F(void, glDeleteProgram, (GLuint p)) \
  F(void, glUseProgram, (GLuint p)) \
  F(GLint, glGetUniformLocation, (GLuint p, const PfxChar *name)) \
  F(void, glUniform1i, (GLint l, GLint v)) \
  F(void, glUniform1f, (GLint l, GLfloat v)) \
  F(void, glUniform2f, (GLint l, GLfloat a, GLfloat b)) \
  F(void, glActiveTexture, (GLenum t))

#define PFX_FBO_FUNCS \
  F(void, glGenFramebuffers, (GLsizei n, GLuint *ids)) \
  F(void, glBindFramebuffer, (GLenum target, GLuint fb)) \
  F(void, glFramebufferTexture2D, (GLenum target, GLenum att, GLenum tt, GLuint tex, GLint level)) \
  F(GLenum, glCheckFramebufferStatus, (GLenum target)) \
  F(void, glDeleteFramebuffers, (GLsizei n, const GLuint *ids))

#define F(ret, name, args) static ret (*pfx_##name) args;
PFX_FUNCS
PFX_FBO_FUNCS
#undef F

static int loadFunctions(void) {
  int ok = 1;
#define F(ret, name, args) \
  pfx_##name = (ret (*) args)SDL_GL_GetProcAddress(#name); \
  if ( pfx_##name == NULL ) ok = 0;
  PFX_FUNCS
#undef F
  // Framebuffer functions: core names, then the EXT versions.
#define F(ret, name, args) \
  pfx_##name = (ret (*) args)SDL_GL_GetProcAddress(#name); \
  if ( pfx_##name == NULL ) pfx_##name = (ret (*) args)SDL_GL_GetProcAddress(#name "EXT"); \
  if ( pfx_##name == NULL ) ok = 0;
  PFX_FBO_FUNCS
#undef F
  return ok;
}

// ---- Shaders ----
static const char *vertexSrc =
  "#version 120\n"
  "void main() {\n"
  "  gl_Position = ftransform();\n"
  "  gl_TexCoord[0] = gl_MultiTexCoord0;\n"
  "}\n";

// trail = max(scene, previous*decay)
static const char *trailSrc =
  "#version 120\n"
  "uniform sampler2D scene;\n"
  "uniform sampler2D prev;\n"
  "uniform float decay;\n"
  "uniform float fadeStep;\n"
  "void main() {\n"
  "  vec3 s = texture2D(scene, gl_TexCoord[0].st).rgb;\n"
  "  vec3 p = texture2D(prev, gl_TexCoord[0].st).rgb;\n"
  "  gl_FragColor = vec4(max(s, p*decay - fadeStep), 1.0);\n"
  "}\n";

// Keep only the bright part of the image (threshold 0 = plain copy).
static const char *brightSrc =
  "#version 120\n"
  "uniform sampler2D tex;\n"
  "uniform float threshold;\n"
  "void main() {\n"
  "  vec3 c = texture2D(tex, gl_TexCoord[0].st).rgb;\n"
  "  gl_FragColor = vec4(max(c - vec3(threshold), 0.0) / (1.0 - threshold), 1.0);\n"
  "}\n";

// Separable gaussian blur; dir is one texel step along the blur axis.
static const char *blurSrc =
  "#version 120\n"
  "uniform sampler2D tex;\n"
  "uniform vec2 dir;\n"
  "void main() {\n"
  "  vec2 uv = gl_TexCoord[0].st;\n"
  "  vec3 c = texture2D(tex, uv).rgb * 0.227027;\n"
  "  c += (texture2D(tex, uv + dir*1.0).rgb + texture2D(tex, uv - dir*1.0).rgb) * 0.1945946;\n"
  "  c += (texture2D(tex, uv + dir*2.0).rgb + texture2D(tex, uv - dir*2.0).rgb) * 0.1216216;\n"
  "  c += (texture2D(tex, uv + dir*3.0).rgb + texture2D(tex, uv - dir*3.0).rgb) * 0.0540540;\n"
  "  c += (texture2D(tex, uv + dir*4.0).rgb + texture2D(tex, uv - dir*4.0).rgb) * 0.0162162;\n"
  "  gl_FragColor = vec4(c, 1.0);\n"
  "}\n";

static const char *compositeSrc =
  "#version 120\n"
  "uniform sampler2D base;\n"
  "uniform sampler2D bloom1;\n"
  "uniform sampler2D bloom2;\n"
  "uniform float intensity;\n"
  "void main() {\n"
  "  vec2 uv = gl_TexCoord[0].st;\n"
  "  vec3 c = texture2D(base, uv).rgb;\n"
  "  c += (texture2D(bloom1, uv).rgb * 0.8 + texture2D(bloom2, uv).rgb) * intensity;\n"
  "  gl_FragColor = vec4(c, 1.0);\n"
  "}\n";

static GLuint compileShader(GLenum type, const char *src) {
  GLuint s = pfx_glCreateShader(type);
  GLint ok = 0;
  pfx_glShaderSource(s, 1, &src, NULL);
  pfx_glCompileShader(s);
  pfx_glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if ( !ok ) {
    char log[1024];
    pfx_glGetShaderInfoLog(s, sizeof(log), NULL, log);
    fprintf(stderr, "Shader compile failed: %s\n", log);
    pfx_glDeleteShader(s);
    return 0;
  }
  return s;
}

static GLuint buildProgram(const char *fragSrc) {
  GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
  GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
  GLuint p;
  GLint ok = 0;
  if ( !vs || !fs ) return 0;
  p = pfx_glCreateProgram();
  pfx_glAttachShader(p, vs);
  pfx_glAttachShader(p, fs);
  pfx_glLinkProgram(p);
  pfx_glDeleteShader(vs);
  pfx_glDeleteShader(fs);
  pfx_glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if ( !ok ) {
    char log[1024];
    pfx_glGetProgramInfoLog(p, sizeof(log), NULL, log);
    fprintf(stderr, "Shader link failed: %s\n", log);
    pfx_glDeleteProgram(p);
    return 0;
  }
  return p;
}

// ---- Render targets ----
typedef struct {
  GLuint tex, fbo;
  int w, h;
} Target;

static int createTarget(Target *t, int w, int h) {
  if ( w < 1 ) w = 1;
  if ( h < 1 ) h = 1;
  t->w = w; t->h = h;
  glGenTextures(1, &t->tex);
  glBindTexture(GL_TEXTURE_2D, t->tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  pfx_glGenFramebuffers(1, &t->fbo);
  pfx_glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
  pfx_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t->tex, 0);
  if ( pfx_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE ) return 0;
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  return 1;
}

static void destroyTarget(Target *t) {
  if ( t->fbo ) pfx_glDeleteFramebuffers(1, &t->fbo);
  if ( t->tex ) glDeleteTextures(1, &t->tex);
  t->fbo = 0; t->tex = 0;
}

enum { SCENE, TRAIL0, TRAIL1, BLOOM_A1, BLOOM_B1, BLOOM_A2, BLOOM_B2, TARGET_NUM };
static Target target[TARGET_NUM];

static GLuint progTrail, progBright, progBlur, progComposite;
static int available = 0;   // Shaders and FBOs work.
static int enabled = 1;     // The user hasn't turned the effects off.
static int active = 0;      // postfxBegin() has been called for this frame.
static int targetsReady = 0;
static int trailCur = 0;
static int winW, winH, viewX, viewY, viewW, viewH;

static void destroyTargets(void) {
  int i;
  for ( i=0 ; i<TARGET_NUM ; i++ ) destroyTarget(&target[i]);
  targetsReady = 0;
}

static int createTargets(void) {
  int ok = 1;
  GLfloat clearColor[4];
  glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor); // createTarget() changes it.
  destroyTargets();
  ok &= createTarget(&target[SCENE], viewW, viewH);
  ok &= createTarget(&target[TRAIL0], viewW, viewH);
  ok &= createTarget(&target[TRAIL1], viewW, viewH);
  ok &= createTarget(&target[BLOOM_A1], viewW/2, viewH/2);
  ok &= createTarget(&target[BLOOM_B1], viewW/2, viewH/2);
  ok &= createTarget(&target[BLOOM_A2], viewW/4, viewH/4);
  ok &= createTarget(&target[BLOOM_B2], viewW/4, viewH/4);
  pfx_glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
  trailCur = 0;
  targetsReady = ok;
  return ok;
}

int postfxInit(void) {
  available = 0;
  if ( !loadFunctions() ) {
    fprintf(stderr, "Shaders/framebuffers unavailable: effects disabled.\n");
    return 0;
  }
  progTrail = buildProgram(trailSrc);
  progBright = buildProgram(brightSrc);
  progBlur = buildProgram(blurSrc);
  progComposite = buildProgram(compositeSrc);
  if ( !progTrail || !progBright || !progBlur || !progComposite ) {
    fprintf(stderr, "Unable to build shaders: effects disabled.\n");
    return 0;
  }
  available = 1;
  if ( viewW > 0 && !createTargets() ) {
    fprintf(stderr, "Unable to create framebuffers: effects disabled.\n");
    destroyTargets();
    available = 0;
  }
  return available;
}

void postfxClose(void) {
  if ( !available ) return;
  destroyTargets();
  pfx_glDeleteProgram(progTrail);
  pfx_glDeleteProgram(progBright);
  pfx_glDeleteProgram(progBlur);
  pfx_glDeleteProgram(progComposite);
  available = 0;
}

void postfxResize(int windowW, int windowH, int vx, int vy, int vw, int vh) {
  int sizeChanged = (vw != viewW || vh != viewH);
  winW = windowW; winH = windowH;
  viewX = vx; viewY = vy; viewW = vw; viewH = vh;
  // Keep the buffers (and the trails in them) unless the size changed.
  if ( available && (sizeChanged || !targetsReady) && !createTargets() ) {
    fprintf(stderr, "Unable to create framebuffers: effects disabled.\n");
    destroyTargets();
    available = 0;
  }
}

void postfxToggle(void) {
  enabled = !enabled;
  if ( enabled && available ) createTargets(); // Start with clean trails.
}

// ---- Drawing ----
static void bindTexture(int unit, const Target *t) {
  pfx_glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, t->tex);
}

static void drawQuad(void) {
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0); glVertex2f(0, 0);
  glTexCoord2f(1, 0); glVertex2f(1, 0);
  glTexCoord2f(1, 1); glVertex2f(1, 1);
  glTexCoord2f(0, 1); glVertex2f(0, 1);
  glEnd();
}

// Render a full-screen quad into dst (NULL = the window's current viewport).
static void pass(GLuint prog, const Target *dst) {
  pfx_glUseProgram(prog);
  if ( dst != NULL ) {
    pfx_glBindFramebuffer(GL_FRAMEBUFFER, dst->fbo);
    glViewport(0, 0, dst->w, dst->h);
  }
  drawQuad();
}

static void blur(Target *a, Target *b) {
  pfx_glUseProgram(progBlur);
  pfx_glUniform1i(pfx_glGetUniformLocation(progBlur, "tex"), 0);
  // Horizontal a -> b, then vertical b -> a.
  bindTexture(0, a);
  pfx_glUniform2f(pfx_glGetUniformLocation(progBlur, "dir"), 1.0f/a->w, 0);
  pass(progBlur, b);
  bindTexture(0, b);
  pfx_glUniform2f(pfx_glGetUniformLocation(progBlur, "dir"), 0, 1.0f/a->h);
  pass(progBlur, a);
}

static void bright(const Target *src, Target *dst, float threshold) {
  pfx_glUseProgram(progBright);
  pfx_glUniform1i(pfx_glGetUniformLocation(progBright, "tex"), 0);
  pfx_glUniform1f(pfx_glGetUniformLocation(progBright, "threshold"), threshold);
  bindTexture(0, src);
  pass(progBright, dst);
}

void postfxBegin(void) {
  active = 0;
  if ( !available || !enabled || !targetsReady ) return;
  pfx_glBindFramebuffer(GL_FRAMEBUFFER, target[SCENE].fbo);
  glViewport(0, 0, viewW, viewH);
  active = 1;
}

void postfxEnd(void) {
  Target *prev, *cur;
  if ( !active ) return;
  active = 0;
  prev = &target[TRAIL0 + trailCur];
  cur = &target[TRAIL0 + 1 - trailCur];
  trailCur = 1 - trailCur;

  glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, 1, 0, 1, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glDisable(GL_BLEND);
  glEnable(GL_TEXTURE_2D);

  // Trails: cur = max(scene, prev*decay)
  pfx_glUseProgram(progTrail);
  pfx_glUniform1i(pfx_glGetUniformLocation(progTrail, "scene"), 0);
  pfx_glUniform1i(pfx_glGetUniformLocation(progTrail, "prev"), 1);
  pfx_glUniform1f(pfx_glGetUniformLocation(progTrail, "decay"), TRAIL_DECAY);
  pfx_glUniform1f(pfx_glGetUniformLocation(progTrail, "fadeStep"), TRAIL_FADE_STEP);
  bindTexture(0, &target[SCENE]);
  bindTexture(1, prev);
  pass(progTrail, cur);

  // Bloom from the trail image, so the trails glow too.
  bright(cur, &target[BLOOM_A1], BLOOM_THRESHOLD);
  blur(&target[BLOOM_A1], &target[BLOOM_B1]);
  bright(&target[BLOOM_A1], &target[BLOOM_A2], 0.0f);
  blur(&target[BLOOM_A2], &target[BLOOM_B2]);
  blur(&target[BLOOM_A2], &target[BLOOM_B2]);

  // Composite into the window. Fill the letterbox bars with the game's clear color first.
  pfx_glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, winW, winH);
  glClear(GL_COLOR_BUFFER_BIT);
  glViewport(viewX, viewY, viewW, viewH);
  pfx_glUseProgram(progComposite);
  pfx_glUniform1i(pfx_glGetUniformLocation(progComposite, "base"), 0);
  pfx_glUniform1i(pfx_glGetUniformLocation(progComposite, "bloom1"), 1);
  pfx_glUniform1i(pfx_glGetUniformLocation(progComposite, "bloom2"), 2);
  pfx_glUniform1f(pfx_glGetUniformLocation(progComposite, "intensity"), BLOOM_INTENSITY);
  bindTexture(0, cur);
  bindTexture(1, &target[BLOOM_A1]);
  bindTexture(2, &target[BLOOM_A2]);
  drawQuad();

  // Restore state for the next frame.
  pfx_glUseProgram(0);
  pfx_glActiveTexture(GL_TEXTURE0 + 2);
  glBindTexture(GL_TEXTURE_2D, 0);
  pfx_glActiveTexture(GL_TEXTURE0 + 1);
  glBindTexture(GL_TEXTURE_2D, 0);
  pfx_glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glPopAttrib();
  glViewport(viewX, viewY, viewW, viewH);
}
