#include "Gfx.h"
#include "Fonts.h"
#include "Utf8.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

int main() {
  EInkDisplay display;
  Gfx gfx(display);
  assert(gfx.begin());

  // 1. ASCII passthrough
  const char* ascii = "Hello Lume";
  assert(gfx.canRender(kFontRegular, ascii));
  assert(gfx.textWidth(kFontRegular, ascii) > 0);

  // 2. Precomposed NFC Italian accents
  const char* nfc = "perché città è";
  assert(gfx.canRender(kFontRegular, nfc));
  int wNfc = gfx.textWidth(kFontRegular, nfc);
  assert(wNfc > 0);

  // 3. Decomposed NFD Italian accents (e + combining acute, a + combining grave, e + combining grave)
  // 'e' + U+0301 (0xCC 0x81) = 'é'
  // 'a' + U+0300 (0xCC 0x80) = 'à'
  // 'e' + U+0300 (0xCC 0x80) = 'è'
  const char* nfd = "perche\xCC\x81 citta\xCC\x80 e\xCC\x80";
  // Directly test utf8ComposeNfc:
  std::string composed = utf8ComposeNfc(nfd);
  assert(composed == nfc);

  // Test that Gfx canRender accepts decomposed NFD and renders it with identical width to NFC!
  assert(gfx.canRender(kFontRegular, nfd));
  int wNfd = gfx.textWidth(kFontRegular, nfd);
  assert(wNfd == wNfc);

  // 4. Null safety
  assert(!gfx.canRender(kFontRegular, nullptr));
  assert(gfx.textWidth(kFontRegular, nullptr) == 0);

  printf("accent_test: all assertions passed\n");
  return 0;
}
