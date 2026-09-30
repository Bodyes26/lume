#pragma once

// Host render harness — X3 panel geometry with a plain RAM framebuffer.
//
// Native landscape 792x528, stride 99 bytes, 52.272 B total: the same numbers
// the device has, so Gfx's portrait rotation, the 8-px window alignment and
// every layout constant behave exactly as on glass. Flush calls only record
// what would have been pushed (the harness prints it), because there is no
// waveform to wait for.

#include <cstdint>
#include <cstring>

class EInkDisplay {
 public:
  enum RefreshMode { FULL_REFRESH, HALF_REFRESH, FAST_REFRESH };

  static constexpr uint16_t kNativeWidth = 792;   // long axis (logical height)
  static constexpr uint16_t kNativeHeight = 528;  // short axis (logical width)
  static constexpr uint16_t kStride = kNativeWidth / 8;
  static constexpr uint32_t kBufferSize = static_cast<uint32_t>(kStride) * kNativeHeight;

  uint16_t getDisplayWidth() const { return kNativeWidth; }
  uint16_t getDisplayHeight() const { return kNativeHeight; }
  uint16_t getDisplayWidthBytes() const { return kStride; }
  uint32_t getBufferSize() const { return kBufferSize; }

  uint8_t* getFrameBuffer() const { return const_cast<uint8_t*>(_fb); }
  void clearScreen(uint8_t color = 0xFF) const { std::memset(const_cast<uint8_t*>(_fb), color, kBufferSize); }

  void displayBuffer(RefreshMode mode = FAST_REFRESH, bool = false) {
    lastMode = mode;
    lastWindow[0] = lastWindow[1] = lastWindow[2] = lastWindow[3] = 0;
    flushes++;
  }
  void displayWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool = false) {
    lastWindow[0] = x; lastWindow[1] = y; lastWindow[2] = w; lastWindow[3] = h;
    flushes++;
  }
  void displayWindowFlash(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    displayWindow(x, y, w, h);
  }
  void requestResync(uint8_t = 0) {}
  bool releaseFramebufferForSync() { return true; }
  bool restoreFramebufferAfterSync() { return true; }

  RefreshMode lastMode = FAST_REFRESH;
  uint16_t lastWindow[4] = {0, 0, 0, 0};
  unsigned flushes = 0;

 private:
  uint8_t _fb[kBufferSize] = {};
};
