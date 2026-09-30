#!/bin/sh
# Host tests for the pure logic in the firmware: civil-date/clock math
# (ClockStore.cpp) and DS3231 register handling (Ds3231.cpp) against a fake I2C
# register file. No board, no PlatformIO, no toolchain download — just a host
# C++17 compiler, so this runs before every flash.
#
#   sh test/host/run.sh
#
# Both firmware locales are compiled and run: the clock formats month and
# weekday names through L10N, so a translation mistake fails here.
set -e
cd "$(dirname "$0")"
SRC=../../src
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
CXXFLAGS="-std=c++17 -Wall -Wextra -Werror -I$SRC"

for locale in IT EN; do
  flag=""
  [ "$locale" = EN ] && flag="-DLUME_LOCALE_EN"

  c++ $CXXFLAGS $flag -Istubs/clock clock_test.cpp -o "$OUT/clock_$locale"
  printf '%s ' "$locale"
  "$OUT/clock_$locale"

  c++ $CXXFLAGS $flag -Istubs/rtc -I. ds3231_test.cpp "$SRC/ClockStore.cpp" -o "$OUT/rtc_$locale"
  printf '%s ' "$locale"
  "$OUT/rtc_$locale"
  c++ $CXXFLAGS $flag -Istubs/sleep_config sleep_config_test.cpp "$SRC/SleepConfigStore.cpp" -o "$OUT/sleep_config_$locale"
  printf '%s ' "$locale"
  "$OUT/sleep_config_$locale"

  c++ $CXXFLAGS $flag mines_test.cpp -o "$OUT/mines_$locale"
  printf '%s ' "$locale"
  "$OUT/mines_$locale"

  c++ $CXXFLAGS $flag sudoku_test.cpp -o "$OUT/sudoku_$locale"
  printf '%s ' "$locale"
  "$OUT/sudoku_$locale"

  c++ $CXXFLAGS $flag nonogram_test.cpp -o "$OUT/nonogram_$locale"
  printf '%s ' "$locale"
  "$OUT/nonogram_$locale"

  c++ $CXXFLAGS $flag -Istubs/sleep_config game_stats_test.cpp -o "$OUT/game_stats_$locale"
  printf '%s ' "$locale"
  "$OUT/game_stats_$locale"

  c++ $CXXFLAGS $flag trail_vm_test.cpp -o "$OUT/trail_vm_$locale"
  printf '%s ' "$locale"
  "$OUT/trail_vm_$locale"

  c++ $CXXFLAGS $flag -Istubs/sleep_config trail_engine_test.cpp -o "$OUT/trail_engine_$locale"
  printf '%s ' "$locale"
  "$OUT/trail_engine_$locale"

  c++ $CXXFLAGS $flag -Irender/stubs -I../../lib/EpdFontCore -I../../lib/Utf8 accent_test.cpp "$SRC/Gfx.cpp" "$SRC/Fonts.cpp" ../../lib/Utf8/Utf8.cpp -o "$OUT/accent_$locale"
  printf '%s ' "$locale"
  "$OUT/accent_$locale"
done
