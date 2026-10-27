/*
 * $Id: barragemanager.cc,v 1.4 2003/04/26 03:24:15 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Handle barrages.
 *
 * @version $Revision: 1.4 $
 */
extern "C" {
#include "SDL.h"
#include "pack.h"
#include "genmcr.h"
#include "brgmng_mtd.h"
}

#include "barragemanager.h"

Barrage barragePattern[BARRAGE_TYPE_NUM][BARRAGE_PATTERN_MAX];
int barragePatternNum[BARRAGE_TYPE_NUM];

static const char *BARRAGE_DIR_NAME[BARRAGE_TYPE_NUM] = {
  "normal", "reversible", "morph", "simple", "morph_heavy", "psy",
};

// Parses a BulletML file stored in the asset pack.
class PackBulletMLParser : public BulletMLParserTinyXML {
public:
  PackBulletMLParser(const std::string& name) : BulletMLParserTinyXML(name), name_(name) {}
  virtual void parse() {
    size_t size;
    const void *data = packGet(name_.c_str(), &size);
    if ( data == NULL ) {
      fprintf(stderr, "Can't find barrage: %s\n", name_.c_str());
      exit(1);
    }
    // TinyXML needs a NUL-terminated string.
    std::string xml((const char *)data, size);
    TiXmlDocument doc;
    doc.Parse(xml.c_str());
    if ( doc.Error() ) {
      fprintf(stderr, "Can't parse barrage: %s\n", name_.c_str());
      exit(1);
    }
    parseImpl(doc);
  }
private:
  std::string name_;
};

static int readBulletMLFiles(const char *dirPath, Barrage brg[]) {
  const char *names[BARRAGE_PATTERN_MAX];
  int n = packList(dirPath, names, BARRAGE_PATTERN_MAX);
  for ( int i=0 ; i<n ; i++ ) {
    brg[i].bulletml = new PackBulletMLParser(names[i]);
    brg[i].bulletml->build();
  }
  return n;
}

void initBarragemanager() {
  for ( int i=0 ; i<BARRAGE_TYPE_NUM ; i++ ) {
    barragePatternNum[i] = readBulletMLFiles(BARRAGE_DIR_NAME[i], barragePattern[i]);
    printf("--------\n");
  }
}

void closeBarragemanager() {
  for ( int i=0 ; i<BARRAGE_TYPE_NUM ; i++ ) {
    for ( int j=0 ; j<barragePatternNum[i] ; j++ ) {
      delete barragePattern[i][j].bulletml;
    }
  }
}
