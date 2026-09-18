#!/bin/sh
set -e
cd "$(dirname "$0")"
SRC=../../../src
LIB=../../../lib
OUT=out
mkdir -p "$OUT"

CXXFLAGS="-std=c++17 -Wall -Wextra -Werror -I. -Istubs -I$SRC -I$LIB/EpdFontCore -Istubs/freertos -I../stubs/sleep_config"

c++ $CXXFLAGS render_main.cpp \
  "$SRC/Gfx.cpp" \
  "$SRC/Fonts.cpp" \
  "$SRC/Scene.cpp" \
  "$SRC/ClockStore.cpp" \
  "$SRC/games/Sudoku.cpp" \
  "$SRC/games/Nonogram.cpp" \
  "$SRC/games/Mines.cpp" \
  "$SRC/games/GameStats.cpp" \
  "$SRC/scenes/GamesScene.cpp" \
  "$SRC/scenes/SudokuScene.cpp" \
  "$SRC/scenes/NonogramScene.cpp" \
  "$SRC/scenes/MinesScene.cpp" \
  -o render_harness

./render_harness "$OUT"
