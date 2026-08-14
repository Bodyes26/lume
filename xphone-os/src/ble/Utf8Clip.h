#pragma once

// UTF-8-safe clipping shared by the two inbound text paths.
//
// Both paths land phone text in storage smaller than the phone is willing to
// send: ANCS attributes go into the fixed NotificationStore buffers
// (CompanionAncsClient.cpp parseAttributes), and companion card fields go into
// the CompanionProtocol MAX_*_CHARS caps (CompanionBleService.cpp
// applyCardPayload). Clipping on the raw byte count cuts multi-byte sequences
// in half — an Italian accented word or an emoji sitting on the limit leaves a
// dangling lead byte and renders as mojibake on glass. Everything here clips
// on a codepoint boundary instead, so a card title and an ANCS title behave
// identically. Header-only + inline: two translation units, no .cpp worth
// linking, and the byte-boundary walk stays a single implementation.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace utf8clip {

// Byte count to keep out of the first `limit` bytes of `data` so the result
// never ends inside a multi-byte sequence. When nothing is cut the input
// length is returned untouched; only the clipped tail is inspected.
inline std::size_t clipLength(const uint8_t* data, std::size_t length, std::size_t limit) {
  std::size_t count = std::min(length, limit);
  if (count == length || count == 0) return count;  // nothing cut, or nothing kept
  // Walk back over trailing continuation bytes (0b10xxxxxx) to the lead byte.
  std::size_t i = count;
  while (i > 0 && (data[i - 1] & 0xC0) == 0x80) --i;
  if (i == 0) return 0;  // window is nothing but continuation bytes
  const uint8_t lead = data[i - 1];
  std::size_t seqLen = 1;
  if ((lead & 0xE0) == 0xC0) seqLen = 2;
  else if ((lead & 0xF0) == 0xE0) seqLen = 3;
  else if ((lead & 0xF8) == 0xF0) seqLen = 4;
  if (seqLen > 1 && (count - (i - 1)) < seqLen) count = i - 1;  // drop the cut sequence
  return count;
}

}  // namespace utf8clip

// Raw BLE attribute bytes -> fixed NUL-terminated buffer (`dstSize` includes
// the NUL). Used by the ANCS attribute parser.
inline void clipUtf8(char* dst, std::size_t dstSize, const uint8_t* data, std::size_t length) {
  if (dstSize == 0) return;
  const std::size_t count = utf8clip::clipLength(data, length, dstSize - 1);
  std::memcpy(dst, data, count);
  dst[count] = '\0';
}

// ArduinoJson C string -> std::string capped at `maxBytes`. The card path gets
// `const char*` out of the JSON document, so the adapter lives here instead of
// a reinterpret_cast at every field assignment. nullptr yields an empty string
// (missing JSON keys resolve to nullptr in some ArduinoJson paths).
inline std::string clipUtf8(const char* value, std::size_t maxBytes) {
  if (!value) return {};
  std::string out(value);
  if (out.size() > maxBytes) {
    out.resize(utf8clip::clipLength(reinterpret_cast<const uint8_t*>(out.data()), out.size(), maxBytes));
  }
  return out;
}

// std::string -> fixed NUL-terminated buffer (`dstSize` includes the NUL).
// Used where an already-parsed card field is copied into a smaller store slot
// (e.g. BlockStatusStore's 24-byte preset vs the card's 96-byte cap), which
// would otherwise re-introduce the byte cut after the card path clipped
// cleanly.
inline void clipUtf8(char* dst, std::size_t dstSize, const char* value) {
  if (dstSize == 0) return;
  if (!value) { dst[0] = '\0'; return; }
  clipUtf8(dst, dstSize, reinterpret_cast<const uint8_t*>(value), std::strlen(value));
}
