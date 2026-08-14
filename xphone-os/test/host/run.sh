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
done
