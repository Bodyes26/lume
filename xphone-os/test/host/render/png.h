#pragma once

// Host render harness — minimal 8-bit grayscale PNG writer.
//
// Deliberately dependency-free (no zlib, no PIL): the deflate stream is written
// as STORED blocks, which is legal zlib and keeps the writer at fifty lines. A
// 528x792 frame lands at ~420 KB, which is irrelevant for a test artifact and
// buys us images any viewer (and the agent reading them back) can open.
//
// Host-only tooling: std::vector is fine here. Nothing in this file is ever
// compiled into the firmware.

#include <cstdint>
#include <cstdio>
#include <vector>

namespace host_png {

inline uint32_t crc32(const uint8_t* data, std::size_t n, uint32_t crc = 0xFFFFFFFFu) {
  static uint32_t table[256];
  static bool ready = false;
  if (!ready) {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      table[i] = c;
    }
    ready = true;
  }
  for (std::size_t i = 0; i < n; i++) crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
  return crc;
}

inline void push32(std::vector<uint8_t>& out, uint32_t v) {
  out.push_back(static_cast<uint8_t>(v >> 24));
  out.push_back(static_cast<uint8_t>(v >> 16));
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

inline void chunk(std::vector<uint8_t>& out, const char tag[5], const std::vector<uint8_t>& body) {
  push32(out, static_cast<uint32_t>(body.size()));
  const std::size_t start = out.size();
  for (int i = 0; i < 4; i++) out.push_back(static_cast<uint8_t>(tag[i]));
  out.insert(out.end(), body.begin(), body.end());
  const uint32_t crc = crc32(out.data() + start, out.size() - start) ^ 0xFFFFFFFFu;
  push32(out, crc);
}

// `gray` is w*h bytes, 0 = ink, 255 = paper.
inline bool write(const char* path, const uint8_t* gray, int w, int h) {
  std::vector<uint8_t> raw;
  raw.reserve(static_cast<std::size_t>(h) * (w + 1));
  for (int y = 0; y < h; y++) {
    raw.push_back(0);  // filter: none
    raw.insert(raw.end(), gray + static_cast<std::size_t>(y) * w, gray + static_cast<std::size_t>(y + 1) * w);
  }

  std::vector<uint8_t> z;
  z.push_back(0x78);  // zlib: deflate, 32K window
  z.push_back(0x01);  // no preset dict, fastest
  std::size_t off = 0;
  while (off < raw.size()) {
    const std::size_t len = (raw.size() - off > 65535u) ? 65535u : raw.size() - off;
    const bool final = (off + len == raw.size());
    z.push_back(final ? 1 : 0);
    z.push_back(static_cast<uint8_t>(len & 0xFF));
    z.push_back(static_cast<uint8_t>(len >> 8));
    z.push_back(static_cast<uint8_t>(~len & 0xFF));
    z.push_back(static_cast<uint8_t>((~len >> 8) & 0xFF));
    z.insert(z.end(), raw.begin() + static_cast<long>(off), raw.begin() + static_cast<long>(off + len));
    off += len;
  }
  uint32_t a = 1, b = 0;
  for (uint8_t byte : raw) {
    a = (a + byte) % 65521u;
    b = (b + a) % 65521u;
  }
  push32(z, (b << 16) | a);

  std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  std::vector<uint8_t> ihdr;
  push32(ihdr, static_cast<uint32_t>(w));
  push32(ihdr, static_cast<uint32_t>(h));
  ihdr.push_back(8);  // bit depth
  ihdr.push_back(0);  // grayscale
  ihdr.push_back(0);
  ihdr.push_back(0);
  ihdr.push_back(0);
  chunk(png, "IHDR", ihdr);
  chunk(png, "IDAT", z);
  chunk(png, "IEND", {});

  FILE* f = std::fopen(path, "wb");
  if (!f) return false;
  const bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
  std::fclose(f);
  return ok;
}

}  // namespace host_png
