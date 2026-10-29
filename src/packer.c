/*
 * Builds the rr.dat asset pack (see pack.c for the format).
 *
 * Usage: packer <output> <dir>...
 * Run from the repository root. Every .bmp/.wav/.xml/tracker module file directly
 * inside each directory is stored as "<dir>/<file>".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define PACK_NAME_LEN 64
#define MAX_ENTRIES 512

typedef struct {
  char name[PACK_NAME_LEN];
  unsigned char *data;
  unsigned int size;
} Entry;

static Entry entries[MAX_ENTRIES];
static int entryNum = 0;

static int hasAssetExt(const char *name) {
  const char *ext = strrchr(name, '.');
  if ( ext == NULL ) return 0;
  return strcmp(ext, ".bmp") == 0 || strcmp(ext, ".wav") == 0 ||
    strcmp(ext, ".xml") == 0 ||
    // Tracker modules (music).
    strcmp(ext, ".it") == 0 || strcmp(ext, ".xm") == 0 ||
    strcmp(ext, ".s3m") == 0 || strcmp(ext, ".mod") == 0 ||
    strcmp(ext, ".mptm") == 0;
}

static void addFile(const char *dir, const char *file) {
  char path[512];
  FILE *fp;
  Entry *e;
  long size;
  if ( entryNum >= MAX_ENTRIES ) {
    fprintf(stderr, "Too many files.\n");
    exit(1);
  }
  snprintf(path, sizeof(path), "%s/%s", dir, file);
  if ( strlen(path) >= PACK_NAME_LEN ) {
    fprintf(stderr, "Name too long: %s\n", path);
    exit(1);
  }
  if ( NULL == (fp = fopen(path, "rb")) ) {
    fprintf(stderr, "Can't open: %s\n", path);
    exit(1);
  }
  fseek(fp, 0, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  e = &entries[entryNum++];
  memset(e->name, 0, PACK_NAME_LEN);
  strcpy(e->name, path);
  e->size = (unsigned int)size;
  e->data = (unsigned char *)malloc(size > 0 ? size : 1);
  if ( fread(e->data, 1, size, fp) != (size_t)size ) {
    fprintf(stderr, "Can't read: %s\n", path);
    exit(1);
  }
  fclose(fp);
}

static void addDir(const char *dir) {
  DIR *dp;
  struct dirent *de;
  int before = entryNum;
  if ( (dp = opendir(dir)) == NULL ) {
    fprintf(stderr, "Can't open directory: %s\n", dir);
    exit(1);
  }
  while ( (de = readdir(dp)) != NULL ) {
    if ( de->d_name[0] == '.' || !hasAssetExt(de->d_name) ) continue;
    addFile(dir, de->d_name);
  }
  closedir(dp);
  if ( entryNum == before ) {
    fprintf(stderr, "No assets found in: %s\n", dir);
    exit(1);
  }
}

static int compareEntry(const void *a, const void *b) {
  return strcmp(((const Entry *)a)->name, ((const Entry *)b)->name);
}

static void putU32(FILE *fp, unsigned int v) {
  fputc(v & 0xff, fp);
  fputc((v>>8) & 0xff, fp);
  fputc((v>>16) & 0xff, fp);
  fputc((v>>24) & 0xff, fp);
}

int main(int argc, char *argv[]) {
  FILE *fp;
  int i;
  unsigned int offset;
  if ( argc < 3 ) {
    fprintf(stderr, "Usage: %s <output> <dir>...\n", argv[0]);
    return 1;
  }
  for ( i=2 ; i<argc ; i++ ) addDir(argv[i]);
  qsort(entries, entryNum, sizeof(Entry), compareEntry);

  if ( NULL == (fp = fopen(argv[1], "wb")) ) {
    fprintf(stderr, "Can't write: %s\n", argv[1]);
    return 1;
  }
  fwrite("RRPK", 1, 4, fp);
  putU32(fp, entryNum);
  offset = 8 + entryNum*(PACK_NAME_LEN+8);
  for ( i=0 ; i<entryNum ; i++ ) {
    fwrite(entries[i].name, 1, PACK_NAME_LEN, fp);
    putU32(fp, offset);
    putU32(fp, entries[i].size);
    offset += entries[i].size;
  }
  for ( i=0 ; i<entryNum ; i++ ) fwrite(entries[i].data, 1, entries[i].size, fp);
  fclose(fp);
  printf("%s: %d files\n", argv[1], entryNum);
  return 0;
}
