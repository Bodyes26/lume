#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

class Preferences {
 public:
  static std::map<std::string, std::string>& store() {
    static std::map<std::string, std::string> s;
    return s;
  }

  static void clear() {
    store().clear();
  }

  bool begin(const char* ns, bool readOnly = false) {
    (void)ns;
    (void)readOnly;
    return true;
  }

  void end() {}

  bool isKey(const char* key) {
    return store().find(key) != store().end();
  }

  uint8_t getUChar(const char* key, uint8_t defaultValue = 0) {
    auto it = store().find(key);
    if (it == store().end()) return defaultValue;
    if (!it->second.empty()) return static_cast<uint8_t>(it->second[0]);
    return defaultValue;
  }

  size_t putUChar(const char* key, uint8_t value) {
    store()[key] = std::string(1, static_cast<char>(value));
    return 1;
  }

  size_t getString(const char* key, char* value, size_t maxLen) {
    auto it = store().find(key);
    if (it == store().end()) {
      if (maxLen > 0) value[0] = '\0';
      return 0;
    }
    strncpy(value, it->second.c_str(), maxLen);
    if (maxLen > 0) value[maxLen - 1] = '\0';
    return strlen(value);
  }

  size_t putString(const char* key, const char* value) {
    store()[key] = value ? value : "";
    return store()[key].size();
  }

  // Raw blobs, matching the ESP32 API: getBytes refuses to truncate (returns 0
  // when the stored blob is larger than the caller's buffer), which is what
  // GameStats relies on to reject a record blob of the wrong version/size.
  size_t getBytesLength(const char* key) {
    auto it = store().find(key);
    return (it == store().end()) ? 0 : it->second.size();
  }

  size_t getBytes(const char* key, void* buf, size_t maxLen) {
    auto it = store().find(key);
    if (it == store().end()) return 0;
    const size_t n = it->second.size();
    if (n > maxLen) return 0;
    memcpy(buf, it->second.data(), n);
    return n;
  }

  size_t putBytes(const char* key, const void* value, size_t len) {
    store()[key] = std::string(static_cast<const char*>(value), len);
    return len;
  }

  bool remove(const char* key) {
    store().erase(key);
    return true;
  }
};
