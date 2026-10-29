/*
 * $Id: soundmanager.c,v 1.3 2003/04/26 03:24:16 kenta Exp $
 *
 * Copyright 2003 Kenta Cho. All rights reserved.
 */

/**
 * BGM/SE manager.
 * The sound effects are played by SDL_mixer. The music is tracker modules
 * (.it, .xm, .s3m, .mod, .mptm) rendered by libopenmpt and fed into
 * SDL_mixer's output through Mix_HookMusic.
 *
 * @version $Revision: 1.3 $
 */
#include "SDL.h"
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "SDL_mixer.h"
#include <libopenmpt/libopenmpt.h>
#include "pack.h"
#include "soundmanager.h"

static int useAudio = 0;

#define MUSIC_NUM 3

// Music files are looked up as sounds/<name><ext> for each extension in turn.
static const char *musicBaseName[MUSIC_NUM] = {
  "stg_a", "stg_b", "stg_c",
};
static const char *musicExt[] = {
  ".it", ".xm", ".s3m", ".mod", ".mptm",
};
#define MUSIC_EXT_NUM (int)(sizeof(musicExt)/sizeof(musicExt[0]))

static openmpt_module *music[MUSIC_NUM];

// State shared between the game thread and the audio thread (guarded by musicMutex).
static SDL_mutex *musicMutex = NULL;
static openmpt_module *curMusic = NULL;
static int musicRate = 44100;
static int musicChannels = 2;
static int fadeLeft = 0;   // Remaining frames of the fade-out (0: not fading).
static int fadeTotal = 0;

#define CHUNK_NUM 16

static char *chunkFileName[CHUNK_NUM] = {
  "laser_start.wav", "laser.wav", "damage.wav", "bomb.wav", 
  "destroied.wav", "explosion1.wav", "explosion2.wav", "miss.wav", "extend.wav",
  "grz.wav", "grzinv.wav", 
  "shot.wav", "change.wav",
  "reflec1.wav", "reflec2.wav", "ref_ready.wav",
};
static Mix_Chunk *chunk[CHUNK_NUM];
static int chunkChannel[CHUNK_NUM] = {
  0, 1, 2, 3,
  4, 5, 6, 7, 4,
  6, 7,
  6, 7,
  7, 7, 7,
};

void closeSound() {
  int i;
  if ( !useAudio ) return;
  Mix_HookMusic(NULL, NULL);
  for ( i=0 ; i<MUSIC_NUM ; i++ ) {
    if ( music[i] ) {
      openmpt_module_destroy(music[i]);
      music[i] = NULL;
    }
  }
  curMusic = NULL;
  if ( musicMutex ) {
    SDL_DestroyMutex(musicMutex);
    musicMutex = NULL;
  }
  for ( i=0 ; i<CHUNK_NUM ; i++ ) {
    if ( chunk[i] ) {
      Mix_FreeChunk(chunk[i]);
    }
  }
  Mix_CloseAudio();
}


// Render the music. Called by SDL_mixer on the audio thread and must always
// fill the whole buffer.
static void musicHook(void *udata, Uint8 *stream, int len) {
  Sint16 *out = (Sint16 *)stream;
  int frames = len / (int)(sizeof(Sint16) * musicChannels);
  int i, c, got;
  (void)udata;
  memset(stream, 0, len);
  SDL_LockMutex(musicMutex);
  if ( curMusic ) {
    if ( musicChannels == 1 ) {
      got = (int)openmpt_module_read_mono(curMusic, musicRate, frames, out);
    } else {
      got = (int)openmpt_module_read_interleaved_stereo(curMusic, musicRate, frames, out);
    }
    if ( fadeLeft > 0 ) {
      for ( i=0 ; i<got ; i++ ) {
	if ( fadeLeft > 0 ) {
	  float g = (float)fadeLeft / fadeTotal;
	  for ( c=0 ; c<musicChannels ; c++ ) {
	    out[i*musicChannels+c] = (Sint16)(out[i*musicChannels+c] * g);
	  }
	  fadeLeft--;
	} else {
	  for ( c=0 ; c<musicChannels ; c++ ) out[i*musicChannels+c] = 0;
	}
      }
      if ( fadeLeft <= 0 ) curMusic = NULL;
    }
    // The module ended (only happens for modules that cannot loop).
    if ( got < frames ) curMusic = NULL;
  }
  SDL_UnlockMutex(musicMutex);
}


