/*
 * Asset pack (rr.dat) reader.
 */
#ifndef PACK_H_
#define PACK_H_

#include <stddef.h>
#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

// Load rr.dat from the executable's directory (or the current directory).
// Returns 1 on success, 0 on failure.
int packOpen(void);
void packClose(void);
// Get the data of an asset such as "sounds/shot.wav". Returns NULL if not found.
const void *packGet(const char *name, size_t *size);
// Get a read-only RWops for an asset. Returns NULL if not found.
SDL_RWops *packRW(const char *name);
// List the assets directly under a directory (sorted). Returns the number stored in names.
int packList(const char *dir, const char *names[], int max);

#ifdef __cplusplus
}
#endif

#endif
