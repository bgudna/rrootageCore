/*
 * Asset pack (rr.dat) reader.
 *
 * File format (all integers are little endian):
 *   "RRPK"            magic
 *   uint32 count      number of entries
 *   count * entry     char name[PACK_NAME_LEN], uint32 offset, uint32 size
 *   data              raw file contents; offsets are from the start of the file
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pack.h"

#define PACK_FILE "rr.dat"
#define PACK_NAME_LEN 64
#define PACK_ENTRY_SIZE (PACK_NAME_LEN+8)
#define PACK_HEADER_SIZE 8

static unsigned char *packData = NULL;
static size_t packDataSize = 0;
static unsigned int entryNum = 0;

static unsigned int readU32(const unsigned char *p) {
  return p[0] | (p[1]<<8) | (p[2]<<16) | ((unsigned int)p[3]<<24);
}

static int loadFile(const char *path) {
  FILE *fp;
  long size;
  if ( NULL == (fp = fopen(path, "rb")) ) return 0;
  fseek(fp, 0, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  if ( size < PACK_HEADER_SIZE ) {
    fclose(fp);
    return 0;
  }
  packData = (unsigned char *)malloc(size);
  if ( packData == NULL || fread(packData, 1, size, fp) != (size_t)size ) {
    fclose(fp);
    free(packData);
    packData = NULL;
    return 0;
  }
  fclose(fp);
  packDataSize = size;
  return 1;
}

static const unsigned char *entryAt(unsigned int i) {
  return packData + PACK_HEADER_SIZE + (size_t)i*PACK_ENTRY_SIZE;
}

int packOpen(void) {
  char path[1024];
  char *base = SDL_GetBasePath();
  int ok = 0;
  if ( base != NULL ) {
    snprintf(path, sizeof(path), "%s%s", base, PACK_FILE);
    SDL_free(base);
    ok = loadFile(path);
  }
  if ( !ok ) ok = loadFile(PACK_FILE);
  if ( !ok ) {
    fprintf(stderr, "Unable to load %s. Place it next to the executable.\n", PACK_FILE);
    return 0;
  }
  if ( memcmp(packData, "RRPK", 4) != 0 ) {
    fprintf(stderr, "%s is not a valid asset pack.\n", PACK_FILE);
    packClose();
    return 0;
  }
  entryNum = readU32(packData+4);
  if ( PACK_HEADER_SIZE + (size_t)entryNum*PACK_ENTRY_SIZE > packDataSize ) {
    fprintf(stderr, "%s is corrupted.\n", PACK_FILE);
    packClose();
    return 0;
  }
  return 1;
}

void packClose(void) {
  free(packData);
  packData = NULL;
  packDataSize = 0;
  entryNum = 0;
}

const void *packGet(const char *name, size_t *size) {
  unsigned int i;
  for ( i=0 ; i<entryNum ; i++ ) {
    const unsigned char *e = entryAt(i);
    if ( strncmp((const char *)e, name, PACK_NAME_LEN) == 0 ) {
      unsigned int off = readU32(e+PACK_NAME_LEN);
      unsigned int sz = readU32(e+PACK_NAME_LEN+4);
      if ( (size_t)off + sz > packDataSize ) return NULL;
      if ( size != NULL ) *size = sz;
      return packData + off;
    }
  }
  return NULL;
}

SDL_RWops *packRW(const char *name) {
  size_t size;
  const void *data = packGet(name, &size);
  if ( data == NULL ) return NULL;
  return SDL_RWFromConstMem(data, (int)size);
}

int packList(const char *dir, const char *names[], int max) {
  unsigned int i;
  int n = 0;
  size_t len = strlen(dir);
  for ( i=0 ; i<entryNum && n<max ; i++ ) {
    const char *name = (const char *)entryAt(i);
    if ( strncmp(name, dir, len) == 0 && name[len] == '/' && strchr(name+len+1, '/') == NULL ) {
      names[n++] = name;
    }
  }
  return n;
}