// Initialize the sound.

static void loadMusic() {
  int i, j;
  char name[64];
  for ( i=0 ; i<MUSIC_NUM ; i++ ) {
    for ( j=0 ; j<MUSIC_EXT_NUM ; j++ ) {
      const void *data;
      size_t size;
      int err = 0;
      const char *errMsg = NULL;
      snprintf(name, sizeof(name), "sounds/%s%s", musicBaseName[i], musicExt[j]);
      data = packGet(name, &size);
      if ( data == NULL ) continue;
      music[i] = openmpt_module_create_from_memory2(data, size, NULL, NULL, NULL, NULL,
						    &err, &errMsg, NULL);
      if ( music[i] == NULL ) {
	fprintf(stderr, "Couldn't load %s: %s\n", name, errMsg ? errMsg : "unknown error");
	if ( errMsg ) openmpt_free_string(errMsg);
	continue;
      }
      openmpt_module_set_repeat_count(music[i], -1);
      break;
    }
    if ( music[i] == NULL ) {
      fprintf(stderr, "No music found for sounds/%s (tried", musicBaseName[i]);
      for ( j=0 ; j<MUSIC_EXT_NUM ; j++ ) fprintf(stderr, " %s", musicExt[j]);
      fprintf(stderr, ")\n");
    }
  }
}

static void loadSounds() {
  int i;
  char name[64];

  for ( i=0 ; i<CHUNK_NUM ; i++ ) {
    strcpy(name, "sounds/");
    strcat(name, chunkFileName[i]);
    if ( NULL == (chunk[i] = Mix_LoadWAV_RW(packRW(name), 1)) ) {
      fprintf(stderr, "Couldn't load: %s\n", name);
      useAudio = 0;
      return;
    }
  }
}

void initSound() {
  int audio_rate;
  Uint16 audio_format;
  int audio_channels;
  int audio_buffers;

  if ( SDL_InitSubSystem(SDL_INIT_AUDIO) < 0 ) {
    fprintf(stderr, "Unable to initialize SDL_AUDIO: %s\n", SDL_GetError());
    return;
  }

  audio_rate = 44100;
  audio_format = AUDIO_S16SYS;
  audio_channels = 2;
  audio_buffers = 4096;
  
  if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) < 0) {
    fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
    return;
  } else {
    Mix_QuerySpec(&audio_rate, &audio_format, &audio_channels);
  }

  useAudio = 1;
  loadSounds();

  // The music hook writes 16-bit mono or stereo samples.
  if ( audio_format == AUDIO_S16SYS && (audio_channels == 1 || audio_channels == 2) ) {
    musicRate = audio_rate;
    musicChannels = audio_channels;
    musicMutex = SDL_CreateMutex();
    loadMusic();
    Mix_HookMusic(musicHook, NULL);
  } else {
    fprintf(stderr, "Unsupported audio format, the music is disabled.\n");
  }
}

// Play/Stop the music/chunk.

void playMusic(int idx) {
  if ( !useAudio || !musicMutex || idx < 0 || idx >= MUSIC_NUM || !music[idx] ) return;
  SDL_LockMutex(musicMutex);
  openmpt_module_set_position_seconds(music[idx], 0.0);
  curMusic = music[idx];
  fadeLeft = fadeTotal = 0;
  SDL_UnlockMutex(musicMutex);
}

void fadeMusic() {
  if ( !useAudio || !musicMutex ) return;
  SDL_LockMutex(musicMutex);
  if ( curMusic ) {
    fadeTotal = fadeLeft = musicRate * 1280 / 1000;
  }
  SDL_UnlockMutex(musicMutex);
}

void stopMusic() {
  if ( !useAudio || !musicMutex ) return;
  SDL_LockMutex(musicMutex);
  curMusic = NULL;
  fadeLeft = fadeTotal = 0;
  SDL_UnlockMutex(musicMutex);
}

void playChunk(int idx) {
  if ( !useAudio ) return;
  Mix_PlayChannel(chunkChannel[idx], chunk[idx], 0);
}

void haltChunk(int idx) {
  if ( !useAudio ) return;
  Mix_HaltChannel(chunkChannel[idx]);
}
